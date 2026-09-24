#!/usr/bin/env python3
"""
jetstream_test_server.py

A fake "Jetstream"-style WebSocket server for testing the C websocket
client's reconnect / idle-watchdog behavior.

WHAT IT DOES
------------
It serves a WSS endpoint that sends fake messages shaped like:

    {"kind": "commit",   ...filler...}
    {"kind": "identity", ...filler...}
    {"kind": "account",  ...filler...}
    {"kind": "info",     ...filler...}

matching the MSG_KIND_* values your consumer.c switches on, at a
steady rate -- and then, on a repeating cycle, goes completely SILENT
for a configurable duration WITHOUT closing the TCP connection. This
reproduces the "zombie connection" symptom you saw in production
(bytes stop flowing but the socket looks fine), then dumps a burst of
queued messages once the stall ends, then resumes normal traffic.

If your fix (last_rx_time watchdog in ws_client_run) is working, you
should see, in your client's logs / metrics_log.txt:
  - Buffer_Occupancy_Pct and commit/identity/account/info counts drop
    to 0 during the stall (expected, no data is arriving)
  - A "No data received in N seconds..." reconnect message from the
    client at roughly stall_after + WS_IDLE_TIMEOUT_SEC seconds into
    the stall (NOT waiting the full stall_duration)
  - A fresh CLIENT_ESTABLISHED once your client reconnects
  - Normal counts resuming

If the fix is NOT working, the client will just sit there silently
for the entire stall_duration with connected=1 the whole time, and
you'll see the delayed burst-catch-up pattern from before.

USAGE
-----
    python3 jetstream_test_server.py
    python3 jetstream_test_server.py --port 8443 --stall-after 30 --stall-duration 25

Then point your C client at it. The quickest way (no rebuild needed
beyond changing the connect target) is to temporarily edit the
ws_client_create(...) call in producer.c:

    ws_client_ctx_t *ws_client = ws_client_create(
        "YOUR_PI_OR_TEST_MACHINE_IP",   // was "jetstream1.us-east.bsky.network"
        8443,                            // was 443
        "/subscribe?wantedCollections=app.bsky.feed.post",
        on_websocket_data,
        prod,
        prod->is_connected
    );

Your client already sets LCCSCF_ALLOW_SELFSIGNED and
LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK, so the self-signed cert this
script generates will be accepted without any client-side changes.

REQUIREMENTS
------------
    pip install websockets
    openssl must be available on PATH (used once to generate a
    self-signed cert/key pair if they don't already exist).
"""

import argparse
import asyncio
import json
import os
import random
import ssl
import subprocess
import sys
import time
from datetime import datetime

try:
    import websockets
except ImportError:
    print("Missing dependency. Install it with:\n    pip install websockets", file=sys.stderr)
    sys.exit(1)


def log(msg):
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{ts}] {msg}", flush=True)


