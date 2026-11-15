#!/bin/bash
# Extension experiments (affinity, weak scaling, N-sweep, lengths 32/187)
# on one ARC node.   Submit from the repo root:   sbatch scripts/sweep_ext.slurm
# Use the SAME partition/node type as the main sweep (EPYC 9115, 16 cores, no SMT: node c0),
# so the numbers are comparable.  Run one mode only:  sbatch --export=ALL,MODES=nsweep scripts/sweep_ext.slurm
#SBATCH --job-name=pulsewarp-ext
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=16
#SBATCH --exclusive
#SBATCH --time=04:00:00
#SBATCH --output=results/logs/ext_%j.out
#SBATCH --error=results/logs/ext_%j.err
##SBATCH --partition=CHANGE_ME

module load gcc 2>/dev/null || true
cd "$SLURM_SUBMIT_DIR"
make clean && make
make test
bash scripts/sweep_ext.sh ${MODES:-all}
