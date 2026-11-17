# PulseWarp extension results

## Few beats, long sequences (speedup vs diagonal-order serial at the same N, L)

### length 1024, p=16

| variant | N=2 | N=3 | N=4 | N=6 | N=8 | N=16 | N=32 |
|---|---:|---:|---:|---:|---:|---:|---:|
| A, outer-index loop | 1.01 | 1.51 | 1.84 | 2.80 | 4.04 | 6.82 | 12.82 |
| A, flattened pairs | 0.99 | 2.91 | 4.68 | 13.08 | 11.81 | 11.43 | 12.81 |
| B wavefront | 0.10 | 0.12 | 0.10 | 0.11 | 0.11 | 0.10 | 0.11 |
| B wavefront, OMP_WAIT_POLICY=active | 0.10 | 0.11 | 0.10 | 0.10 | 0.11 | 0.10 | 0.11 |
| B tiled (64) | 0.18 | 0.58 | 0.52 | 0.53 | 0.56 | 0.49 | 0.58 |
| B tiled (128) | 0.51 | 0.55 | 0.47 | 0.49 | 0.51 | 0.46 | 0.50 |

### length 4096, p=16

| variant | N=2 | N=3 | N=4 | N=6 | N=8 | N=16 | N=32 |
|---|---:|---:|---:|---:|---:|---:|---:|
| A, outer-index loop | 0.97 | 1.52 | 1.98 | 2.91 | 3.68 | 6.85 | 11.89 |
| A, flattened pairs | 1.00 | 3.00 | 5.08 | 10.34 | 10.30 | 11.20 | 11.28 |
| B wavefront | 0.48 | 0.49 | 0.49 | 0.49 | 0.48 | 0.48 | 0.50 |
| B wavefront, OMP_WAIT_POLICY=active | 0.48 | 0.50 | 0.48 | 0.48 | 0.48 | 0.49 | 0.52 |
| B tiled (64) | 0.35 | 0.60 | 0.73 | 0.87 | 0.81 | 0.84 | 0.90 |
| B tiled (128) | 0.38 | 0.65 | 0.76 | 0.85 | 0.86 | 0.90 | 0.95 |
