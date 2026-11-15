#!/bin/bash
# Full PulseWarp sweep on one ARC node.   Submit from the repo root:
#     sbatch scripts/sweep.slurm
# The sweep defaults to 16 threads. Increase both this allocation and THREADS
# only when the selected node provides more physical cores.
#SBATCH --job-name=pulsewarp-sweep
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=16
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
