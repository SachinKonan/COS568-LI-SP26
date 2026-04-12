"""
Task 2 Milestone 2: Analysis script.
Produces 4 bar plots for the Facebook dataset:
  1. Mixed 10% Insert - Throughput
  2. Mixed 10% Insert - Index Size
  3. Mixed 90% Insert - Throughput
  4. Mixed 90% Insert - Index Size
"""
import os
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def load_results():
    dataset = "fb_100M_public_uint64"
    indexes = ["DynamicPGM", "LIPP", "Hybrid"]

    mix10_file = f"results/{dataset}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix_results_table.csv"
    mix90_file = f"results/{dataset}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix_results_table.csv"

    mix10 = pd.read_csv(mix10_file)
    mix90 = pd.read_csv(mix90_file)

    results = {}
    for label, df in [("mix10", mix10), ("mix90", mix90)]:
        throughputs = {}
        sizes = {}
        for idx in indexes:
            rows = df[df["index_name"] == idx]
            if rows.empty:
                throughputs[idx] = 0
                sizes[idx] = 0
                continue
            # Average throughput across 3 runs, pick best hyperparameter config
            avg = rows[["mixed_throughput_mops1", "mixed_throughput_mops2",
                         "mixed_throughput_mops3"]].mean(axis=1)
            best_row = avg.idxmax()
            throughputs[idx] = avg[best_row]
            sizes[idx] = rows.loc[best_row, "index_size_bytes"]
        results[label] = {"throughput": throughputs, "size": sizes}

    return results, indexes


def plot_results(results, indexes):
    os.makedirs("task2_results", exist_ok=True)

    fig, axs = plt.subplots(2, 2, figsize=(12, 8))

    configs = [
        (0, 0, "mix10", "throughput", "Mixed 10% Insert - Throughput", "Throughput (Mops/s)"),
        (0, 1, "mix10", "size", "Mixed 10% Insert - Index Size", "Index Size (bytes)"),
        (1, 0, "mix90", "throughput", "Mixed 90% Insert - Throughput", "Throughput (Mops/s)"),
        (1, 1, "mix90", "size", "Mixed 90% Insert - Index Size", "Index Size (bytes)"),
    ]

    colors = ["#4C72B0", "#DD8452", "#55A868"]

    for row, col, workload, metric, title, ylabel in configs:
        ax = axs[row][col]
        data = results[workload][metric]
        values = [data[idx] for idx in indexes]
        bars = ax.bar(indexes, values, color=colors)
        ax.set_title(title)
        ax.set_ylabel(ylabel)

        # Add value labels on bars
        for bar, val in zip(bars, values):
            if metric == "size":
                label = f"{val/1e9:.2f} GB" if val > 1e9 else f"{val/1e6:.1f} MB"
            else:
                label = f"{val:.2f}"
            ax.text(bar.get_x() + bar.get_width() / 2., bar.get_height(),
                    label, ha='center', va='bottom', fontsize=9)

    fig.suptitle("Task 2 Milestone 2: Hybrid DPGM+LIPP (Facebook Dataset)", fontsize=14)
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig("task2_results/task2_benchmark_results.png", dpi=300)
    print("Saved plot to task2_results/task2_benchmark_results.png")

    # Save summary CSV
    rows = []
    for workload in ["mix10", "mix90"]:
        for idx in indexes:
            rows.append({
                "workload": workload,
                "index": idx,
                "throughput_mops": results[workload]["throughput"][idx],
                "index_size_bytes": results[workload]["size"][idx],
            })
    df = pd.DataFrame(rows)
    df.to_csv("task2_results/task2_summary.csv", index=False)
    print("Saved summary to task2_results/task2_summary.csv")
    print(df.to_string(index=False))


if __name__ == "__main__":
    results, indexes = load_results()
    plot_results(results, indexes)
