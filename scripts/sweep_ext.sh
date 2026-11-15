#!/usr/bin/env bash
# PulseWarp extension experiments beyond the main sweep (scheduling and the
# per-thread profile are already covered by sweep.sh / profile.sh):
#   affinity  OMP_PROC_BIND / OMP_PLACES variants (false, close/spread x cores/threads)
#   weak      weak scaling: work grows with threads (A: N ~ sqrt(p); B: L ~ sqrt(p))
#   nsweep    crossover regime for B: few beats, long sequences (A runs out of pairs)
#   lengths   the locked-scope lengths missing from the main sweep (32, 187)
#
#   bash scripts/sweep_ext.sh <mode> [mode ...]      # or:  all
# Every run is verified against a serial golden vector. Output: results/ext_<mode>_<jobid>.csv
# with two leading columns (exp,tag) in front of the usual pulsewarp row.
# Env overrides: DATA (path, or "synthetic"), REPS, TMAX (max threads, default 16).
set -euo pipefail
cd "$(dirname "$0")/.."

DATA=${DATA:-data/mitbih_train.csv}
REPS=${REPS:-5}
TMAX=${TMAX:-16}
JOB=${SLURM_JOB_ID:-local}
THREADS=${THREADS:-"1 2 4 8 16"}
export OMP_PROC_BIND=${OMP_PROC_BIND:-close}
export OMP_PLACES=${OMP_PLACES:-cores}

make -s
GD=results/golden_ext; rm -rf "$GD"; mkdir -p "$GD" results/logs   # fresh golden vectors every run (never reuse across datasets)
if [ "$DATA" = synthetic ]; then SRC=(--synthetic); else
  [ -f "$DATA" ] || { echo "missing $DATA" >&2; exit 1; }; SRC=(--input "$DATA"); fi
HDR="exp,tag,strategy,schedule,tile,threads,n,len,pairs,reps,median_s,min_s,max_s,checksum,verify,max_rel_err,times"

golden() {  # golden <n> <len>  -> echoes path, (re)creating it if missing
  local g=$GD/golden_N$1_L$2.bin
  [ -f "$g" ] || ./pulsewarp "${SRC[@]}" --n "$1" --len "$2" --strategy serial --out "$g" >/dev/null 2>&1
  echo "$g"
}

