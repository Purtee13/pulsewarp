#!/bin/bash
# Template for a single PulseWarp run (handy for debugging one configuration).
#     sbatch --export=ALL,STRAT=B,LEN=512,THREADS=16 scripts/pulsewarp.slurm
#SBATCH --job-name=pulsewarp
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --exclusive
#SBATCH --time=00:30:00
#SBATCH --output=results/logs/run_%j.out
#SBATCH --error=results/logs/run_%j.err
##SBATCH --partition=CHANGE_ME

module load gcc 2>/dev/null || true
cd "$SLURM_SUBMIT_DIR"
make -s

STRAT=${STRAT:-A}; SCHED=${SCHED:-dynamic}; LEN=${LEN:-256}
THREADS=${THREADS:-$SLURM_CPUS_PER_TASK}; N=${N:-300}; REPS=${REPS:-3}
DATA=${DATA:-data/mitbih_train.csv}
G=results/golden/golden_N${N}_L${LEN}.bin
export OMP_PROC_BIND=close OMP_PLACES=cores

[ -f "$G" ] || ./pulsewarp --input "$DATA" --n "$N" --len "$LEN" --strategy serial --out "$G" > /dev/null
./pulsewarp --input "$DATA" --n "$N" --len "$LEN" --strategy "$STRAT" --schedule "$SCHED" \
            --threads "$THREADS" --reps "$REPS" --golden "$G" --header
