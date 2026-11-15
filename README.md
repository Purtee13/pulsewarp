# PulseWarp: Parallel Pairwise DTW for ECG Heartbeats

CSC 548 Fall 2026 — Purtee Mahajan and Yash Dhavale

PulseWarp computes all-pairs Dynamic Time Warping (DTW) distances for `N`
heartbeat sequences, requiring `N(N-1)/2` pairwise DTW computations. It compares
two OpenMP parallelization strategies:

| | Strategy A | Strategy B |
|---|---|---|
| Parallel unit | Whole heartbeat pairs | Cells on one anti-diagonal |
| Parallelism | Across independent DTW computations | Within a DTW computation |
| Main overhead | Scheduling pair iterations | A barrier between anti-diagonals |

## Repository layout

```text
src/pulsewarp.h   API
src/data.c        MIT-BIH CSV loader, synthetic beats, and linear resampling
src/dtw.c         Row-order reference, diagonal-order, wavefront, and tiled kernels
src/allpairs.c    All-pairs drivers for the strategies
src/main.c        CLI, timing, golden-vector verification, and CSV output
tests/test_dtw.c  Unit tests for DTW variants
scripts/          SLURM templates, sweeps, data download, and analysis
data/             Put mitbih_train.csv here; see data/README.md
results/          Sweep outputs, golden vectors, logs, and analysis results
```

## Strategies (`--strategy`)

| Name | Description |
|---|---|
| `serial` | Serial pair loop using row-order DTW; the golden reference and row-order serial baseline. |
| `A` | OpenMP `parallel for schedule(runtime)` over the outer heartbeat index; choose `--schedule static`, `dynamic`, or `guided` (optionally with a chunk). |
| `Aflat` | Strategy A over one flattened iteration space containing all pairs. |
| `Adiag` | Strategy A using the diagonal-order DTW kernel for each pair. |
| `Adflat` | Flattened Strategy A using the diagonal-order DTW kernel. |
| `Bdiag` | Serial diagonal-order DTW; checks the diagonal reordering and supplies its serial baseline. |
| `B` | Wavefront DTW: one parallel region, an OpenMP loop per anti-diagonal, and an implicit barrier between diagonals. |
| `Btiled` | Tiled wavefront DTW using `--tile T` and dynamic scheduling over block-diagonals. |

All variants use the same recurrence and arithmetic order where applicable.
The measured sweep passed exact golden-vector verification for all 260 rows.

## Build and test

```bash
module load gcc          # on ARC
make                     # builds ./pulsewarp
make test                # runs unit tests
```

## Run one configuration

```bash
./pulsewarp --input data/mitbih_train.csv --n 300 --len 256 --strategy serial \
  --out results/golden/golden_N300_L256.bin

./pulsewarp --input data/mitbih_train.csv --n 300 --len 256 --strategy A \
  --schedule dynamic --threads 16 --reps 5 \
  --golden results/golden/golden_N300_L256.bin --header
```

Each run prints one CSV row with the strategy, schedule, tile size, thread
count, input dimensions, timing statistics, checksum, and verification result.
Only the all-pairs computation is timed; data loading and resampling are not.
Use `--synthetic` instead of `--input` for a quick run on generated ECG-like
beats.

Makefile shortcuts include `make golden` to generate serial golden vectors and
`make verify` to compare variants against them. Both accept `DATA`, `N`, and
`LENGTHS` overrides.

## Main ARC sweep

The main sweep is complete. It ran on ARC node `c0` with an AMD EPYC 9115
(16 physical cores, no SMT, one NUMA node), GCC 12.4.0, and
`OMP_PROC_BIND=close OMP_PLACES=cores`. The run used `N=300`, sequence lengths
64, 128, 256, 512, and 1024, thread counts 1, 2, 4, 8, and 16, and five timed
repetitions per configuration. All 260 rows passed exact golden-vector
verification.

The source files and scripts can be rebuilt on ARC with `make`. To reproduce
the sweep, first check that the requested partition and CPU allocation match
the SLURM settings in `scripts/sweep.slurm`, then submit:

```bash
sbatch scripts/sweep.slurm
squeue -u "$USER"
```

The sweep script accepts overrides through its environment, for example:

```bash
sbatch --export=ALL,N=200,REPS=3 scripts/sweep.slurm
```

