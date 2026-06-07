#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl
import os
from pathlib import Path
import numpy as np

mpl.rcParams['font.sans-serif'] = ['SimHei', 'DejaVu Sans']
mpl.rcParams['axes.unicode_minus'] = False
plt.style.use('seaborn-v0_8-darkgrid')

figures_dir = Path(__file__).parent / 'data'
figures_dir.mkdir(exist_ok=True)


def safe_read_csv(filename):
    try:
        return pd.read_csv(filename)
    except Exception as e:
        return None


def plot_with_repeats(ax, data, x_col, y_col, run_col,
                      color, marker, label='',
                      logx=False, fill=True,
                      avg_linewidth=2.5, individual_alpha=0.3,
                      err_band_color=None):
    """
    Plot repeated-measures data: individual run traces (faint) +
    mean line (bold) + stddev shading.
    """
    if err_band_color is None:
        err_band_color = color

    # Individual runs as faint lines without markers
    for run_id in sorted(data[run_col].unique()):
        run_data = data[data[run_col] == run_id].sort_values(x_col)
        ax.plot(run_data[x_col], run_data[y_col],
                color=color, linewidth=0.8, alpha=individual_alpha,
                marker='', linestyle='-')

    # Average
    grp = data.groupby(x_col)
    x_vals = grp[y_col].mean().index.values.astype(float)
    y_mean = grp[y_col].mean().values
    y_std = grp[y_col].std().values

    ax.plot(x_vals, y_mean, marker=marker, color=color,
            linewidth=avg_linewidth, markersize=8, label=label)

    if fill:
        ax.fill_between(x_vals,
                         np.maximum(y_mean - y_std, 0),
                         y_mean + y_std,
                         alpha=0.15, color=err_band_color)

    if logx:
        ax.set_xscale('log')


# ============================================================================
# Plots
# ============================================================================

def plot_iops_comparison():
    print("Generating: IOPS Comparison...")
    lookup_data = safe_read_csv(str(figures_dir / 'lookup_iops_by_scale.csv'))
    mixed_data = safe_read_csv(str(figures_dir / 'mixed_workload.csv'))

    if lookup_data is None:
        return

    fig, ax = plt.subplots(figsize=(12, 6))

    plot_with_repeats(ax, lookup_data, 'Scale', 'Lookup_IOPS', 'Run',
                       color='#4ECDC4', marker='s', label='Lookup IOPS',
                       logx=True)

    if mixed_data is not None:
        ax.plot(mixed_data['Scale'], mixed_data['Mixed_IOPS'], marker='^',
                label='Mixed IOPS', linewidth=2, markersize=8, color='#95E1D3')

    ax.set_xlabel('Data Scale (records)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Throughput (IOPS)', fontsize=12, fontweight='bold')
    ax.set_title('B+ Tree Throughput Comparison\n(Lookup: mean ± 1σ, individual runs faint)',
                 fontsize=14, fontweight='bold')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'iops_comparison.png'), dpi=150)
    plt.close()


def plot_cache_efficiency():
    print("Generating: Cache Pressure Analysis...")
    cache_data = safe_read_csv(str(figures_dir / 'cache_pressure_by_pool_size.csv'))
    if cache_data is None:
        return

    fig, ax = plt.subplots(figsize=(10, 6))

    plot_with_repeats(ax, cache_data, 'PoolSize', 'Throughput_IOPS', 'Run',
                       color='#FF6B6B', marker='D', label='Throughput')

    ax.axvline(x=200, color='green', linestyle='--', linewidth=2, alpha=0.7,
               label='Working set (200 pages)')

    ax.set_xlabel('Buffer Pool Size (pages)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Throughput (IOPS)', fontsize=12, fontweight='bold')
    ax.set_title('Cache Pressure: Throughput vs Pool Size\n(mean ± 1σ, individual runs faint)',
                 fontsize=14, fontweight='bold')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'cache_efficiency.png'), dpi=150)
    plt.close()


