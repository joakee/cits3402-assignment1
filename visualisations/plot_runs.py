#!/usr/bin/env python3
"""
Plots the OpenMP runs in big_run_no1/ (one .out per slurm job).

Each run only reports one number, the "Elapsed nonce calculation time", so to
get a line per thread count this script estimates how many nonce pairs had
been checked when the collision was found, using the collision nonces and the
way schedule(static) splits the loop between threads (see estimate_pairs).
Each line is drawn from (0 s, 0 pairs) to (elapsed, estimated pairs), so its
slope is the average hashing rate. Only the end point comes from the run; the
straight line assumes the rate was constant.

outputs (written next to this script):
  <n>_<size>_progress.png  one chart per file size, a line per thread count
  time_vs_threads.png      measured elapsed time against threads, per size
  results.csv              everything parsed/derived, as a table

usage (from anywhere): python3 visualisations/plot_runs.py
needs numpy + matplotlib
"""

import csv
import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter, MaxNLocator

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
RUN_DIR = REPO / "big_run_no1"
TEST_DIR = REPO / "test_files"

# must match openmp_implementation.c
MAX_TRIES = 50_000_000
NONCE_MAX = 2 << 47  # nonce_2 = nonce_max - i

# ---- look -----------------------------------------------------------------
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_2 = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
AXIS = "#c3c2b7"

# fixed thread count -> colour, so a thread count is the same colour in every
# chart even when that size didn't run it (validated categorical order)
THREAD_COLOURS = {
    8: "#2a78d6",
    16: "#eb6834",
    32: "#1baf7a",
    48: "#eda100",
    64: "#e87ba4",
    96: "#008300",
}
SINGLE_SERIES = "#2a78d6"

plt.rcParams.update({
    "font.family": ["Inter", "DejaVu Sans"],
    "font.size": 10,
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "savefig.facecolor": SURFACE,
    "axes.edgecolor": AXIS,
    "axes.linewidth": 0.8,
    "axes.labelcolor": INK_2,
    "axes.titlecolor": INK,
    "xtick.color": MUTED,
    "ytick.color": MUTED,
    "xtick.labelcolor": INK_2,
    "ytick.labelcolor": INK_2,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "grid.color": GRID,
    "grid.linewidth": 0.7,
    "grid.linestyle": "-",
    "legend.frameon": False,
    "legend.labelcolor": INK_2,
})

OUT_RE = re.compile(r"^(\d+)_([a-z]+)_omp_(\d+)_(\d+)\.out$")
COLLISION_RE = re.compile(
    r"collision: pdf([12]) nonce (\d+) hits a pdf[12] hash \(nonce (\d+)\)")
ELAPSED_RE = re.compile(r"Elapsed nonce calculation time:\s*([0-9.]+)")


def loop_index(pdf, nonce):
    """nonce -> the loop iteration i that produced it"""
    return nonce if pdf == 1 else NONCE_MAX - nonce


def static_chunk(i, threads, n=MAX_TRIES):
    """
    (start, size) of the schedule(static) chunk holding iteration i.
    gcc/libgomp gives the first n % threads threads one extra iteration.
    """
    q, extra = divmod(n, threads)
    boundary = extra * (q + 1)
    if i < boundary:
        tid = i // (q + 1)
        return tid * (q + 1), q + 1
    tid = extra + (i - boundary) // q
    return boundary + (tid - extra) * q, q


def estimate_pairs(threads, current_i, stored_i):
    """
    Roughly how many nonce pairs had been checked when the collision was hit.
    Every thread walks its own chunk at about the same speed, so when the
    hitting thread is `offset` iterations into its chunk, every other thread
    is about that far into theirs too.
    """
    start, _ = static_chunk(current_i, threads)
    offset = current_i - start
    q, extra = divmod(MAX_TRIES, threads)
    sizes = np.array([q + 1] * extra + [q] * (threads - extra))
    pairs = int(np.minimum(sizes, offset + 1).sum())

    # under the same-speed assumption the stored half of the pair must have
    # been reached first; if not, the estimate for this run is shakier
    stored_start, _ = static_chunk(stored_i, threads)
    consistent = stored_i - stored_start <= offset
    return pairs, consistent


