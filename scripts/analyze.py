#!/usr/bin/env python3
"""PulseWarp comparison + plots.

    python3 scripts/analyze.py results/sweep_*.csv [--out results/analysis]

Reads one or more sweep CSVs (rows printed by ./pulsewarp), and writes
  summary.csv   runtime, speedup vs the matching serial kernel, efficiency,
                speedup vs row-order serial, and speedup vs own 1-thread run
  summary.md    the same as Markdown tables, plus A-vs-B winner per (len, p)
  fig_*.png     runtime, speedup, efficiency, A/B ratio heatmap, serial scaling
Needs pandas + matplotlib (on ARC: module load python, or pip install --user).
"""
import argparse
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

# fixed variant -> colour/marker, so a variant keeps its colour in every figure
VARIANTS = [
    ("A-static",   "Strategy A, static",           "#2a78d6", "o"),
    ("A-dynamic",  "Strategy A, dynamic",          "#eb6834", "s"),
    ("A-guided",   "Strategy A, guided",           "#8b5cf6", "P"),
    ("Aflat-static", "Strategy A flat, static",      "#0f766e", "X"),
    ("Aflat-dynamic", "Strategy A flat, dynamic",    "#d97706", "*"),
    ("Aflat-guided", "Strategy A flat, guided",      "#64748b", "h"),
    ("Adiag-dynamic", "Strategy A, diag kernel",   "#1baf7a", "D"),
    ("Adflat-dynamic", "Strategy A flat, diag kernel", "#a855f7", "v"),
    ("B",          "Strategy B, wavefront",        "#6250d6", "^"),
    ("Btiled",     "Strategy B, tiled",            "#e87ba4", "v"),
]
STYLE = {k: (lab, c, m) for k, lab, c, m in VARIANTS}
INK, MUTED, GRID = "#0b0b0b", "#52514e", "#e4e3df"


def variant(row):
    if row.strategy in ("A", "Aflat", "Adiag", "Adflat"):
        return f"{row.strategy}-{str(row.schedule).replace(',', ':')}"
    return row.strategy


def load(paths, allow_merge=False):
    frames = []
    for p in paths:
        f = pd.read_csv(p)
        f["_src"] = os.path.basename(p)
        frames.append(f)
    df = pd.concat(frames, ignore_index=True)
    # The same configuration in several files usually means sweeps from different
    # nodes/CPUs; averaging them would silently mix hardware. Refuse unless asked.
    dup = df.groupby(["strategy", "schedule", "tile", "threads", "n", "len"])["_src"].nunique()
    if (dup > 1).any() and not allow_merge:
        sys.exit(f"ERROR: {int((dup > 1).sum())} configurations appear in more than one input CSV "
                 f"({', '.join(sorted(df._src.unique()))}).\nThese are probably different machines. "
                 "Pass a single sweep CSV, or --merge-repeats to take the median across files.")
    bad = df[df.verify.isin(["FAIL", "ERROR"])]
    if len(bad):
        print(f"WARNING: {len(bad)} rows failed verification:", file=sys.stderr)
        print(bad[["strategy", "schedule", "threads", "len", "max_rel_err"]], file=sys.stderr)
    df["variant"] = df.apply(variant, axis=1)
    # repeated configurations (several sweep files): keep the median
    keys = ["variant", "n", "len", "threads"]
    return df.groupby(keys, as_index=False).agg(time=("median_s", "median"),
                                                verify=("verify", lambda v: ",".join(sorted(set(v)))),
                                                max_rel_err=("max_rel_err", "max"))


def summarise(df):
    ser = df[df.variant == "serial"].set_index(["n", "len"]).time.rename("t_serial")
    diag = df[df.variant == "Bdiag"].set_index(["n", "len"]).time.rename("t_diag_serial")
    par = df[~df.variant.isin(["serial", "Bdiag"])].copy()
    par = par.join(ser, on=["n", "len"]).join(diag, on=["n", "len"])
    one = par[par.threads == 1].set_index(["variant", "n", "len"]).time.rename("t_self1")
    par = par.join(one, on=["variant", "n", "len"])
    # Preserve same-variant thread scaling as a separate metric. Headline
    # speedup uses the matching serial kernel: row order for A/Aflat and
    # diagonal order for Adiag/Adflat/B/Btiled.
    par["self_speedup"] = par.t_self1 / par.time              # vs same code at p=1
    par["speedup_vs_row_serial"] = par.t_serial / par.time
    par["speedup_vs_diag"] = par.t_diag_serial / par.time     # vs diagonal-order serial
    diag_variants = par.variant.isin(
        ["Adiag-dynamic", "Adflat-dynamic", "B", "Btiled"]
    )
    par["speedup"] = par.speedup_vs_row_serial
    par.loc[diag_variants, "speedup"] = par.loc[
        diag_variants, "speedup_vs_diag"
    ]
    par["efficiency"] = par.speedup / par.threads             # S_p / p
    return par.sort_values(["len", "variant", "threads"])