def plot_waf_analysis():
    print("Generating: WAF Analysis...")
    waf_data = safe_read_csv(str(figures_dir / 'write_amplification.csv'))
    if waf_data is None:
        waf_data = safe_read_csv(str(figures_dir / 'waf_real.csv'))
        if waf_data is None:
            return

    fig, ax = plt.subplots(figsize=(11, 6))
    ax.bar(range(len(waf_data)), waf_data.iloc[:, 1], color='#4ECDC4',
           edgecolor='black', linewidth=1.5, alpha=0.8)
    ax.axhline(y=2, color='green', linestyle='--', linewidth=2, alpha=0.7, label='SSD (2x)')
    ax.axhline(y=5, color='orange', linestyle='--', linewidth=2, alpha=0.7, label='HDD (5x)')
    ax.set_xlabel('Update Data Size (bytes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Write Amplification (WAF)', fontsize=12, fontweight='bold')
    ax.set_title('Write Amplification Analysis', fontsize=14, fontweight='bold')
    ax.set_xticks(range(len(waf_data)))
    ax.set_xticklabels([str(x) for x in waf_data.iloc[:, 0]], rotation=45, ha='right')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'waf_analysis.png'), dpi=150)
    plt.close()


def plot_tree_height_evolution():
    print("Generating: Tree Height Evolution...")
    height_data = safe_read_csv(str(figures_dir / 'tree_height_evolution.csv'))
    if height_data is None:
        return

    fig, ax = plt.subplots(figsize=(11, 6))
    ax.plot(height_data['Scale'], height_data['Tree_Height'], marker='o',
            label='Measured', linewidth=2.5, markersize=8, color='#FF6B6B')
    theory_height = np.log(height_data['Scale']) / np.log(250) + 1
    ax.plot(height_data['Scale'], theory_height, marker='s',
            label='Theory log250(N)', linewidth=2.5, markersize=8,
            color='#4ECDC4', linestyle='--')
    ax.set_xlabel('Data Scale (records)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Tree Height (levels)', fontsize=12, fontweight='bold')
    ax.set_title('B+ Tree Height Growth Pattern', fontsize=14, fontweight='bold')
    ax.set_xscale('log')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'tree_height_evolution.png'), dpi=150)
    plt.close()


def plot_io_pattern_comparison():
    print("Generating: I/O Pattern Comparison...")
    io_data = safe_read_csv(str(figures_dir / 'sequential_vs_random.csv'))
    if io_data is None:
        return

    fig, ax = plt.subplots(figsize=(11, 6))

    seq_data = io_data[io_data['AccessPattern'] == 'seq']
    rand_data = io_data[io_data['AccessPattern'] == 'rand']

    plot_with_repeats(ax, seq_data, 'DataSize', 'IOPS', 'Run',
                       color='#FF6B6B', marker='s', label='Sequential',
                       logx=True)

    plot_with_repeats(ax, rand_data, 'DataSize', 'IOPS', 'Run',
                       color='#4ECDC4', marker='^', label='Random',
                       logx=True)

    ax.set_xlabel('Data Scale (records)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Throughput (IOPS)', fontsize=12, fontweight='bold')
    ax.set_title('Sequential vs Random Access Performance\n(mean ± 1σ, individual runs faint)',
                 fontsize=14, fontweight='bold')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'io_pattern_comparison.png'), dpi=150)
    plt.close()


def plot_working_set_pressure():
    print("Generating: Working Set Pressure Analysis...")
    ws_data = safe_read_csv(str(figures_dir / 'working_set_pressure.csv'))
    if ws_data is None:
        return

    fig, ax = plt.subplots(figsize=(11, 6))
    pool_size = ws_data['PoolSize'].iloc[0]

    ax.axvline(x=pool_size, color='red', linestyle='--', linewidth=2,
               alpha=0.7, label=f'Pool Size ({int(pool_size)} pages)')

    plot_with_repeats(ax, ws_data, 'WorksetSize', 'Throughput_IOPS', 'Run',
                       color='#FF6B6B', marker='D', label='Throughput',
                       logx=True)

    ax.set_xlabel('Working Set Size (pages)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Throughput (IOPS)', fontsize=12, fontweight='bold')
    ax.set_title(f'Working Set Pressure: Pool={int(pool_size)} pages\n'
                 f'(mean ± 1σ, individual runs faint)',
                 fontsize=14, fontweight='bold')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'working_set_pressure.png'), dpi=150)
    plt.close()