def load_runs():
    runs = []
    for path in sorted(RUN_DIR.glob("*.out")):
        m = OUT_RE.match(path.name)
        if not m:
            print(f"skipping {path.name}: unexpected name")
            continue
        text = path.read_text()
        col = COLLISION_RE.search(text)
        ela = ELAPSED_RE.search(text)
        if not col or not ela:
            print(f"skipping {path.name}: no collision/elapsed line")
            continue

        threads = int(m.group(3))
        pdf = int(col.group(1))
        current_i = loop_index(pdf, int(col.group(2)))
        stored_i = loop_index(3 - pdf, int(col.group(3)))
        elapsed = float(ela.group(1))
        pairs, consistent = estimate_pairs(threads, current_i, stored_i)

        size_name = f"{m.group(1)}_{m.group(2)}"
        pdf_path = TEST_DIR / f"{size_name}_a.pdf"
        runs.append({
            "size": size_name,
            "size_order": int(m.group(1)),
            "pdf_bytes": pdf_path.stat().st_size if pdf_path.exists() else "",
            "threads": threads,
            "job_id": int(m.group(4)),
            "elapsed_s": elapsed,
            "collision_iteration": current_i,
            "est_pairs_checked": pairs,
            "est_pairs_per_s": pairs / elapsed,
            "estimate_consistent": consistent,
        })
    return sorted(runs, key=lambda r: (r["size_order"], r["threads"]))


def size_title(run):
    name = run["size"].split("_", 1)[1].capitalize()
    b = run["pdf_bytes"]
    return f"{name} ({b // 1024:,} KiB PDFs)" if b else name


def millions(x, _pos):
    return f"{x / 1e6:g}M" if x else "0"


# where to try a line-end label, in order: right, above, below, left
LABEL_SPOTS = [((8, 0), "left", "center"), ((0, 9), "center", "bottom"),
               ((0, -9), "center", "top"), ((-8, 0), "right", "center")]


def label_ends(ax, points, lines):
    """
    direct label at each line end. tries each spot in LABEL_SPOTS and keeps
    the first that doesn't cover another label, any line or dot, or run off
    the plot; a label with no free spot is left off (the legend and
    results.csv still cover it)
    """
    fig = ax.figure
    fig.canvas.draw()
    renderer = fig.canvas.get_renderer()
    plot_box = ax.get_window_extent(renderer)

    # every line sampled densely in pixel space, dots included as end samples
    t = np.linspace(0, 1, 500)[:, None]
    samples = np.vstack([
        ax.transData.transform(np.hstack([xs[0] + t * (xs[1] - xs[0]),
                                          ys[0] + t * (ys[1] - ys[0])]))
        for xs, ys in lines
    ])

    placed = []
    for x, y, text in points:
        for offset, ha, va in LABEL_SPOTS:
            label = ax.annotate(text, (x, y), xytext=offset,
                                textcoords="offset points", ha=ha, va=va,
                                fontsize=9, color=INK_2)
            box = label.get_window_extent(renderer).padded(6)
            hits_mark = ((samples[:, 0] >= box.x0) & (samples[:, 0] <= box.x1) &
                         (samples[:, 1] >= box.y0) & (samples[:, 1] <= box.y1)).any()
            fits = (box.x0 >= plot_box.x0 and box.x1 <= plot_box.x1 and
                    box.y0 >= plot_box.y0 and box.y1 <= plot_box.y1)
            if fits and not hits_mark and not any(box.overlaps(p) for p in placed):
                placed.append(box)
                break
            label.remove()


def header(fig, title, subtitle):
    fig.text(0.02, 0.975, title, ha="left", va="top", fontsize=13,
             fontweight="semibold", color=INK)
    fig.text(0.02, 0.925, subtitle, ha="left", va="top", fontsize=9,
             color=INK_2, linespacing=1.4)


