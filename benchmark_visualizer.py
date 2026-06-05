#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl
import os
import sys
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

def plot_iops_comparison():
    print("Generating: IOPS Comparison...")
    insert_data = safe_read_csv(str(figures_dir / 'tree_iops_real.csv'))
    lookup_data = safe_read_csv(str(figures_dir / 'lookup_iops_by_scale.csv'))
    mixed_data = safe_read_csv(str(figures_dir / 'mixed_workload.csv'))
    
    if not all([insert_data is not None, lookup_data is not None, mixed_data is not None]):
        return
    
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.plot(insert_data['Scale'], insert_data['IOPS'], marker='o', label='Insert IOPS', linewidth=2, markersize=8, color='#FF6B6B')
    ax.plot(lookup_data['Scale'], lookup_data['Lookup_IOPS'], marker='s', label='Lookup IOPS', linewidth=2, markersize=8, color='#4ECDC4')
    ax.plot(mixed_data['Scale'], mixed_data['Mixed_IOPS'], marker='^', label='Mixed IOPS', linewidth=2, markersize=8, color='#95E1D3')
    ax.set_xlabel('Data Scale (records)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Throughput (IOPS)', fontsize=12, fontweight='bold')
    ax.set_title('B+ Tree IOPS Comparison', fontsize=14, fontweight='bold')
    ax.set_xscale('log')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'iops_comparison.png'), dpi=150)
    plt.close()

def plot_cache_efficiency():
    print("Generating: Cache Efficiency...")
    cache_data = safe_read_csv(str(figures_dir / 'cache_hitrate_by_pool_size.csv'))
    if cache_data is None:
        return
    
    fig, ax1 = plt.subplots(figsize=(10, 6))
    color1 = '#FF6B6B'
    ax1.plot(cache_data['PoolSize'], cache_data['HitRate_Percent'], marker='D', color=color1, linewidth=2.5, markersize=9, label='Hit Rate')
    ax1.fill_between(cache_data['PoolSize'], cache_data['HitRate_Percent'], alpha=0.2, color=color1)
    ax1.set_xlabel('Buffer Pool Size (pages)', fontsize=12, fontweight='bold')
    ax1.set_ylabel('Hit Rate (%)', fontsize=12, fontweight='bold', color=color1)
    ax1.tick_params(axis='y', labelcolor=color1)
    
    ax2 = ax1.twinx()
    color2 = '#4ECDC4'
    ax2.plot(cache_data['PoolSize'], 100 - cache_data['HitRate_Percent'], marker='o', color=color2, linewidth=2.5, markersize=9, label='Eviction Rate', linestyle='--')
    ax2.set_ylabel('Eviction Rate (%)', fontsize=12, fontweight='bold', color=color2)
    ax2.tick_params(axis='y', labelcolor=color2)
    
    ax1.set_title('Cache Efficiency Analysis', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc='lower right', fontsize=10)
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'cache_efficiency.png'), dpi=150)
    plt.close()