def plot_summary_dashboard():
    print("Generating: Summary Dashboard...")
    fig = plt.figure(figsize=(16, 10))

    lookup_data = safe_read_csv(str(figures_dir / 'lookup_iops_by_scale.csv'))
    cache_data = safe_read_csv(str(figures_dir / 'cache_pressure_by_pool_size.csv'))
    height_data = safe_read_csv(str(figures_dir / 'tree_height_evolution.csv'))
    ws_data = safe_read_csv(str(figures_dir / 'working_set_pressure.csv'))

    if lookup_data is not None:
        ax1 = plt.subplot(2, 2, 1)
        plot_with_repeats(ax1, lookup_data, 'Scale', 'Lookup_IOPS', 'Run',
                           color='#4ECDC4', marker='s', logx=True)
        ax1.set_xlabel('Data Scale', fontsize=10)
        ax1.set_ylabel('IOPS', fontsize=10)
        ax1.set_title('Lookup Throughput', fontsize=11, fontweight='bold')
        ax1.grid(True, alpha=0.3)

    if cache_data is not None:
        ax2 = plt.subplot(2, 2, 2)
        plot_with_repeats(ax2, cache_data, 'PoolSize', 'Throughput_IOPS', 'Run',
                           color='#FF6B6B', marker='D')
        ax2.set_xlabel('Buffer Pool Size', fontsize=10)
        ax2.set_ylabel('Throughput (IOPS)', fontsize=10)
        ax2.set_title('Cache Pressure', fontsize=11, fontweight='bold')
        ax2.grid(True, alpha=0.3)

    if height_data is not None:
        ax3 = plt.subplot(2, 2, 3)
        ax3.plot(height_data['Scale'], height_data['Tree_Height'],
                 marker='o', color='#FF6B6B', linewidth=2)
        ax3.set_xlabel('Data Scale', fontsize=10)
        ax3.set_ylabel('Tree Height', fontsize=10)
        ax3.set_title('Tree Height Evolution', fontsize=11, fontweight='bold')
        ax3.set_xscale('log')
        ax3.grid(True, alpha=0.3)

    if ws_data is not None:
        ax4 = plt.subplot(2, 2, 4)
        pool = ws_data['PoolSize'].iloc[0]
        plot_with_repeats(ax4, ws_data, 'WorksetSize', 'Throughput_IOPS', 'Run',
                           color='#4ECDC4', marker='D', logx=True)
        ax4.axvline(x=pool, color='red', linestyle='--', alpha=0.5,
                    label=f'Pool={int(pool)}')
        ax4.set_xlabel('Working Set Size', fontsize=10)
        ax4.set_ylabel('Throughput (IOPS)', fontsize=10)
        ax4.set_title('Working Set Pressure', fontsize=11, fontweight='bold')
        ax4.legend(fontsize=8)
        ax4.grid(True, alpha=0.3)

    plt.suptitle('LTM B+ Tree Performance Dashboard', fontsize=16, fontweight='bold', y=0.995)
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'dashboard_summary.png'), dpi=150)
    plt.close()


def main():
    print("\n" + "="*60)
    print("   B+ Tree LTM Performance Visualization")
    print("="*60)
    print(f"Data directory: {figures_dir}\n")

    plot_iops_comparison()
    plot_cache_efficiency()
    plot_waf_analysis()
    plot_tree_height_evolution()
    plot_io_pattern_comparison()
    plot_working_set_pressure()
    plot_summary_dashboard()

    print("\n" + "="*60)
    print("   OK: All charts generated successfully!")
    print(f"   Charts location: {figures_dir}")
    print("="*60 + "\n")


if __name__ == '__main__':
    main()