def plot_progress(size_runs, out_path):
    fig, ax = plt.subplots(figsize=(8, 5.2), dpi=200)
    fig.subplots_adjust(left=0.1, right=0.97, top=0.76, bottom=0.12)

    ends, lines = [], []
    for r in size_runs:
        lines.append(([0, r["elapsed_s"]], [0, r["est_pairs_checked"]]))
        colour = THREAD_COLOURS[r["threads"]]
        ax.plot([0, r["elapsed_s"]], [0, r["est_pairs_checked"]],
                color=colour, lw=1.8, solid_capstyle="round",
                label=f"{r['threads']} threads", zorder=3)
        ax.plot(r["elapsed_s"], r["est_pairs_checked"], "o", ms=6.5,
                color=colour, mec=SURFACE, mew=1.5, zorder=4)
        ends.append((r["elapsed_s"], r["est_pairs_checked"],
                     f"{r['threads']} thr · {r['elapsed_s']:.0f} s"))

    max_t = max(r["elapsed_s"] for r in size_runs)
    max_p = max(r["est_pairs_checked"] for r in size_runs)
    ax.set_xlim(0, max_t * 1.22)
    ax.set_ylim(0, max_p * 1.1)
    ax.xaxis.set_major_locator(MaxNLocator(nbins=7, steps=[1, 2, 2.5, 5, 10]))
    ax.yaxis.set_major_formatter(FuncFormatter(millions))
    ax.set_xlabel("Elapsed nonce calculation time (s)")
    ax.set_ylabel("Nonce pairs checked (estimated)")
    ax.tick_params(length=0)

    ax.legend(loc="lower left", bbox_to_anchor=(0, 1.02), borderaxespad=0,
              ncol=len(size_runs), fontsize=9, handlelength=1.6,
              columnspacing=1.4)
    label_ends(ax, sorted(ends, key=lambda e: e[0]), lines)

    header(fig, f"{size_title(size_runs[0])}: search progress by thread count",
           "Dot = when the collision was found. Steeper line = more pairs "
           "hashed per second.\nPairs checked is estimated from the collision "
           "nonces and the schedule(static) split; the line assumes a steady rate.")
    fig.savefig(out_path)
    plt.close(fig)


def plot_time_vs_threads(runs, out_path):
    sizes = sorted({(r["size_order"], r["size"]) for r in runs})
    all_threads = sorted(THREAD_COLOURS)

    fig, axes = plt.subplots(2, 3, figsize=(10, 6.4), dpi=200,
                             sharex=True, sharey=True)
    fig.subplots_adjust(left=0.08, right=0.98, top=0.84, bottom=0.1,
                        hspace=0.35, wspace=0.12)
    max_t = max(r["elapsed_s"] for r in runs)

    for ax, (_, size) in zip(axes.flat, sizes):
        rs = [r for r in runs if r["size"] == size]
        x = [r["threads"] for r in rs]
        y = [r["elapsed_s"] for r in rs]
        ax.plot(x, y, color=SINGLE_SERIES, lw=1.8, solid_capstyle="round",
                zorder=3)
        ax.plot(x, y, "o", ms=6, color=SINGLE_SERIES, mec=SURFACE, mew=1.5,
                zorder=4)
        ax.set_title(size_title(rs[0]), loc="left", fontsize=10,
                     fontweight="semibold", pad=8)
        ax.set_xscale("log", base=2)
        ax.set_xticks(all_threads)
        ax.set_xticklabels([str(t) for t in all_threads])
        ax.minorticks_off()
        ax.set_xlim(6.5, 118)
        ax.set_ylim(0, max_t * 1.1)
        ax.tick_params(length=0)

    for ax in axes[-1]:
        ax.set_xlabel("Threads")
    for ax in axes[:, 0]:
        ax.set_ylabel("Elapsed time (s)")

    header(fig, "Elapsed nonce calculation time vs threads",
           "Measured, one run per point (big_run_no1). Same y-axis in every panel; "
           "x-axis is log₂.")
    fig.savefig(out_path)
    plt.close(fig)


def write_csv(runs, out_path):
    fields = ["size", "pdf_bytes", "threads", "job_id", "elapsed_s",
              "collision_iteration", "est_pairs_checked", "est_pairs_per_s",
              "estimate_consistent"]
    with open(out_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in runs:
            w.writerow({**r, "est_pairs_per_s": round(r["est_pairs_per_s"])})


def main():
    runs = load_runs()
    if not runs:
        raise SystemExit(f"no runs found in {RUN_DIR}")

    write_csv(runs, HERE / "results.csv")
    for size in sorted({r["size"] for r in runs}):
        size_runs = [r for r in runs if r["size"] == size]
        plot_progress(size_runs, HERE / f"{size}_progress.png")
    plot_time_vs_threads(runs, HERE / "time_vs_threads.png")

    print(f"{'size':<8} {'thr':>4} {'elapsed':>9} {'est pairs':>11} "
          f"{'pairs/s':>9}  consistent")
    for r in runs:
        print(f"{r['size']:<8} {r['threads']:>4} {r['elapsed_s']:>8.1f}s "
              f"{r['est_pairs_checked']:>11,} {r['est_pairs_per_s']:>9,.0f}  "
              f"{r['estimate_consistent']}")
    print(f"wrote {len(runs)} runs -> {HERE}")


if __name__ == "__main__":
    main()