def ensure_self_signed_cert(certfile, keyfile):
    """Generate a self-signed cert/key pair with openssl if missing."""
    if os.path.exists(certfile) and os.path.exists(keyfile):
        return
    log(f"Generating self-signed cert ({certfile}, {keyfile})...")
    try:
        subprocess.run(
            [
                "openssl", "req", "-x509", "-newkey", "rsa:2048",
                "-keyout", keyfile, "-out", certfile,
                "-days", "365", "-nodes",
                "-subj", "/CN=localhost",
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(
            "Failed to auto-generate a self-signed cert with openssl.\n"
            "Generate one manually and re-run, e.g.:\n\n"
            "    openssl req -x509 -newkey rsa:2048 -keyout key.pem "
            "-out cert.pem -days 365 -nodes -subj /CN=localhost\n",
            file=sys.stderr,
        )
        raise SystemExit(1) from e
    log("Cert generated.")


def make_fake_message(kind, seq, filler_size):
    """Build a fake Jetstream-shaped message for the given kind."""
    payload = {
        "did": f"did:plc:testuser{seq % 500:05d}",
        "time_us": int(time.time() * 1_000_000),
        "kind": kind,
        "seq": seq,
        "filler": "x" * filler_size,
    }
    return json.dumps(payload)


KINDS = ["commit", "commit", "commit", "identity", "account", "info"]
# weighted so "commit" dominates, similar to real Jetstream traffic


async def send_normal_traffic(ws, seq_ref, rate_per_sec, filler_size, stop_event, duration=None):
    """Send messages at roughly rate_per_sec until stop_event is set or duration elapses."""
    interval = 1.0 / rate_per_sec if rate_per_sec > 0 else 1.0
    start = time.monotonic()
    while not stop_event.is_set():
        if duration is not None and (time.monotonic() - start) >= duration:
            return
        kind = random.choice(KINDS)
        msg = make_fake_message(kind, seq_ref[0], filler_size)
        seq_ref[0] += 1
        await ws.send(msg)
        await asyncio.sleep(interval)


async def handle_client(ws, args):
    peer = getattr(ws, "remote_address", "?")
    log(f"Client connected: {peer}")
    seq_ref = [0]
    stop_event = asyncio.Event()

    async def reader():
        # Drain anything the client sends (it shouldn't send much, but
        # keep the connection healthy and detect client-initiated close).
        try:
            async for _ in ws:
                pass
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            stop_event.set()

    reader_task = asyncio.create_task(reader())

    try:
        cycle = 0
        while not stop_event.is_set():
            cycle += 1

            # --- Phase 1: normal traffic ---
            log(f"[cycle {cycle}] Sending normal traffic for {args.stall_after}s "
                f"(~{args.rate} msg/s)")
            await send_normal_traffic(
                ws, seq_ref, args.rate, args.filler_size, stop_event,
                duration=args.stall_after,
            )
            if stop_event.is_set():
                break

            if not args.no_stall:
                # --- Phase 2: silent stall (TCP stays open, no bytes sent) ---
                log(f"[cycle {cycle}] *** STALL START *** going silent for "
                    f"{args.stall_duration}s (connection stays open, no data sent). "
                    f"Watch your client -- it should force-reconnect at roughly "
                    f"the idle-timeout mark, well before this stall ends.")
                stall_start = time.monotonic()
                while (time.monotonic() - stall_start) < args.stall_duration:
                    if stop_event.is_set():
                        break
                    # Do not send anything. Just sleep. TCP connection is
                    # left fully intact -- this is the "zombie" scenario.
                    await asyncio.sleep(0.5)
                log(f"[cycle {cycle}] *** STALL END *** "
                    f"(client should have reconnected by now if the fix works; "
                    f"if it didn't, you're about to see the delayed burst)")

                if stop_event.is_set():
                    break

                # --- Phase 3: burst of queued messages ---
                log(f"[cycle {cycle}] Dumping burst of {args.burst_size} messages "
                    f"as fast as possible (simulates delayed backlog flush)")
                for _ in range(args.burst_size):
                    if stop_event.is_set():
                        break
                    kind = random.choice(KINDS)
                    msg = make_fake_message(kind, seq_ref[0], args.filler_size)
                    seq_ref[0] += 1
                    try:
                        await ws.send(msg)
                    except websockets.exceptions.ConnectionClosed:
                        break

            if args.once:
                log("Single-cycle mode (--once): closing server-side send loop, "
                    "leaving connection open for client to manage.")
                break

    except websockets.exceptions.ConnectionClosed:
        log(f"Client disconnected: {peer}")
    finally:
        reader_task.cancel()
        try:
            await reader_task
        except (asyncio.CancelledError, Exception):
            pass
        log(f"Session ended for {peer} (sent {seq_ref[0]} messages)")


async def main():
    parser = argparse.ArgumentParser(
        description="Fake Jetstream WSS server for testing reconnect/idle-watchdog behavior."
    )
    parser.add_argument("--host", default="0.0.0.0", help="Bind address (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8443, help="Bind port (default: 8443)")
    parser.add_argument("--cert", default="test_cert.pem", help="TLS cert file (auto-generated if missing)")
    parser.add_argument("--key", default="test_key.pem", help="TLS key file (auto-generated if missing)")
    parser.add_argument("--rate", type=float, default=20.0, help="Messages/sec during normal traffic (default: 20)")
    parser.add_argument("--filler-size", type=int, default=200, help="Bytes of filler text per message (default: 200)")
    parser.add_argument("--stall-after", type=float, default=30.0,
                         help="Seconds of normal traffic before each stall (default: 30)")
    parser.add_argument("--stall-duration", type=float, default=25.0,
                         help="Seconds of silence per stall, connection stays open (default: 25). "
                              "Should be longer than your client's WS_IDLE_TIMEOUT_SEC (15s) "
                              "so you can see whether it reconnects DURING the stall.")
    parser.add_argument("--burst-size", type=int, default=300,
                         help="Messages to dump immediately after a stall ends (default: 300)")
    parser.add_argument("--no-stall", action="store_true",
                         help="Disable stall injection entirely; just send normal traffic "
                              "(baseline / sanity check run)")
    parser.add_argument("--once", action="store_true",
                         help="Only run one normal->stall->burst cycle per connection instead of looping")
    args = parser.parse_args()

    ensure_self_signed_cert(args.cert, args.key)

    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_context.load_cert_chain(args.cert, args.key)

    log(f"Starting test server on wss://{args.host}:{args.port}")
    log(f"Cycle plan: {args.rate} msg/s for {args.stall_after}s, then "
        f"{'NO STALL' if args.no_stall else f'{args.stall_duration}s silent stall'}, "
        f"then burst of {args.burst_size} msgs, repeat.")
    log("Waiting for client connection... (Ctrl+C to stop)")

    async def handler(ws):
        await handle_client(ws, args)

    async with websockets.serve(handler, args.host, args.port, ssl=ssl_context):
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print()
        log("Shutting down.")