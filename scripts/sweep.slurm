#!/bin/bash
# Full PulseWarp sweep on one ARC node.   Submit from the repo root:
#     sbatch scripts/sweep.slurm
# Pick a partition with >= 32 cores first:  sinfo -o "%P %c %N"
#SBATCH --job-name=pulsewarp-sweep
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --exclusive
#SBATCH --time=06:00:00
#SBATCH --output=results/logs/sweep_%j.out
#SBATCH --error=results/logs/sweep_%j.err
##SBATCH --partition=CHANGE_ME

module load gcc 2>/dev/null || true
cd "$SLURM_SUBMIT_DIR"
make clean && make          # build on the compute node (-march=native)
make test
N=${N:-300} REPS=${REPS:-5} bash scripts/sweep.sh
