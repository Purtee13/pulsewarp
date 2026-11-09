#!/usr/bin/env python3
"""PulseWarp comparison + plots.

    python3 scripts/analyze.py results/sweep_*.csv [--out results/analysis]

Reads one or more sweep CSVs (rows printed by ./pulsewarp), and writes
  summary.csv   runtime, speedup vs row-order serial, efficiency, and
                speedup vs the variant's own 1-thread run, per configuration
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
    ("Adiag-dynamic", "Strategy A, diag kernel",   "#1baf7a", "D"),
    ("B",          "Strategy B, wavefront",        "#6250d6", "^"),
    ("Btiled",     "Strategy B, tiled",            "#e87ba4", "v"),
]
STYLE = {k: (lab, c, m) for k, lab, c, m in VARIANTS}
INK, MUTED, GRID = "#0b0b0b", "#52514e", "#e4e3df"


def variant(row):
    if row.strategy in ("A", "Adiag"):
        return f"{row.strategy}-{str(row.schedule).split(',')[0]}"
    return row.strategy


def load(paths):
    df = pd.concat([pd.read_csv(p) for p in paths], ignore_index=True)
    bad = df[df.verify.isin(["FAIL", "ERROR"])]
    if len(bad):
        print(f"WARNING: {len(bad)} rows failed verification:", file=sys.stderr)
        print(bad[["strategy", "schedule", "threads", "len", "max_rel_err"]], file=sys.stderr)
    df["variant"] = df.apply(variant, axis=1)
    # repeated configurations (several sweep files): keep the median
    keys = ["variant", "n", "len", "threads"]
    return df.groupby(keys, as_index=False).agg(time=("median_s", "median"),
                                                verify=("verify", lambda v: ",".join(sorted(set(v)))))


def summarise(df):
    ser = df[df.variant == "serial"].set_index(["n", "len"]).time.rename("t_serial")
    diag = df[df.variant == "Bdiag"].set_index(["n", "len"]).time.rename("t_diag_serial")
    par = df[~df.variant.isin(["serial", "Bdiag"])].copy()
    par = par.join(ser, on=["n", "len"]).join(diag, on=["n", "len"])
    one = par[par.threads == 1].set_index(["variant", "n", "len"]).time.rename("t_self1")
    par = par.join(one, on=["variant", "n", "len"])
    par["speedup"] = par.t_serial / par.time                  # S_p = T_serial / T_p
    par["efficiency"] = par.speedup / par.threads             # S_p / p
    par["self_speedup"] = par.t_self1 / par.time              # vs same code at p=1
    par["speedup_vs_diag"] = par.t_diag_serial / par.time     # vs best serial kernel
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
    fig.legend(h, l, loc="upper center", ncol=len(l), frameon=False, fontsize=8)
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(os.path.join(out, fname), dpi=160)
    plt.close(fig)


def heatmap_ab(par, out):
    """T_B / T_A(dynamic): >1 means Strategy A is faster."""
    a = par[par.variant == "A-dynamic"].set_index(["len", "threads"]).time
    for bkey, fname in (("B", "fig_ratio_B_over_A.png"), ("Btiled", "fig_ratio_Btiled_over_A.png")):
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
        ax.set_title(f"T({STYLE[bkey][0]}) / T(Strategy A, dynamic)\n>1: A faster", fontsize=9)
        fig.colorbar(im, ax=ax, label="log2 ratio", shrink=0.8)
        fig.tight_layout()
        fig.savefig(os.path.join(out, fname), dpi=160)
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


def write_md(df, par, out):
    lines = ["# PulseWarp results", ""]
    lines += ["## Serial baselines (s)", "", "| length | row order | diagonal order |", "|---:|---:|---:|"]
    for L in sorted(df.len.unique()):
        s = df[(df.len == L) & (df.variant == "serial")].time
        d = df[(df.len == L) & (df.variant == "Bdiag")].time
        lines.append(f"| {L} | {s.iloc[0]:.3f} | {d.iloc[0]:.3f} |" if len(s) and len(d) else f"| {L} | - | - |")
    lines += ["", "## Speedup vs row-order serial (efficiency in brackets)", ""]
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
    lines += ["## Faster of A (dynamic) and B (wavefront / tiled) per configuration", "",
              "| length | " + " | ".join(f"p={t}" for t in threads) + " |", "|---|" + "---:|" * len(threads)]
    for L in sorted(par.len.unique()):
        row = []
        for t in threads:
            q = par[(par.len == L) & (par.threads == t) & par.variant.isin(["A-dynamic", "B", "Btiled"])]
            if q.empty:
                row.append("-"); continue
            w = q.loc[q.time.idxmin()]
            row.append(f"{w.variant} ({q.time.max() / w.time:.1f}x)")
        lines.append(f"| {L} | " + " | ".join(row) + " |")
    verdicts = sorted(set(",".join(df.verify).split(",")))
    lines += ["", f"Verification flags seen: {', '.join(verdicts)}", ""]
    with open(os.path.join(out, "summary.md"), "w") as f:
        f.write("\n".join(lines))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+")
    ap.add_argument("--out", default="results/analysis")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    df = load(a.csv)
    par = summarise(df)
    par.to_csv(os.path.join(a.out, "summary.csv"), index=False, float_format="%.6g")
    facet(par, "time", "runtime (s)", "fig_runtime.png", a.out, logy=True)
    facet(par, "speedup", "speedup vs serial", "fig_speedup.png", a.out, ideal=lambda t: t)
    facet(par, "efficiency", "efficiency (S/p)", "fig_efficiency.png", a.out, ideal=lambda t: 1.0)
    facet(par, "self_speedup", "speedup vs own 1-thread", "fig_self_speedup.png", a.out, ideal=lambda t: t)
    heatmap_ab(par, a.out)
    serial_scaling(df, a.out)
    write_md(df, par, a.out)
    print(f"wrote summary.csv, summary.md and figures to {a.out}")


if __name__ == "__main__":
    main()