# run <outfile> <exp> <tag> <pulsewarp args...>   (n and len must be among the args)
run() {
  local out=$1 exp=$2 tag=$3; shift 3; tag=${tag//,/:}
  ./pulsewarp "${SRC[@]}" --reps "$REPS" "$@" 2>>"results/logs/ext_${JOB}.err" | awk -v p="$exp,$tag," '{print p $0}' | tee -a "$out"
}
newcsv() { echo "$HDR" > "$1"; }

env_dump() {
  { echo "host: $(hostname)"; echo "date: $(date -Is)"; gcc --version | head -1
    lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core|Socket|NUMA node\(s\)" || true
    echo "DATA=$DATA REPS=$REPS THREADS=$THREADS"; echo "modes: $*"; } > "results/ext_${JOB}.env.txt"
}

mode_affinity() {
  local N=${N:-300} L=${L:-256} out=results/ext_affinity_${JOB}.csv G; newcsv "$out"; G=$(golden $N $L)
  # (proc_bind, places, threads) combinations; 32 = both SMT siblings of 16 cores
  for cfg in "false -" "close cores" "close threads" "spread cores" "spread threads"; do
    set -- $cfg; b=$1; pl=$2
    for T in $TMAX; do
      ( if [ "$b" = false ]; then unset OMP_PLACES; export OMP_PROC_BIND=false
        else export OMP_PROC_BIND=$b OMP_PLACES=$pl; fi
        run "$out" affinity "$b/$pl" --n $N --len $L --threads $T --strategy Adflat --schedule dynamic --golden "$G"
        run "$out" affinity "$b/$pl" --n 60 --len 1024 --threads $T --strategy Btiled --golden "$(golden 60 1024)" )
    done
  done
  # SMT point: 32 threads on 16 cores (only meaningful on a node with 32 logical CPUs)
  if [ "$(nproc)" -ge 32 ]; then
    ( export OMP_PROC_BIND=close OMP_PLACES=threads
      run "$out" affinity "close/threads@32" --n $N --len $L --threads 32 --strategy Adflat --schedule dynamic --golden "$G" )
  fi
}

mode_weak() {  # A: pairs ~ p (N = N1*sqrt(p)); B/Btiled: work per DTW ~ p (L = L1*sqrt(p))
  local out=results/ext_weak_${JOB}.csv; newcsv "$out"
  local N1=${N1:-212} LA=${LA:-256} NB=${NB:-40} LB1=${LB1:-256}
  for T in $THREADS; do
    N=$(python3 -c "import math;print(round($N1*math.sqrt($T)))")
    G=$(golden $N $LA)
    run "$out" weakA "A" --n $N --len $LA --threads $T --strategy Adflat --schedule dynamic --golden "$G"
    run "$out" weakA "A-rowkernel" --n $N --len $LA --threads $T --strategy Aflat --schedule dynamic --golden "$G"
    L=$(python3 -c "import math;print(round($LB1*math.sqrt($T)))")
    G=$(golden $NB $L)
    run "$out" weakB "B"      --n $NB --len $L --threads $T --strategy B      --golden "$G"
    run "$out" weakB "Btiled" --n $NB --len $L --threads $T --strategy Btiled --golden "$G"
    run "$out" weakB "Adflat" --n $NB --len $L --threads $T --strategy Adflat --schedule dynamic --golden "$G"
  done
}

mode_nsweep() {  # few beats + long sequences: does B finally win once A has too few pairs?
  local out=results/ext_nsweep_${JOB}.csv T=$TMAX; newcsv "$out"
  for L in ${NS_LENGTHS:-1024 4096}; do
    for N in ${NS_N:-2 3 4 6 8 16 32}; do
      G=$(golden $N $L)
      run "$out" nsweep "A-outer"   --n $N --len $L --threads $T --strategy Adiag  --schedule dynamic --golden "$G"
      run "$out" nsweep "A-flat"    --n $N --len $L --threads $T --strategy Adflat --schedule dynamic --golden "$G"
      run "$out" nsweep "B"         --n $N --len $L --threads $T --strategy B      --golden "$G"
      run "$out" nsweep "Btiled"    --n $N --len $L --threads $T --strategy Btiled --tile 64  --golden "$G"
      run "$out" nsweep "Btiled128" --n $N --len $L --threads $T --strategy Btiled --tile 128 --golden "$G"
      ( export OMP_WAIT_POLICY=active
        run "$out" nsweep "B+spin"  --n $N --len $L --threads $T --strategy B      --golden "$G" )
      run "$out" nsweep "serial-row"  --n $N --len $L --strategy serial --golden "$G"
      run "$out" nsweep "serial-diag" --n $N --len $L --strategy Bdiag  --golden "$G"
    done
  done
}

mode_lengths() {  # locked-scope lengths 32 and 187 (native); same layout as the main sweep
  local N=${N:-300} out=results/ext_lengths_${JOB}_N${N}.csv
  # PROFILE_OUT=/dev/null: do not overwrite the committed Strategy-A profile
  PROFILE_OUT=/dev/null OUT=$out LENGTHS="${LENGTHS:-32 187}" THREADS="$THREADS" N=$N REPS=$REPS DATA=$DATA bash scripts/sweep.sh
}

modes=("$@"); [ ${#modes[@]} -eq 0 ] && { echo "usage: $0 affinity|weak|nsweep|lengths|all" >&2; exit 1; }
[ "${modes[0]}" = all ] && modes=(affinity weak nsweep lengths)
env_dump "${modes[@]}"
for m in "${modes[@]}"; do echo "== $m" >&2; "mode_$m"; done
if grep -qE ",(FAIL|ERROR)," results/ext_*_${JOB}.csv 2>/dev/null; then echo "WARNING: verification failures" >&2; exit 2; fi
echo "done"