def style_ax(ax):
    ax.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(MUTED)
    ax.tick_params(colors=MUTED, labelsize=8)


def facet(par, ycol, ylabel, fname, out, ideal=None, logy=False):
    lens = sorted(par.len.unique())
    cols = min(3, len(lens))
    rows = int(np.ceil(len(lens) / cols))
    fig, axes = plt.subplots(rows, cols, figsize=(4.2 * cols, 3.4 * rows), squeeze=False, sharex=True)
    threads = sorted(par.threads.unique())
    for ax, L in zip(axes.flat, lens):
        sub = par[par.len == L]
        if ideal is not None:
            ax.plot(threads, [ideal(t) for t in threads], color=MUTED, lw=1, ls="--", label="ideal")
        for key, _, _, _ in VARIANTS:
            s = sub[sub.variant == key]
            if s.empty:
                continue
            lab, c, m = STYLE[key]
            ax.plot(s.threads, s[ycol], color=c, marker=m, ms=5, lw=2, label=lab)
        ax.set_xscale("log", base=2)
        ax.set_xticks(threads, [str(t) for t in threads])
        if logy:
            ax.set_yscale("log")
        ax.set_title(f"length {L}", fontsize=10, color=INK)
        style_ax(ax)
    for ax in axes.flat[len(lens):]:
        ax.set_visible(False)
    for ax in axes[:, 0]:
        ax.set_ylabel(ylabel, fontsize=9, color=INK)
    for ax in axes[-1, :]:
        ax.set_xlabel("threads", fontsize=9, color=INK)
    h, l = axes.flat[0].get_legend_handles_labels()
    fig.legend(h, l, loc="upper center", ncol=4, frameon=False, fontsize=8)
    fig.tight_layout(rect=(0, 0, 1, 0.88))
    fig.savefig(os.path.join(out, fname), dpi=160)
    plt.close(fig)


def heatmap_ab(par, out):
    """Compare B against row-order A and the matching diagonal-order A kernel."""
    for akey, suffix in (("A-dynamic", ""), ("Adiag-dynamic", "_over_Adiag")):
        a = par[par.variant == akey].set_index(["len", "threads"]).time
        for bkey, prefix in (("B", "fig_ratio_B"), ("Btiled", "fig_ratio_Btiled")):
            b = par[par.variant == bkey].set_index(["len", "threads"]).time
            r = (b / a).dropna().unstack("threads")
            if r.empty:
                continue
            fig, ax = plt.subplots(figsize=(1.0 + 0.9 * r.shape[1], 1.0 + 0.6 * r.shape[0]))
            lr = np.log2(r.values)
            lim = max(1.0, np.nanmax(np.abs(lr)))
            im = ax.imshow(lr, cmap="RdBu_r", vmin=-lim, vmax=lim, aspect="auto")
            ax.set_xticks(range(r.shape[1]), r.columns)
            ax.set_yticks(range(r.shape[0]), r.index)
            ax.set_xlabel("threads"); ax.set_ylabel("sequence length")
            for i in range(r.shape[0]):
                for j in range(r.shape[1]):
                    v = r.values[i, j]
                    if np.isfinite(v):
                        ax.text(j, i, f"{v:.1f}x", ha="center", va="center", fontsize=8,
                                color="white" if abs(lr[i, j]) > 0.6 * lim else INK)
            ax.set_title(f"T({STYLE[bkey][0]}) / T({STYLE[akey][0]})\n>1: A faster", fontsize=9)
            fig.colorbar(im, ax=ax, label="log2 ratio", shrink=0.8)
            fig.tight_layout()
            tail = suffix if suffix else "_over_A"
            fig.savefig(os.path.join(out, f"{prefix}{tail}.png"), dpi=160)
            plt.close(fig)


def serial_scaling(df, out):
    fig, ax = plt.subplots(figsize=(5, 3.4))
    for key, lab, c, m in (("serial", "serial, row order", "#2a78d6", "o"),
                           ("Bdiag", "serial, diagonal order", "#6250d6", "^")):
        s = df[df.variant == key].sort_values("len")
        if not s.empty:
            ax.plot(s.len, s.time, color=c, marker=m, lw=2, ms=5, label=lab)
    s = df[df.variant == "serial"].sort_values("len")
    if len(s):
        L0, t0 = s.len.iloc[0], s.time.iloc[0]
        ax.plot(s.len, t0 * (s.len / L0) ** 2, color=MUTED, ls="--", lw=1, label="O(L²)")
    ax.set_xscale("log", base=2); ax.set_yscale("log")
    ax.set_xlabel("sequence length"); ax.set_ylabel("all-pairs runtime (s)")
    style_ax(ax); ax.legend(frameon=False, fontsize=8)
    fig.tight_layout(); fig.savefig(os.path.join(out, "fig_serial_vs_length.png"), dpi=160)
    plt.close(fig)