The existing main sweep is `results/sweep_341380_N300.csv`; its environment
record is `results/sweep_341380_N300.env.txt`. The per-thread profile used in
the load-imbalance figure is
`results/profile_A_static_L256_p16.csv`.

## Analysis output (`scripts/analyze.py`)

Regenerate the tables and figures locally or on ARC from the existing sweep:

```bash
python3 scripts/analyze.py results/sweep_341380_N300.csv \
  --profile results/profile_A_static_L256_p16.csv \
  --out results/analysis
```

Speedup is `T_serial / T_p`, and efficiency is speedup divided by the thread
count. Row-order A/Aflat variants and `Btiled` use the row-order serial
baseline; `Btiled` computes the cells within each tile in row order. Diagonal
kernel variants and wavefront `B` use `Bdiag`, the diagonal-order serial
baseline. The separate `self_speedup` column and `fig_self_speedup.png` show
scaling against each variant's own one-thread run. `analyze.py` refuses to combine
overlapping configurations from multiple sweep files unless
`--merge-repeats` is explicitly supplied.

The analysis produces:

- `summary.csv` and `summary.md`: runtimes, matched-serial speedup and
  efficiency, same-variant scaling, and the fastest A and B variant by
  length/thread configuration.
- `fig_runtime.png`, `fig_speedup.png`, `fig_efficiency.png`, and
  `fig_self_speedup.png`: timing and scaling plots by sequence length.
- `fig_ratio_B_over_A*.png` and `fig_ratio_Btiled_over_A*.png`: B/A runtime
  ratio heatmaps. For matched kernels, use `fig_ratio_Btiled_over_A.png`
  (row-order kernels) and `fig_ratio_B_over_Adiag.png` (diagonal-order
  kernels). Ratios above 1 mean A is faster.
- `fig_crossover.png`: speedup against both serial baselines at selected thread
  counts and lengths.
- `fig_load_imbalance.png`: per-thread assigned work and time; requires
  `--profile`.
- `fig_correctness.png` and `correctness.csv`: verification coverage and
  maximum relative error.
- `fig_serial_vs_length.png`: both serial kernels and an `O(L²)` reference.

## Main-sweep findings

These results describe one node and one dataset. They should not be generalized
to other CPUs without further measurements.

- Dynamic scheduling for Strategy A and the flattened A variants reached
  roughly 15–16x speedup at 16 threads. Static and guided scheduling over the
  outer triangular loop reached about 8x; the profile shows a substantial
  imbalance in the number of pairs assigned to threads.
- The diagonal-order serial kernel is substantially faster than row-order
  serial on this CPU (at `L=256`, 0.574 s versus 4.768 s). Therefore, speedup
  comparisons should use each variant's matching serial baseline, and the
  diagonal-kernel advantage should be reported separately from parallel
  scaling.
- For the tested `N=300` configurations, the fastest A variant beat the
  fastest B variant at every measured length and thread count. The extension
  experiments below have not been run, so this finding is limited to the main
  sweep's workload.
- Wavefront B at one thread ran at 0.13–0.52x the diagonal-order serial baseline. Tiled B reached about 7.1x row-order serial at L=1024, p=8, then fell to about 4.8x at p=16. Against Strategy A with the same row-order kernel, tiled B was faster in 8 of 25 configurations (mostly p<=4 at L>=512; ratio 0.73–0.89), partly because tiling already speeds up the kernel at one thread (1.38x at L=1024). At p=16 A is 3–24x faster. No B variant beat the diagonal-kernel A variants in any configuration.

## Optional extension experiments

These experiments are follow-up work and are **not included in the main-sweep
results**. They are not required to reproduce the results above. If run later,
`scripts/sweep_ext.slurm` submits them and `scripts/analyze_ext.py` analyzes the
extension output.

| Mode | Question |
|---|---|
| `nsweep` | With few pairs (`N=2..32`) and long sequences (`L=1024, 4096`), can B overtake A? Includes an active-wait B run. |
| `weak` | How does performance change as work grows with thread count? |
| `affinity` | How do `OMP_PROC_BIND` and `OMP_PLACES` settings affect performance? |
| `lengths` | How do the proposal's additional lengths 32 and 187 perform? |

Do not cite extension results unless these experiments have actually been run
and analyzed.
