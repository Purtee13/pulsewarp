#!/usr/bin/env bash
# Collect a per-thread Strategy-A work profile for the report.
# Example: DATA=data/mitbih_train.csv N=300 LENGTH=256 THREADS=16 \
#          SCHEDULE=static PROFILE=results/profile_Astatic_L256.csv scripts/profile.sh
set -euo pipefail
cd "$(dirname "$0")/.."

DATA=${DATA:-data/mitbih_train.csv}
N=${N:-300}
LENGTH=${LENGTH:-256}
THREADS=${THREADS:-16}
SCHEDULE=${SCHEDULE:-static}
STRATEGY=${STRATEGY:-A}
PROFILE=${PROFILE:-results/profile_${STRATEGY}_${SCHEDULE}_L${LENGTH}_p${THREADS}.csv}
GOLDEN=${GOLDEN:-results/golden/golden_N${N}_L${LENGTH}.bin}

if [ ! -f "$DATA" ]; then echo "missing $DATA - see data/README.md" >&2; exit 1; fi
make -s
if [ ! -f "$GOLDEN" ]; then
  mkdir -p "$(dirname "$GOLDEN")"
  ./pulsewarp --input "$DATA" --n "$N" --len "$LENGTH" --strategy serial --out "$GOLDEN" >/dev/null
fi
mkdir -p "$(dirname "$PROFILE")"
./pulsewarp --input "$DATA" --n "$N" --len "$LENGTH" --strategy "$STRATEGY" \
  --schedule "$SCHEDULE" --threads "$THREADS" --reps 3 --golden "$GOLDEN" \
  --profile "$PROFILE" >/dev/null
echo "profile written to $PROFILE"