def crossover_plot(par, out):
    """Plan's length crossover plot at p=4/max, with both serial baselines."""
    ps = sorted(set(p for p in (4, int(par.threads.max())) if p in set(par.threads)))
    fig, axes = plt.subplots(2, len(ps), figsize=(4.6 * len(ps), 6.4),
                             squeeze=False, sharex=True)
    rows = (("speedup_vs_row_serial", "speedup vs row-order serial"),
            ("speedup_vs_diag", "speedup vs diagonal-order serial"))
    for r, (column, ylabel) in enumerate(rows):
        for c, p in enumerate(ps):
            ax = axes[r, c]
            ax.axhline(1, color=MUTED, lw=1, ls=":")
            ax.axhline(p, color=MUTED, lw=1, ls="--", label=f"ideal p={p}" if r == 0 else None)
            for key, label, color, marker in VARIANTS:
                sub = par[(par.variant == key) & (par.threads == p)].sort_values("len")
                if not sub.empty:
                    ax.plot(sub.len, sub[column], color=color, marker=marker, ms=5,
                            lw=2, label=label)
            ax.set_xscale("log", base=2); ax.set_yscale("log")
            ax.set_xticks(sorted(par.len.unique()), [str(x) for x in sorted(par.len.unique())])
            ax.set_title(f"p = {p}", fontsize=10, color=INK)
            ax.set_ylabel(ylabel, fontsize=9, color=INK)
            if r == 1: ax.set_xlabel("sequence length", fontsize=9, color=INK)
            style_ax(ax)
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=4, frameon=False, fontsize=8)
    fig.tight_layout(rect=(0, 0, 1, 0.86))
    fig.savefig(os.path.join(out, "fig_crossover.png"), dpi=160)
    plt.close(fig)


def correctness_table(df, out):
    """Summarize verification coverage and maximum relative error by variant."""
    rows = []
    for key, sub in df.groupby("variant", sort=True):
        flags = ",".join(sorted(set(",".join(sub.verify).split(","))))
        err = sub.max_rel_err.max()
        rows.append((key, len(sub), flags, "n/a" if pd.isna(err) else f"{err:.3e}"))
    fig, ax = plt.subplots(figsize=(7, max(2.2, 0.42 * len(rows) + 1.2)))
    ax.axis("off")
    table = ax.table(cellText=rows, colLabels=["variant", "runs", "verification", "max relative error"],
                     loc="center", cellLoc="center")
    table.auto_set_font_size(False); table.set_fontsize(9); table.scale(1, 1.5)
    ax.set_title("Correctness against serial golden vectors", pad=16)
    fig.tight_layout(); fig.savefig(os.path.join(out, "fig_correctness.png"), dpi=170)
    plt.close(fig)
    pd.DataFrame(rows, columns=["variant", "runs", "verification", "max_rel_err"]).to_csv(
        os.path.join(out, "correctness.csv"), index=False)


def load_imbalance_plot(path, out):
    if not path:
        return False
    p = pd.read_csv(path)
    if not {"thread", "pairs", "elapsed_s"}.issubset(p.columns):
        raise ValueError("profile CSV must contain thread,pairs,elapsed_s")
    fig, ax = plt.subplots(figsize=(7, 3.6))
    x = np.arange(len(p))
    ax.bar(x, p.elapsed_s, color="#2a78d6", label="loop time")
    ax.set_xticks(x, p.thread)
    ax.set_xlabel("OpenMP thread"); ax.set_ylabel("assigned-loop time (s)")
    title = "Strategy A per-thread load profile"
    if {"strategy", "schedule", "n", "len", "threads"}.issubset(p.columns):
        r = p.iloc[0]
        title += f" ({r.strategy}, {r.schedule}, N={r.n}, L={r.len}, p={r.threads})"
    ax.set_title(title)
    style_ax(ax)
    ax2 = ax.twinx(); ax2.plot(x, p.pairs, color="#eb6834", marker="o", lw=1.5)
    ax2.set_ylabel("pair count", color="#eb6834")
    fig.tight_layout(); fig.savefig(os.path.join(out, "fig_load_imbalance.png"), dpi=170)
    plt.close(fig)
    return True


