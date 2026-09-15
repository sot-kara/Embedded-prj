import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import numpy as np

def generate_telemetry_plots(csv_filename):
    try:
        df = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Error: Could not find '{csv_filename}'.")
        return

    df.columns = df.columns.str.strip()
    
    # 1. Convert Unix timestamps into Pandas DateTime objects
    df['Time_Sec'] = df['Seconds'] + df['Nanoseconds'] / 1e9
    
    # Parse as UTC, then add 3 hours for GMT+3 (e.g., EEST)
    df['DateTime'] = pd.to_datetime(df['Time_Sec'], unit='s') + pd.Timedelta(hours=3)

    # --- Jitter Calculation ---
    df['Elapsed_Time'] = df['Time_Sec'].diff()
    df['Jitter_ms'] = (df['Elapsed_Time'] - 1.0) * 1000.0
    df.loc[df.index[0], 'Jitter_ms'] = 0.0

    # --- Identify Invalid/Disconnected States ---
    is_offline = df['Commit_Count'] == -1

    # Calculate Total Messages normally
    df['Total_Messages_Hz'] = df['Commit_Count'] + df['Identity_Count'] + df['Account_Count'] + df['Info_Count']
    
    # Replace -1 (and invalid totals) with NaN so matplotlib doesn't plot them
    cols_to_nan = ['Commit_Count', 'Identity_Count', 'Account_Count', 'Info_Count', 'Total_Messages_Hz']
    df.loc[is_offline, cols_to_nan] = np.nan

    # Helper function to shade disconnected regions using Timedeltas
    def highlight_offline_regions(ax):
        offline_times = df[is_offline]['DateTime']
        for t in offline_times:
            t_start = t - pd.Timedelta(seconds=0.5)
            t_end = t + pd.Timedelta(seconds=0.5)
            ax.axvspan(t_start, t_end, color='red', alpha=0.3, lw=0)

    # Date Locator (tick every 4 hours) and Formatter (Month-Day Hour:Minute)
    hour_locator = mdates.HourLocator(interval=4)
    date_formatter = mdates.DateFormatter('%m-%d %H:%M')

    # ==========================================
    # --- Plot 1: Jitter Diagram ---
    # ==========================================
    fig1, ax1 = plt.subplots(figsize=(12, 6))
    
    ax1.plot(df['DateTime'], df['Jitter_ms'], marker='o', linestyle='-', color='b', alpha=0.7)
    ax1.axhline(0, color='red', linestyle='--', linewidth=1)
    ax1.set_title('Periodic Thread Wake-up Jitter over Time', fontsize=14, fontweight='bold')
    ax1.set_xlabel('Date & Time (GMT+3)')
    ax1.set_ylabel('Jitter (milliseconds)')
    ax1.xaxis.set_major_locator(hour_locator)
    ax1.xaxis.set_major_formatter(date_formatter)
    ax1.grid(True, linestyle=':', alpha=0.6)
    highlight_offline_regions(ax1)
    
    fig1.autofmt_xdate(rotation=0, ha='center')
    fig1.tight_layout()
    fig1.savefig('telemetry_jitter.png', dpi=300)
    print("Saved 'telemetry_jitter.png'")

    # ==========================================
    # --- Plot 2: Load & Buffer Diagram ---
    # ==========================================
    fig2, ax2 = plt.subplots(figsize=(12, 6))
    color_msg = 'tab:blue'
    
    ax2.set_xlabel('Date & Time (GMT+3)')
    ax2.set_ylabel('Incoming Messages (Hz)', color=color_msg)
    ax2.plot(df['DateTime'], df['Total_Messages_Hz'], color=color_msg, label='Message Rate', marker='.', alpha=0.8)
    ax2.tick_params(axis='y', labelcolor=color_msg)
    ax2.xaxis.set_major_locator(hour_locator)
    ax2.xaxis.set_major_formatter(date_formatter)
    ax2.grid(True, linestyle=':', alpha=0.6)

    ax2_twin = ax2.twinx()  
    color_buf = 'tab:red'
    ax2_twin.set_ylabel('Buffer Occupancy (%)', color=color_buf)
    ax2_twin.plot(df['DateTime'], df['Buffer_Occupancy_Pct'], color=color_buf, linestyle='--', label='Buffer %', alpha=0.8)
    ax2_twin.tick_params(axis='y', labelcolor=color_buf)
    ax2_twin.set_ylim(-5, 105) 
    
    ax2.set_title('Network Burstiness (Red background indicates disconnected state)', fontsize=14, fontweight='bold')
    highlight_offline_regions(ax2)
    
    fig2.autofmt_xdate(rotation=0, ha='center')
    fig2.tight_layout()
    fig2.savefig('telemetry_burstiness.png', dpi=300)
    print("Saved 'telemetry_burstiness.png'")

    # ==========================================
    # --- Plot 3: CPU Usage vs Incoming Messages ---
    # ==========================================
    fig3, ax3 = plt.subplots(figsize=(12, 6))
    
    valid_data = df.dropna(subset=['Total_Messages_Hz'])
    ax3.scatter(valid_data['Total_Messages_Hz'], valid_data['CPU_Pct'], color='purple', alpha=0.7, edgecolors='k')
    
    if len(valid_data) > 1:
        z = np.polyfit(valid_data['Total_Messages_Hz'], valid_data['CPU_Pct'], 1)
        p = np.poly1d(z)
        ax3.plot(valid_data['Total_Messages_Hz'], p(valid_data['Total_Messages_Hz']), "k--", alpha=0.8)

    ax3.set_title('CPU Utilization vs Incoming Message Rate (Valid Data Only)', fontsize=14, fontweight='bold')
    ax3.set_xlabel('Incoming Messages (Hz)')
    ax3.set_ylabel('CPU Usage (%)')
    ax3.grid(True, linestyle=':', alpha=0.6)
    
    fig3.tight_layout()
    fig3.savefig('telemetry_cpu.png', dpi=300)
    print("Saved 'telemetry_cpu.png'")

    # Display all figures interactively
    plt.show()

if __name__ == "__main__":
    generate_telemetry_plots('metrics_log.txt')