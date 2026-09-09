import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def generate_telemetry_plots(csv_filename):
    try:
        df = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: Could not find '{csv_filename}'.")
        return

    df.columns = df.columns.str.strip()
    df['Time_Sec'] = df['Seconds'] + df['Nanoseconds'] / 1e9
    df['Relative_Time'] = df['Time_Sec'] - df['Time_Sec'].iloc[0]

    # --- Jitter Calculation ---
    df['Elapsed_Time'] = df['Time_Sec'].diff()
    df['Jitter_ms'] = (df['Elapsed_Time'] - 1.0) * 1000.0
    df.loc[df.index[0], 'Jitter_ms'] = 0.0

    # --- Identify Invalid/Disconnected States ---
    # The monitor outputs -1 in Commit_Count when the websocket is offline
    is_offline = df['Commit_Count'] == -1

    # Calculate Total Messages normally
    df['Total_Messages_Hz'] = df['Commit_Count'] + df['Identity_Count'] + df['Account_Count'] + df['Info_Count']
    
    # Replace -1 (and invalid totals) with NaN so matplotlib doesn't plot them
    cols_to_nan = ['Commit_Count', 'Identity_Count', 'Account_Count', 'Info_Count', 'Total_Messages_Hz']
    df.loc[is_offline, cols_to_nan] = np.nan

    # 2. Setup Plotting
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 16))
    fig.suptitle('Real-Time Embedded System Telemetry (Bluesky Firehose)', fontsize=16, fontweight='bold')

    # Helper function to shade disconnected regions
    def highlight_offline_regions(ax):
        offline_times = df[is_offline]['Relative_Time']
        for t in offline_times:
            ax.axvspan(t - 0.5, t + 0.5, color='red', alpha=0.3, lw=0)

    # --- Plot 1: Jitter Diagram ---
    ax1.plot(df['Relative_Time'], df['Jitter_ms'], marker='o', linestyle='-', color='b', alpha=0.7)
    ax1.axhline(0, color='red', linestyle='--', linewidth=1)
    ax1.set_title('Periodic Thread Wake-up Jitter over Time')
    ax1.set_xlabel('Time (seconds)')
    ax1.set_ylabel('Jitter (milliseconds)')
    ax1.grid(True, linestyle=':', alpha=0.6)
    highlight_offline_regions(ax1)

    # --- Plot 2: Load & Buffer Diagram ---
    color_msg = 'tab:blue'
    ax2.set_xlabel('Time (seconds)')
    ax2.set_ylabel('Incoming Messages (Hz)', color=color_msg)
    
    # Using marker='.' ensures we see valid data points even if they are surrounded by NaNs
    ax2.plot(df['Relative_Time'], df['Total_Messages_Hz'], color=color_msg, label='Message Rate', marker='.', alpha=0.8)
    ax2.tick_params(axis='y', labelcolor=color_msg)
    ax2.grid(True, linestyle=':', alpha=0.6)

    ax2_twin = ax2.twinx()  
    color_buf = 'tab:red'
    ax2_twin.set_ylabel('Buffer Occupancy (%)', color=color_buf)
    ax2_twin.plot(df['Relative_Time'], df['Buffer_Occupancy_Pct'], color=color_buf, linestyle='--', label='Buffer %', alpha=0.8)
    ax2_twin.tick_params(axis='y', labelcolor=color_buf)
    ax2_twin.set_ylim(-5, 105) 
    
    ax2.set_title('Network Burstiness (Red background indicates disconnected state)')
    highlight_offline_regions(ax2)

    # --- Plot 3: CPU Usage vs Incoming Messages ---
    # Drop rows where Total_Messages_Hz is NaN to calculate a correct trendline
    valid_data = df.dropna(subset=['Total_Messages_Hz'])
    ax3.scatter(valid_data['Total_Messages_Hz'], valid_data['CPU_Pct'], color='purple', alpha=0.7, edgecolors='k')
    
    if len(valid_data) > 1:
        z = np.polyfit(valid_data['Total_Messages_Hz'], valid_data['CPU_Pct'], 1)
        p = np.poly1d(z)
        ax3.plot(valid_data['Total_Messages_Hz'], p(valid_data['Total_Messages_Hz']), "k--", alpha=0.8)

    ax3.set_title('CPU Utilization vs Incoming Message Rate (Valid Data Only)')
    ax3.set_xlabel('Incoming Messages (Hz)')
    ax3.set_ylabel('CPU Usage (%)')
    ax3.grid(True, linestyle=':', alpha=0.6)

    plt.tight_layout()
    plt.subplots_adjust(top=0.95)
    plt.savefig('telemetry_analysis.png', dpi=300)
    plt.show()

if __name__ == "__main__":
    generate_telemetry_plots('metrics_log.txt')