def write_md(df, par, out):
    lines = ["# PulseWarp results", ""]
    lines += ["## Serial baselines (s)", "", "| length | row order | diagonal order |", "|---:|---:|---:|"]
    for L in sorted(df.len.unique()):
        s = df[(df.len == L) & (df.variant == "serial")].time
        d = df[(df.len == L) & (df.variant == "Bdiag")].time
        lines.append(f"| {L} | {s.iloc[0]:.3f} | {d.iloc[0]:.3f} |" if len(s) and len(d) else f"| {L} | - | - |")
    lines += ["", "## Speedup vs matching serial baseline (efficiency in brackets)",
              "", "Speedup is T_serial / T_p; efficiency is speedup / thread count.",
              "Row-order A variants use row-order serial. Diagonal-kernel and wavefront variants use diagonal-order serial (Bdiag).",
              "Scaling against each variant's own one-thread run is available in `summary.csv` as `self_speedup` and in `fig_self_speedup.png`.", ""]
    threads = sorted(par.threads.unique())
    for L in sorted(par.len.unique()):
        lines += [f"### length {L}", "", "| variant | " + " | ".join(f"p={t}" for t in threads) + " |",
                  "|---|" + "---:|" * len(threads)]
        for key, lab, _, _ in VARIANTS:
            s = par[(par.len == L) & (par.variant == key)].set_index("threads")
            if s.empty:
                continue
            cells = [f"{s.speedup[t]:.2f} ({s.efficiency[t]:.2f})" if t in s.index else "-" for t in threads]
            lines.append(f"| {lab} | " + " | ".join(cells) + " |")
        lines.append("")
    lines += ["## Best Strategy A variant vs best Strategy B variant per configuration", "",
              "Cell: winner (best time of the other strategy / winner time). Strategy A candidates include the",
              "diagonal-kernel variants (Adiag, Adflat), so the result is not decided by the kernel alone.",
              "Same-kernel pairs: A-dynamic/Aflat vs Btiled (row-order cells), Adiag/Adflat vs B (diagonal order).", "",
              "| length | " + " | ".join(f"p={t}" for t in threads) + " |", "|---|" + "---:|" * len(threads)]
    a_vars = [k for k, *_ in VARIANTS if k.startswith("A")]
    b_vars = ["B", "Btiled"]
    for L in sorted(par.len.unique()):
        row = []
        for t in threads:
            q = par[(par.len == L) & (par.threads == t)]
            qa, qb = q[q.variant.isin(a_vars)], q[q.variant.isin(b_vars)]
            if qa.empty or qb.empty:
                row.append("-"); continue
            wa, wb = qa.loc[qa.time.idxmin()], qb.loc[qb.time.idxmin()]
            if wa.time <= wb.time:
                row.append(f"A: {wa.variant} ({wb.time / wa.time:.1f}x)")
            else:
                row.append(f"B: {wb.variant} ({wa.time / wb.time:.1f}x)")
        lines.append(f"| {L} | " + " | ".join(row) + " |")
    verdicts = sorted(set(",".join(df.verify).split(",")))
    lines += ["", f"Verification flags seen: {', '.join(verdicts)}", ""]
    with open(os.path.join(out, "summary.md"), "w") as f:
        f.write("\n".join(lines))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+")
    ap.add_argument("--out", default="results/analysis")
    ap.add_argument("--merge-repeats", action="store_true",
                    help="allow the same configuration in several CSVs (median is taken)")
    ap.add_argument("--profile", help="per-thread profile CSV from --profile in pulsewarp")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    df = load(a.csv, a.merge_repeats)
    par = summarise(df)
    par.to_csv(os.path.join(a.out, "summary.csv"), index=False, float_format="%.6g")
    facet(par, "time", "runtime (s)", "fig_runtime.png", a.out, logy=True)
    facet(par, "speedup", "speedup vs matching serial baseline", "fig_speedup.png", a.out, ideal=lambda t: t)
    facet(par, "efficiency", "efficiency (S/p)", "fig_efficiency.png", a.out, ideal=lambda t: 1.0)
    facet(par, "self_speedup", "speedup vs own 1-thread", "fig_self_speedup.png", a.out, ideal=lambda t: t)
    heatmap_ab(par, a.out)
    serial_scaling(df, a.out)
    crossover_plot(par, a.out)
    correctness_table(df, a.out)
    if not load_imbalance_plot(a.profile, a.out):
        print("note: pass --profile results/profile.csv to generate fig_load_imbalance.png")
    write_md(df, par, a.out)
    print(f"wrote summary.csv, summary.md and figures to {a.out}")


if __name__ == "__main__":
    main()