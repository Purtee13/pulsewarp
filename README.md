# PulseWarp: Parallel Pairwise DTW for ECG Heartbeats

CSC 548 Fall 2026, Purtee Mahajan and Yash Dhavale.

All-pairs Dynamic Time Warping over N ECG heartbeats (N(N-1)/2 DTWs),
accelerated with OpenMP two ways:

| | Strategy A | Strategy B |
|---|---|---|
| split unit | whole heartbeat pairs | cells of one anti-diagonal |
| parallelism | across DTW computations | inside one DTW |
| overhead | almost none | one barrier per diagonal (2L per DTW) |

## Layout

```
src/pulsewarp.h   API
src/data.c        MIT-BIH CSV loader, synthetic beats, linear resampling
src/dtw.c         DTW kernels: row order (reference), diagonal order,
                  wavefront (omp for per anti-diagonal), tiled wavefront
src/allpairs.c    all-pairs drivers for every strategy
src/main.c        CLI, timing (median of reps), golden write/verify, CSV row
tests/test_dtw.c  unit tests (hand-checked values, all variants bit-identical)
scripts/          SLURM templates, sweep, data download, analysis/plots
data/             put mitbih_train.csv here (see data/README.md)
results/          golden vectors, sweep CSVs, logs, analysis output
```

## Strategies (`--strategy`)

| name | what it is |
|---|---|
| `serial` | row-order DTW, serial pair loop. **The golden reference and T_serial.** |
| `A` | `omp parallel for schedule(runtime)` over the outer beat index; `--schedule static\|dynamic\|guided[,chunk]` |
| `Adiag` | Strategy A, but each pair uses the diagonal-order kernel (see note below) |
| `Bdiag` | Strategy B step 1: diagonal-order DTW, serial. Verifies the restructuring. |
| `B` | wavefront: one parallel region for the whole run, `omp for` over each anti-diagonal, implicit barrier between diagonals |
| `Btiled` | tiled wavefront: `--tile T` blocks, `omp for schedule(dynamic,1)` over each block-diagonal |

All variants use the same cell update in the same argument order, so their
distances are **bit-identical** to the serial reference (max_rel_err = 0).

## Build and test

```bash
module load gcc          # on ARC
make                     # builds ./pulsewarp
make test                # unit tests
```

## Run

```bash
./pulsewarp --input data/mitbih_train.csv --n 300 --len 256 --strategy serial \
            --out results/golden/golden_N300_L256.bin            # golden reference
./pulsewarp --input data/mitbih_train.csv --n 300 --len 256 --strategy A \
            --schedule dynamic --threads 16 --reps 5 \
            --golden results/golden/golden_N300_L256.bin --header
```

Every run prints one CSV row: `strategy,schedule,tile,threads,n,len,pairs,reps,
median_s,min_s,max_s,checksum,verify,max_rel_err,times`. Only the all-pairs
computation is timed (`omp_get_wtime`), not loading or resampling.
`--synthetic` replaces `--input` with generated ECG-like beats for quick tests.

Makefile shortcuts: `make golden` (serial at every length), `make verify`
(every strategy against golden), both take `DATA=... N=... LENGTHS="..."`.

## On ARC

```bash
ssh <unityid>@arc.csc.ncsu.edu          # on the NCSU VPN
# copy this folder over (scp/git), put data/mitbih_train.csv in place
sinfo -o "%P %c %N"                     # pick a partition with >= 32 cores
#   then edit the  ##SBATCH --partition=  line in scripts/sweep.slurm
sbatch scripts/sweep.slurm              # full sweep, roughly 1-3 h at N=300
squeue -u $USER
python3 scripts/analyze.py results/sweep_*.csv   # -> results/analysis/
```

The sweep covers lengths 64..1024 x threads 1..32 x {A static, A dynamic,
Adiag, B, Btiled} plus both serial baselines, median of 5 reps, and checks
every run against the golden vector. Override with env vars, e.g.
`sbatch --export=ALL,N=200,REPS=3 scripts/sweep.slurm`.
`scripts/pulsewarp.slurm` is a single-run template for debugging one config.

## Analysis output (`scripts/analyze.py`)

- `summary.csv` / `summary.md`: runtime, speedup S_p = T_serial / T_p,
  efficiency S_p / p, speedup vs the variant's own 1-thread run, and which of
  A / B wins at each (length, threads)
- `fig_runtime.png`, `fig_speedup.png`, `fig_efficiency.png`,
  `fig_self_speedup.png`: one panel per length
- `fig_ratio_B_over_A.png`, `fig_ratio_Btiled_over_A.png`: heatmaps of
  T_B / T_A over (length, threads)
- `fig_serial_vs_length.png`: both serial kernels against O(L^2)

## Note: the diagonal kernel is much faster serially

The row-order kernel is latency bound: each cell waits on its left neighbour.
In diagonal order the cells of a diagonal are independent, so GCC vectorises
the inner loop and the *serial* diagonal kernel runs several times faster
(about 8x at L=256 in local tests). That is why `Adiag` exists and why the
analysis reports speedup against both serial baselines. It is worth a
sentence in the report, because it changes what "speedup vs serial" means.
