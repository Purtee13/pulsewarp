#!/usr/bin/env python3
"""Analysis of the extension experiments written by scripts/sweep_ext.sh.

    python3 scripts/analyze_ext.py [--dir results] [--out results/analysis_ext]

Picks the newest results/ext_<mode>_*.csv for each mode and writes
  fig_affinity.png    runtime per OMP_PROC_BIND/OMP_PLACES setting, relative to close/cores
  fig_weak.png        weak-scaling efficiency T(1)/T(p)
  fig_nsweep.png      few beats + long sequences: where Strategy B can overtake A
  ext_summary.md      the numbers behind each figure
Missing modes are skipped.
"""
import argparse, glob, os, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

INK, MUTED, GRID = "#0b0b0b", "#52514e", "#e4e3df"
PAL = ["#2a78d6", "#eb6834", "#1baf7a", "#6250d6", "#e87ba4", "#8a6d1d", "#52514e"]


def style_ax(ax):
    ax.grid(True, color=GRID, linewidth=0.8); ax.set_axisbelow(True)
    for s in ("top", "right"): ax.spines[s].set_visible(False)
    for s in ("left", "bottom"): ax.spines[s].set_color(MUTED)
    ax.tick_params(colors=MUTED, labelsize=8)


def newest(d, mode):
    f = sorted(glob.glob(os.path.join(d, f"ext_{mode}_*.csv")), key=os.path.getmtime)
    return f[-1] if f else None


def read(d, mode):
    f = newest(d, mode)
    if not f: return None
    df = pd.read_csv(f)
    bad = df[df.verify.isin(["FAIL", "ERROR"])] if "verify" in df else []
    if len(bad): print(f"WARNING {f}: {len(bad)} rows failed verification", file=sys.stderr)
    print(f"{mode}: {f}")
    return df


def save(fig, out, name):
    fig.tight_layout(); fig.savefig(os.path.join(out, name), dpi=160); plt.close(fig)


def affinity(df, out, md):
    md += ["## Thread affinity (time in s, ratio to close/cores)", "", "| workload | setting | threads | median (s) | ratio |", "|---|---|---:|---:|---:|"]
    work = [("Adflat", "Strategy A (Adflat, dynamic)"), ("Btiled", "Strategy B tiled")]
    fig, axes = plt.subplots(1, 2, figsize=(10, 3.6))
    for ax, (strat, title) in zip(axes, work):
        s = df[df.strategy == strat]
        if s.empty: ax.set_visible(False); continue
        s = s.groupby("tag", as_index=False).median_s.median()
        ref = s[s.tag == "close/cores"].median_s
        ref = ref.iloc[0] if len(ref) else s.median_s.min()
        ax.bar(range(len(s)), s.median_s / ref, color=PAL[3 if strat == "Btiled" else 0])
        ax.axhline(1, color=MUTED, lw=1, ls="--")
        ax.set_xticks(range(len(s)), s.tag, rotation=30, ha="right", fontsize=8)
        ax.set_ylabel("time / time(close,cores)", fontsize=9); ax.set_title(title, fontsize=9, color=INK); style_ax(ax)
        for _, r in s.iterrows():
            thr = int(df[(df.strategy == strat) & (df.tag == r.tag)].threads.iloc[0])
            md.append(f"| {title} | {r.tag} | {thr} | {r.median_s:.3f} | {r.median_s/ref:.2f} |")
    md.append("")
    save(fig, out, "fig_affinity.png")


