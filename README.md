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
| `Aflat` | Strategy A over one flattened iteration space containing every pair |
| `Adiag` | Strategy A, but each pair uses the diagonal-order kernel (see note below) |
| `Adflat` | Flattened Strategy A using the diagonal-order DTW kernel |
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
sinfo -o "%P %c %N"                     # inspect cores available per partition
#   edit the SBATCH partition line in scripts/sweep.slurm for your allocation
sbatch scripts/sweep.slurm              # full sweep, roughly 1-3 h at N=300
squeue -u $USER
make analyze PROFILE=results/profile_A_static_L256_p16.csv
DATA=data/mitbih_train.csv N=300 LENGTH=256 THREADS=16 \
  SCHEDULE=static scripts/profile.sh         # -> per-thread profile CSV
python3 scripts/analyze.py results/sweep_*.csv --profile results/profile_A_static_L256_p16.csv
```

The sweep covers lengths 64..1024 x threads 1..16 by default x {A/Aflat static,
dynamic and guided; Adiag/Adflat; B; Btiled} plus both serial baselines, median
of 5 reps, and checks
every run against the golden vector. Override with env vars, e.g.
`sbatch --export=ALL,N=200,REPS=3 scripts/sweep.slurm`. Increase the thread
list with `THREADS="1 2 4 8 16 32"` only when your SLURM allocation provides
at least that many CPUs.
`scripts/pulsewarp.slurm` is a single-run template for debugging one config.

## Analysis output (`scripts/analyze.py`)

- `summary.csv` / `summary.md`: runtime, speedup and efficiency against the
  same strategy at one thread, end-to-end speedup against row-order serial,
  and which of A / B wins at each (length, threads)
- `fig_runtime.png`, `fig_speedup.png`, `fig_efficiency.png`,
  `fig_self_speedup.png`: one panel per length
- `fig_ratio_B_over_A*.png`, `fig_ratio_Btiled_over_A*.png`: heatmaps of
  T_B / T_A using both the row-order A kernel and the diagonal-order A kernel
  (the latter controls for the diagonal kernel's vectorization advantage)
- `fig_crossover.png`: same-kernel scaling and observed B/A runtime ratio
- `fig_load_imbalance.png`: per-thread work time and pair count (requires `--profile`)
- `fig_correctness.png` / `correctness.csv`: verification coverage and maximum error
- `fig_serial_vs_length.png`: both serial kernels against O(L²)

## Extension experiments (`scripts/sweep_ext.sh`)

Not yet run on ARC. Same node type as the main sweep.

```bash
sbatch scripts/sweep_ext.slurm                      # all modes
sbatch --export=ALL,MODES=nsweep scripts/sweep_ext.slurm
python3 scripts/analyze_ext.py                      # -> results/analysis_ext/
# fold the extra lengths into the main plots (configurations do not overlap):
make analyze ANALYSIS_INPUTS="results/sweep_341380_N300.csv results/ext_lengths_<jobid>_N300.csv"
```

| mode | question |
|---|---|
| `nsweep` | few beats (N=2..32), long sequences (L=1024, 4096), 16 threads: once A has fewer pairs than threads, can B overtake it? Also B with `OMP_WAIT_POLICY=active` |
| `weak` | work grows with p: A with N ~ sqrt(p); B with L ~ sqrt(p) |
| `affinity` | OMP_PROC_BIND / OMP_PLACES: false, close, spread x cores, threads |
| `lengths` | lengths 32 and 187 (native) from the locked proposal scope |

## Interpreting the diagonal kernel and measured results

The row-order kernel is latency bound: each cell waits on its left neighbour.
In diagonal order the cells of a diagonal are independent, so GCC can
vectorise the inner loop. On the included sweep, the serial diagonal kernel
(`Bdiag`) is about 8x faster than row-order serial (L=256: 0.574 s vs 4.768 s;
L=1024: 9.12 s vs 80.9 s). That is why `Adiag`/`Adflat` are separate variants
and why the analysis distinguishes speedup against the same strategy at one
thread from end-to-end speedup against row-order serial.

The included sweep (`results/sweep_341380_N300.*`) is one run on node `c0`:
AMD EPYC 9115, 16 physical cores, no SMT, one NUMA node, GCC 12.4.0,
`OMP_PROC_BIND=close OMP_PLACES=cores`, N=300, lengths 64..1024, threads
1..16, five timed repetitions per configuration. Every row (260) passed exact
golden-vector verification. It includes `Aflat`/`Adflat` and guided scheduling
and was used to produce `results/analysis/` and the per-thread profile
`results/profile_A_static_L256_p16.csv`. An earlier sweep from a different CPU
(EPYC 7302P) is kept in `results/archive/` for reference only; `analyze.py`
refuses to average sweeps from different files unless `--merge-repeats` is given.

Observed in this run (single node, single dataset): Strategy A with dynamic
scheduling, or any schedule on the flattened loop, reaches about 15-16x at
16 threads; static and guided on the outer-index loop stall near 8x because
the triangular loop gives thread 0 about 36x the pairs of thread 15 (see the
profile). The diagonal-kernel variants scale to roughly 10-13x at 16 threads.
Strategy B (wavefront) is slower than its own 1-thread run in every
configuration with p >= 2 except L=1024 at p=2 (1.05x); the tiled version helps only at the longest lengths and still does not
beat Strategy A with the same kernel.