def plot_waf_analysis():
    print("Generating: WAF Analysis...")
    waf_data = safe_read_csv(str(figures_dir / 'waf_real.csv'))
    if waf_data is None:
        return
    
    fig, ax = plt.subplots(figsize=(11, 6))
    ax.bar(range(len(waf_data)), waf_data.iloc[:, 1], color='#4ECDC4', edgecolor='black', linewidth=1.5, alpha=0.8)
    ax.axhline(y=2, color='green', linestyle='--', linewidth=2, alpha=0.7, label='SSD (2x)')
    ax.axhline(y=5, color='orange', linestyle='--', linewidth=2, alpha=0.7, label='HDD (5x)')
    ax.set_xlabel('Update Data Size (bytes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Write Amplification (WAF)', fontsize=12, fontweight='bold')
    ax.set_title('Write Amplification Analysis', fontsize=14, fontweight='bold')
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
    ax.plot(height_data['Scale'], height_data['Tree_Height'], marker='o', label='Measured', linewidth=2.5, markersize=8, color='#FF6B6B')
    theory_height = np.log(height_data['Scale']) / np.log(250) + 1
    ax.plot(height_data['Scale'], theory_height, marker='s', label='Theory log250(N)', linewidth=2.5, markersize=8, color='#4ECDC4', linestyle='--')
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
    x = range(len(io_data))
    width = 0.35
    ax.bar([i - width/2 for i in x], io_data['Sequential_IOPS'], width, label='Sequential', color='#FF6B6B', edgecolor='black', linewidth=1.5)
    ax.bar([i + width/2 for i in x], io_data['Random_IOPS'], width, label='Random', color='#4ECDC4', edgecolor='black', linewidth=1.5)
    ax.set_xlabel('Data Scale (records)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Throughput (IOPS)', fontsize=12, fontweight='bold')
    ax.set_title('Sequential vs Random Access Performance', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels([f'{int(s):,}' for s in io_data['DataSize']], rotation=45, ha='right')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'io_pattern_comparison.png'), dpi=150)
    plt.close()

def plot_dirty_pages_timeline():
    print("Generating: Dirty Pages Analysis...")
    dirty_data = safe_read_csv(str(figures_dir / 'dirty_pages_analysis.csv'))
    if dirty_data is None:
        return
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
    ax1.plot(dirty_data['UpdateFrequency'], dirty_data['DirtyPageCount'], marker='D', linewidth=2.5, markersize=9, color='#FF6B6B')
    ax1.fill_between(dirty_data['UpdateFrequency'], dirty_data['DirtyPageCount'], alpha=0.2, color='#FF6B6B')
    ax1.set_xlabel('Update Frequency', fontsize=11, fontweight='bold')
    ax1.set_ylabel('Dirty Page Count', fontsize=11, fontweight='bold')
    ax1.set_title('Garbage Data Growth', fontsize=12, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    
    ax2.plot(dirty_data['UpdateFrequency'], dirty_data['CleanTime_ms'], marker='s', linewidth=2.5, markersize=9, color='#4ECDC4')
    ax2.fill_between(dirty_data['UpdateFrequency'], dirty_data['CleanTime_ms'], alpha=0.2, color='#4ECDC4')
    ax2.set_xlabel('Update Frequency', fontsize=11, fontweight='bold')
    ax2.set_ylabel('Clean Time (ms)', fontsize=11, fontweight='bold')
    ax2.set_title('Cleanup Cost', fontsize=12, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    
    plt.suptitle('Dirty Page (Garbage Data) Lifecycle', fontsize=14, fontweight='bold', y=1.00)
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'dirty_pages_timeline.png'), dpi=150)
    plt.close()

def plot_lru_eviction_analysis():
    print("Generating: LRU Eviction Analysis...")
    evict_data = safe_read_csv(str(figures_dir / 'lru_eviction_analysis.csv'))
    if evict_data is None:
        return
    
    fig, ax1 = plt.subplots(figsize=(11, 6))
    color1 = '#FF6B6B'
    ax1.plot(evict_data['WorksetSize'], evict_data['EvictionCount'], marker='o', color=color1, linewidth=2.5, markersize=9, label='Eviction Count')
    ax1.set_xlabel('Workset Size (pages)', fontsize=12, fontweight='bold')
    ax1.set_ylabel('Eviction Count', fontsize=12, fontweight='bold', color=color1)
    ax1.tick_params(axis='y', labelcolor=color1)
    ax1.fill_between(evict_data['WorksetSize'], evict_data['EvictionCount'], alpha=0.2, color=color1)
    
    ax2 = ax1.twinx()
    color2 = '#4ECDC4'
    ax2.plot(evict_data['WorksetSize'], evict_data['InvalidPages'], marker='s', color=color2, linewidth=2.5, markersize=9, label='Invalid Pages', linestyle='--')
    ax2.set_ylabel('Invalid Cache Pages', fontsize=12, fontweight='bold', color=color2)
    ax2.tick_params(axis='y', labelcolor=color2)
    
    ax1.set_title('LRU Eviction Effect (Garbage Data Generation)', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc='upper left', fontsize=10)
    plt.tight_layout()
    plt.savefig(str(figures_dir / 'lru_eviction_analysis.png'), dpi=150)
    plt.close()