def weak(df, out, md):
    md += ["## Weak scaling (work per thread constant; efficiency = T(1)/T(p), ideal 1.0)", "", "| experiment | variant | p | n | len | time (s) | efficiency |", "|---|---|---:|---:|---:|---:|---:|"]
    fig, ax = plt.subplots(figsize=(5.4, 3.8))
    ax.axhline(1.0, color=MUTED, ls="--", lw=1)
    k = 0
    for (exp, tag), s in df.groupby(["exp", "tag"]):
        s = s.sort_values("threads")
        if (s.threads == 1).sum() == 0: continue
        eff = s.median_s.iloc[0] / s.median_s.values
        label = {"weakA": "A: N~sqrt(p)", "weakB": "B-side: L~sqrt(p)"}[exp] + " / " + tag
        ax.plot(s.threads, eff, marker="osD^v"[k % 5], color=PAL[k % len(PAL)], lw=2, ms=5, label=label); k += 1
        for (_, r), e in zip(s.iterrows(), eff):
            md.append(f"| {exp} | {tag} | {r.threads} | {r.n} | {r.len} | {r.median_s:.3f} | {e:.2f} |")
    ax.set_xscale("log", base=2); ax.set_xticks(sorted(df.threads.unique()), [str(t) for t in sorted(df.threads.unique())])
    ax.set_xlabel("threads", fontsize=9); ax.set_ylabel("weak-scaling efficiency", fontsize=9); ax.legend(frameon=False, fontsize=7); style_ax(ax)
    md.append("")
    save(fig, out, "fig_weak.png")


def nsweep(df, out, md):
    lens = sorted(df.len.unique())
    fig, axes = plt.subplots(1, len(lens), figsize=(5.2 * len(lens), 3.9), squeeze=False)
    md += ["## Few beats, long sequences (speedup vs diagonal-order serial at the same N, L)", ""]
    order = ["A-outer", "A-flat", "B", "B+spin", "Btiled", "Btiled128"]
    names = {"A-outer": "A, outer-index loop", "A-flat": "A, flattened pairs", "B": "B wavefront", "B+spin": "B wavefront, OMP_WAIT_POLICY=active", "Btiled": "B tiled (64)", "Btiled128": "B tiled (128)"}
    p = int(df[df.tag == "B"].threads.max())
    for ax, L in zip(axes[0], lens):
        d = df[df.len == L]
        ref = d[d.tag == "serial-diag"].set_index("n").median_s
        ax.axhline(1, color=MUTED, ls=":", lw=1); ax.axhline(p, color=MUTED, ls="--", lw=1)
        md += [f"### length {L}, p={p}", "", "| variant | " + " | ".join(f"N={n}" for n in sorted(ref.index)) + " |", "|---|" + "---:|" * len(ref)]
        for k, tag in enumerate(order):
            s = d[d.tag == tag].set_index("n").median_s
            sp = (ref / s).dropna().sort_index()
            if sp.empty: continue
            ax.plot(sp.index, sp.values, marker="osD^vP"[k], color=PAL[k % len(PAL)], lw=2, ms=5, label=names[tag])
            md.append(f"| {names[tag]} | " + " | ".join(f"{sp.get(n, float('nan')):.2f}" for n in sorted(ref.index)) + " |")
        md.append("")
        ax.set_xscale("log", base=2); ax.set_yscale("log")
        ax.set_xticks(sorted(ref.index), [str(n) for n in sorted(ref.index)])
        ax.set_xlabel("number of beats N (pairs = N(N-1)/2)", fontsize=9); ax.set_title(f"length {L}, {p} threads", fontsize=9, color=INK); style_ax(ax)
    axes[0][0].set_ylabel("speedup vs serial (diagonal order)", fontsize=9); axes[0][0].legend(frameon=False, fontsize=7)
    save(fig, out, "fig_nsweep.png")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", default="results"); ap.add_argument("--out", default="results/analysis_ext")
    a = ap.parse_args(); os.makedirs(a.out, exist_ok=True)
    md = ["# PulseWarp extension results", ""]
    df = read(a.dir, "affinity");  affinity(df, a.out, md) if df is not None else None
    df = read(a.dir, "weak");      weak(df, a.out, md) if df is not None else None
    df = read(a.dir, "nsweep");    nsweep(df, a.out, md) if df is not None else None
    open(os.path.join(a.out, "ext_summary.md"), "w").write("\n".join(md) + "\n")
    print("wrote", a.out)


if __name__ == "__main__":
    main()
