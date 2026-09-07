import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def generate_telemetry_plots(csv_filename):
    # 1. Load the data using pandas
    try:
        df = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: Could not find '{csv_filename}'.")
        return

    # Clean column names just in case of trailing spaces
    df.columns = df.columns.str.strip()

    # Create a unified high-resolution timestamp (in seconds) relative to the start
    df['Time_Sec'] = df['Seconds'] + df['Nanoseconds'] / 1e9
    df['Relative_Time'] = df['Time_Sec'] - df['Time_Sec'].iloc[0]

    # Calculate Total Messages (Hz) as the sum of all quad tuple fields
    df['Total_Messages_Hz'] = df['Commit_Count'] + df['Identity_Count'] + df['Account_Count'] + df['Info_Count']

    # --- Jitter Calculation ---
    # The monitor thread is supposed to wake up exactly every 1.000000000 seconds
    # Jitter is the difference between actual elapsed time and ideal elapsed time
    df['Elapsed_Time'] = df['Time_Sec'].diff()
    df['Jitter_ms'] = (df['Elapsed_Time'] - 1.0) * 1000.0
    # Fill the first NaN row with 0 for clean plotting
    df.loc[df.index[0], 'Jitter_ms'] = 0.0

    # 2. Setup the Plotting Environment (3 subplots)
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 16))
    fig.suptitle('Real-Time Embedded System Telemetry (Bluesky Firehose)', fontsize=16, fontweight='bold')

    # --- Plot 1: Jitter Diagram ---
    # Shows the time difference (ms) of the periodic thread execution vs ideal 1s interval
    ax1.plot(df['Relative_Time'], df['Jitter_ms'], marker='o', linestyle='-', color='b', alpha=0.7)
    ax1.axhline(0, color='r', linestyle='--', linewidth=1)
    ax1.set_title('Periodic Thread Wake-up Jitter over Time')
    ax1.set_xlabel('Time (seconds)')
    ax1.set_ylabel('Jitter (milliseconds)')
    ax1.grid(True, linestyle=':', alpha=0.6)

    # --- Plot 2: Load & Buffer Diagram (Dual Axis) ---
    # Demonstrates the "burstiness" of the network and buffer occupancy
    color_msg = 'tab:blue'
    ax2.set_xlabel('Time (seconds)')
    ax2.set_ylabel('Incoming Messages (Hz)', color=color_msg)
    ax2.plot(df['Relative_Time'], df['Total_Messages_Hz'], color=color_msg, label='Message Rate', alpha=0.8)
    ax2.tick_params(axis='y', labelcolor=color_msg)
    ax2.grid(True, linestyle=':', alpha=0.6)

    # Instantiate a second axes that shares the same x-axis
    ax2_twin = ax2.twinx()  
    color_buf = 'tab:red'
    ax2_twin.set_ylabel('Buffer Occupancy (%)', color=color_buf)
    ax2_twin.plot(df['Relative_Time'], df['Buffer_Occupancy_Pct'], color=color_buf, linestyle='--', label='Buffer %', alpha=0.8)
    ax2_twin.tick_params(axis='y', labelcolor=color_buf)
    
    # Force the buffer y-axis to always show 0-100% context even if mostly empty
    ax2_twin.set_ylim(-5, 105) 
    ax2.set_title('Network Burstiness: Message Rate vs Buffer Occupancy')

    # --- Plot 3: CPU Usage vs Incoming Messages ---
    # Shows the correlation between message rate (Hz) and CPU workload
    ax3.scatter(df['Total_Messages_Hz'], df['CPU_Pct'], color='purple', alpha=0.7, edgecolors='k')
    
    # Add a trendline to make the correlation clearer
    if len(df) > 1:
        z = np.polyfit(df['Total_Messages_Hz'], df['CPU_Pct'], 1)
        p = np.poly1d(z)
        ax3.plot(df['Total_Messages_Hz'], p(df['Total_Messages_Hz']), "k--", alpha=0.8)

    ax3.set_title('CPU Utilization vs Incoming Message Rate')
    ax3.set_xlabel('Incoming Messages (Hz)')
    ax3.set_ylabel('CPU Usage (%)')
    ax3.grid(True, linestyle=':', alpha=0.6)

    # 3. Finalize and display
    plt.tight_layout()
    plt.subplots_adjust(top=0.95) # Make room for the main title
    
    # Save the figure to disk
    plt.savefig('telemetry_analysis.png', dpi=300)
    print("Plots saved successfully as 'telemetry_analysis.png'")
    
    # Display interactively
    plt.show()

if __name__ == "__main__":
    generate_telemetry_plots('metrics_log.txt')