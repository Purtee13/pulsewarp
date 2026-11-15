#!/usr/bin/env bash
# PulseWarp benchmark sweep: strategy x threads x length, every run verified
# against the serial golden vector. Run on a compute node (via sweep.slurm).
#
# Override any of these from the environment, e.g.  N=200 REPS=3 scripts/sweep.sh
set -euo pipefail
cd "$(dirname "$0")/.."

DATA=${DATA:-data/mitbih_train.csv}
N=${N:-300}
LENGTHS=${LENGTHS:-"64 128 256 512 1024"}
THREADS=${THREADS:-"1 2 4 8 16"}
REPS=${REPS:-5}
TILE=${TILE:-64}
OUT=${OUT:-results/sweep_${SLURM_JOB_ID:-local}_N${N}.csv}

export OMP_PROC_BIND=${OMP_PROC_BIND:-close}
export OMP_PLACES=${OMP_PLACES:-cores}

# Do not launch more OpenMP threads than the allocation can run concurrently.
# Users can still provide THREADS explicitly, but invalid values fail early.
ALLOC_CPUS=${SLURM_CPUS_PER_TASK:-$(getconf _NPROCESSORS_ONLN)}
for T in $THREADS; do
  if [ "$T" -gt "$ALLOC_CPUS" ]; then
    echo "requested THREADS=$T exceeds allocated CPUs ($ALLOC_CPUS); adjust THREADS or request more CPUs" >&2
    exit 1
  fi
done

[ -f "$DATA" ] || { echo "missing $DATA - see data/README.md" >&2; exit 1; }
make -s
mkdir -p results/golden results/logs
echo "strategy,schedule,tile,threads,n,len,pairs,reps,median_s,min_s,max_s,checksum,verify,max_rel_err,times" > "$OUT"

{
  echo "host: $(hostname)"; echo "date: $(date -Is)"; gcc --version | head -1
  lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core|Socket|NUMA node\(s\)" || true
  echo "N=$N LENGTHS=$LENGTHS THREADS=$THREADS REPS=$REPS TILE=$TILE"
  echo "OMP_PROC_BIND=$OMP_PROC_BIND OMP_PLACES=$OMP_PLACES"
} | tee "${OUT%.csv}.env.txt"

run() {   # run <args...> : one timed configuration, appended to $OUT
  ./pulsewarp --input "$DATA" --n "$N" --reps "$REPS" "$@" | tee -a "$OUT"
}

for L in $LENGTHS; do
  G=results/golden/golden_N${N}_L${L}.bin
  # serial reference (also (re)writes the golden vector for this length)
  ./pulsewarp --input "$DATA" --n "$N" --len "$L" --strategy serial --out "$G" > /dev/null
  run --len "$L" --strategy serial --golden "$G"
  run --len "$L" --strategy Bdiag  --golden "$G"
  for T in $THREADS; do
    run --len "$L" --threads "$T" --strategy A     --schedule static  --golden "$G"
    run --len "$L" --threads "$T" --strategy A     --schedule dynamic --golden "$G"
    run --len "$L" --threads "$T" --strategy A     --schedule guided  --golden "$G"
    run --len "$L" --threads "$T" --strategy Aflat --schedule static  --golden "$G"
    run --len "$L" --threads "$T" --strategy Aflat --schedule dynamic --golden "$G"
    run --len "$L" --threads "$T" --strategy Aflat --schedule guided  --golden "$G"
    run --len "$L" --threads "$T" --strategy Adiag --schedule dynamic --golden "$G"
    run --len "$L" --threads "$T" --strategy Adflat --schedule dynamic --golden "$G"
    run --len "$L" --threads "$T" --strategy B                        --golden "$G"
    run --len "$L" --threads "$T" --strategy Btiled --tile "$TILE"    --golden "$G"
  done
done

if grep -qE ",(FAIL|ERROR)," "$OUT"; then echo "WARNING: verification failures in $OUT" >&2; exit 2; fi

# Collect the proposal's per-thread imbalance profile on this same compute
# node. It is a separate pass and is not included in benchmark medians.
PROFILE_LEN=${PROFILE_LEN:-256}
PROFILE_THREADS=${PROFILE_THREADS:-${SLURM_CPUS_PER_TASK:-16}}
PROFILE_OUT=${PROFILE_OUT:-results/profile_A_static_L${PROFILE_LEN}_p${PROFILE_THREADS}.csv}
PROFILE_G=results/golden/golden_N${N}_L${PROFILE_LEN}.bin
if [ ! -f "$PROFILE_G" ]; then
  ./pulsewarp --input "$DATA" --n "$N" --len "$PROFILE_LEN" --strategy serial --out "$PROFILE_G" >/dev/null
fi
./pulsewarp --input "$DATA" --n "$N" --len "$PROFILE_LEN" --strategy A \
  --schedule static --threads "$PROFILE_THREADS" --reps 1 --golden "$PROFILE_G" \
  --profile "$PROFILE_OUT" >/dev/null
echo "done -> $OUT"
