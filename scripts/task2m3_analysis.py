"""
Task 2 Milestone 3: Analysis script.
Produces 12 bar plots:
  For each dataset (FB, Books, OSMC):
    - Mixed 10% Insert - Throughput
    - Mixed 10% Insert - Index Size
    - Mixed 90% Insert - Throughput
    - Mixed 90% Insert - Index Size
"""
import os
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


DATASETS = {
    "fb": "fb_100M_public_uint64",
    "books": "books_100M_public_uint64",
    "osmc": "osmc_100M_public_uint64",
}
INDEXES = ["DynamicPGM", "LIPP", "Hybrid"]
# Hybrid family: pool all variant names and pick the best per workload
HYBRID_NAMES = {"Hybrid", "HybridLookup", "HybridLookupPrefix", "HybridInsert"}
WORKLOADS = {
    "mix10": "0.100000i_0m_mix",
    "mix90": "0.900000i_0m_mix",
}


def load_results():
    results = {}
    for ds_label, ds_name in DATASETS.items():
        results[ds_label] = {}
        for wl_label, wl_suffix in WORKLOADS.items():
            fname = f"results/{ds_name}_ops_2M_0.000000rq_0.500000nl_{wl_suffix}_results_table.csv"
            if not os.path.exists(fname):
                print(f"WARNING: {fname} not found, skipping")
                continue
            df = pd.read_csv(fname)
            throughputs = {}
            sizes = {}
            for idx in INDEXES:
                if idx == "Hybrid":
                    rows = df[df["index_name"].isin(HYBRID_NAMES)]
                else:
                    rows = df[df["index_name"] == idx]
                if rows.empty:
                    throughputs[idx] = 0
                    sizes[idx] = 0
                    continue
                # Average throughput across 3 runs, pick best config
                avg = rows[["mixed_throughput_mops1", "mixed_throughput_mops2",
                             "mixed_throughput_mops3"]].mean(axis=1)
                best_row = avg.idxmax()
                throughputs[idx] = avg[best_row]
                sizes[idx] = rows.loc[best_row, "index_size_bytes"]
            results[ds_label][wl_label] = {"throughput": throughputs, "size": sizes}
    return results


def plot_results(results):
    os.makedirs("task2m3_results", exist_ok=True)

    fig, axs = plt.subplots(3, 4, figsize=(20, 12))
    colors = ["#4C72B0", "#DD8452", "#55A868"]

    for row_idx, (ds_label, ds_name) in enumerate(DATASETS.items()):
        configs = [
            (0, "mix10", "throughput",
             f"{ds_label.upper()} - 10% Insert - Throughput", "Throughput (Mops/s)"),
            (1, "mix10", "size",
             f"{ds_label.upper()} - 10% Insert - Index Size", "Index Size (bytes)"),
            (2, "mix90", "throughput",
             f"{ds_label.upper()} - 90% Insert - Throughput", "Throughput (Mops/s)"),
            (3, "mix90", "size",
             f"{ds_label.upper()} - 90% Insert - Index Size", "Index Size (bytes)"),
        ]

        for col_idx, workload, metric, title, ylabel in configs:
            ax = axs[row_idx][col_idx]
            if workload not in results.get(ds_label, {}):
                ax.set_title(title + " (N/A)")
                continue

            data = results[ds_label][workload][metric]
            values = [data.get(idx, 0) for idx in INDEXES]
            bars = ax.bar(INDEXES, values, color=colors)
            ax.set_title(title, fontsize=10)
            ax.set_ylabel(ylabel, fontsize=9)

            # Add value labels on bars
            for bar, val in zip(bars, values):
                if metric == "size":
                    label = f"{val/1e9:.2f}GB" if val > 1e9 else f"{val/1e6:.0f}MB"
                else:
                    label = f"{val:.2f}"
                ax.text(bar.get_x() + bar.get_width() / 2., bar.get_height(),
                        label, ha='center', va='bottom', fontsize=8)

    fig.suptitle("Task 2 Milestone 3: Hybrid DPGM+LIPP (All Datasets)", fontsize=14)
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    plt.savefig("task2m3_results/task2m3_benchmark_results.png", dpi=300)
    print("Saved plot to task2m3_results/task2m3_benchmark_results.png")

    # Save summary CSV
    rows_out = []
    for ds_label in DATASETS:
        for wl_label in WORKLOADS:
            if wl_label not in results.get(ds_label, {}):
                continue
            for idx in INDEXES:
                rows_out.append({
                    "dataset": ds_label,
                    "workload": wl_label,
                    "index": idx,
                    "throughput_mops": results[ds_label][wl_label]["throughput"].get(idx, 0),
                    "index_size_bytes": results[ds_label][wl_label]["size"].get(idx, 0),
                })
    df = pd.DataFrame(rows_out)
    df.to_csv("task2m3_results/task2m3_summary.csv", index=False)
    print("Saved summary to task2m3_results/task2m3_summary.csv")
    print(df.to_string(index=False))


if __name__ == "__main__":
    results = load_results()
    plot_results(results)
