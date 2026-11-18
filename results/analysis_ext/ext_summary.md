# PulseWarp extension results

## Weak scaling (work per thread constant; efficiency = T(1)/T(p), ideal 1.0)

| experiment | variant | p | n | len | time (s) | efficiency |
|---|---|---:|---:|---:|---:|---:|
| weakA | A | 1 | 212 | 256 | 0.278 | 1.00 |
| weakA | A | 2 | 300 | 256 | 0.278 | 1.00 |
| weakA | A | 4 | 424 | 256 | 0.297 | 0.94 |
| weakA | A | 8 | 600 | 256 | 0.349 | 0.80 |
| weakA | A | 16 | 848 | 256 | 0.384 | 0.72 |
| weakA | A-rowkernel | 1 | 212 | 256 | 2.378 | 1.00 |
| weakA | A-rowkernel | 2 | 300 | 256 | 2.386 | 1.00 |
| weakA | A-rowkernel | 4 | 424 | 256 | 2.391 | 0.99 |
| weakA | A-rowkernel | 8 | 600 | 256 | 2.398 | 0.99 |
| weakA | A-rowkernel | 16 | 848 | 256 | 2.449 | 0.97 |
| weakB | Adflat | 1 | 40 | 256 | 0.009 | 1.00 |
| weakB | Adflat | 2 | 40 | 362 | 0.009 | 1.04 |
| weakB | Adflat | 4 | 40 | 512 | 0.011 | 0.84 |
| weakB | Adflat | 8 | 40 | 724 | 0.010 | 0.92 |
| weakB | Adflat | 16 | 40 | 1024 | 0.013 | 0.71 |
| weakB | B | 1 | 40 | 256 | 0.037 | 1.00 |
| weakB | B | 2 | 40 | 362 | 0.079 | 0.47 |
| weakB | B | 4 | 40 | 512 | 0.183 | 0.20 |
| weakB | B | 8 | 40 | 724 | 0.374 | 0.10 |
| weakB | B | 16 | 40 | 1024 | 1.464 | 0.03 |
| weakB | Btiled | 1 | 40 | 256 | 0.063 | 1.00 |
| weakB | Btiled | 2 | 40 | 362 | 0.073 | 0.87 |
| weakB | Btiled | 4 | 40 | 512 | 0.093 | 0.69 |
| weakB | Btiled | 8 | 40 | 724 | 0.117 | 0.54 |
| weakB | Btiled | 16 | 40 | 1024 | 0.303 | 0.21 |

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
