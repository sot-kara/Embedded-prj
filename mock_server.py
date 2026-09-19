import asyncio
import json
import random
import string
import time
import websockets

def generate_static_payload(target_size_bytes=3900):
    """Generate the payload ONCE to save CPU."""
    data = {
        "id": 999999,
        "timestamp": time.time(),
        "kind": "commit",
        "author": "speed_tester",
        "text": ""
    }
    base_json_str = json.dumps(data)
    padding_needed = target_size_bytes - len(base_json_str.encode('utf-8'))
    
    if padding_needed > 0:
        data["text"] = ''.join(random.choices(string.ascii_letters + string.digits, k=padding_needed))
        
    return json.dumps(data)

# Pre-calculate the heavy JSON string
STATIC_PAYLOAD = generate_static_payload(3900)

# Pre-calculate a fragmented version of the same payload.
# We split the ~3900 byte string into 4 smaller chunks.
chunk_size = len(STATIC_PAYLOAD) // 4
FRAGMENTED_PAYLOAD = [
    STATIC_PAYLOAD[i:i + chunk_size] 
    for i in range(0, len(STATIC_PAYLOAD), chunk_size)
]

async def jetstream_handler(websocket):
    print(f"✅ Client connected from {websocket.remote_address}")
    
    message_count = 0
    fragmented_count = 0
    start_time = time.time()
    
    try:
        while True:
            # BATCH SEND: Send 100 messages as fast as the network allows
            for _ in range(100):
                
                # 10% chance to send the message as fragmented frames
                if random.random() < 0.10:
                    # Passing a list to send() forces the Python websockets 
                    # library to send it as WebSocket continuation frames.
                    await websocket.send(FRAGMENTED_PAYLOAD)
                    fragmented_count += 1
                else:
                    # Send normally as a single frame
                    await websocket.send(STATIC_PAYLOAD)
                
                message_count += 1
            
            # Print stats every 2000 messages (roughly every 1 second)
            if message_count % 2000 == 0:
                elapsed = time.time() - start_time
                msgs_per_sec = message_count / elapsed
                print(f"Sent {message_count} total ({fragmented_count} fragmented) | Speed: {msgs_per_sec:.2f} msg/sec")
            
            # Sleep for 50ms (0.05s) to maintain ~2000 msgs/sec
            await asyncio.sleep(0)
            
    except websockets.exceptions.ConnectionClosed as e:
        print(f"❌ Client disconnected: {e}")

async def main():
    print("Starting FRAGMENTATION STRESS-TEST Server on ws://localhost:8080...")
    async with websockets.serve(jetstream_handler, "localhost", 8080, subprotocols=["jetstream-protocol"]):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