def plot_summary_dashboard():
    print("Generating: Summary Dashboard...")
    fig = plt.figure(figsize=(16, 10))
    
    insert_data = safe_read_csv(str(figures_dir / 'tree_iops_real.csv'))
    lookup_data = safe_read_csv(str(figures_dir / 'lookup_iops_by_scale.csv'))
    cache_data = safe_read_csv(str(figures_dir / 'cache_hitrate_by_pool_size.csv'))
    height_data = safe_read_csv(str(figures_dir / 'tree_height_evolution.csv'))
    
    if insert_data is not None and lookup_data is not None:
        ax1 = plt.subplot(2, 2, 1)
        ax1.plot(insert_data['Scale'], insert_data['IOPS'], marker='o', label='Insert', color='#FF6B6B')
        ax1.plot(lookup_data['Scale'], lookup_data['Lookup_IOPS'], marker='s', label='Lookup', color='#4ECDC4')
        ax1.set_xlabel('Data Scale', fontsize=10)
        ax1.set_ylabel('IOPS', fontsize=10)
        ax1.set_title('IOPS Comparison', fontsize=11, fontweight='bold')
        ax1.set_xscale('log')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
    
    if cache_data is not None:
        ax2 = plt.subplot(2, 2, 2)
        ax2.plot(cache_data['PoolSize'], cache_data['HitRate_Percent'], marker='D', color='#FF6B6B', linewidth=2)
        ax2.fill_between(cache_data['PoolSize'], cache_data['HitRate_Percent'], alpha=0.2, color='#FF6B6B')
        ax2.set_xlabel('Buffer Pool Size', fontsize=10)
        ax2.set_ylabel('Hit Rate (%)', fontsize=10)
        ax2.set_title('Cache Efficiency', fontsize=11, fontweight='bold')
        ax2.grid(True, alpha=0.3)
    
    if height_data is not None:
        ax3 = plt.subplot(2, 2, 3)
        ax3.plot(height_data['Scale'], height_data['Tree_Height'], marker='o', color='#FF6B6B', linewidth=2)
        ax3.set_xlabel('Data Scale', fontsize=10)
        ax3.set_ylabel('Tree Height', fontsize=10)
        ax3.set_title('Tree Height Evolution', fontsize=11, fontweight='bold')
        ax3.set_xscale('log')
        ax3.grid(True, alpha=0.3)
    
    waf_data = safe_read_csv(str(figures_dir / 'waf_real.csv'))
    if waf_data is not None:
        ax4 = plt.subplot(2, 2, 4)
        ax4.bar(range(len(waf_data)), waf_data.iloc[:, 1], color='#4ECDC4', edgecolor='black', alpha=0.8)
        ax4.set_xlabel('Update Size', fontsize=10)
        ax4.set_ylabel('WAF', fontsize=10)
        ax4.set_title('Write Amplification', fontsize=11, fontweight='bold')
        ax4.set_xticklabels([str(x) for x in waf_data.iloc[:, 0]], rotation=45, ha='right')
        ax4.grid(True, alpha=0.3, axis='y')
    
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
    plot_dirty_pages_timeline()
    plot_lru_eviction_analysis()
    plot_summary_dashboard()
    
    print("\n" + "="*60)
    print("   OK: All charts generated successfully!")
    print(f"   Charts location: {figures_dir}")
    print("="*60 + "\n")

if __name__ == '__main__':
    main()
