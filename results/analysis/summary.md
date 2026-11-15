# PulseWarp results

## Serial baselines (s)

| length | row order | diagonal order |
|---:|---:|---:|
| 64 | 0.229 | 0.061 |
| 128 | 1.098 | 0.164 |
| 256 | 4.768 | 0.574 |
| 512 | 19.835 | 2.383 |
| 1024 | 80.897 | 9.119 |

## Speedup vs matching serial baseline (efficiency in brackets)

Speedup is T_serial / T_p; efficiency is speedup / thread count.
Row-order A variants and tiled B (row-order cells inside each tile) use row-order serial. Diagonal-kernel A variants and wavefront B use diagonal-order serial (Bdiag).
Scaling against each variant's own one-thread run is available in `summary.csv` as `self_speedup` and in `fig_self_speedup.png`.

### length 64

| variant | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| Strategy A, static | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.52) | 7.94 (0.50) |
| Strategy A, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (0.99) | 15.54 (0.97) |
| Strategy A, guided | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.52) | 7.85 (0.49) |
| Strategy A flat, static | 1.00 (1.00) | 2.00 (1.00) | 3.98 (1.00) | 7.93 (0.99) | 15.44 (0.97) |
| Strategy A flat, dynamic | 1.00 (1.00) | 1.99 (0.99) | 3.97 (0.99) | 7.90 (0.99) | 14.84 (0.93) |
| Strategy A flat, guided | 1.00 (1.00) | 2.00 (1.00) | 3.98 (1.00) | 7.93 (0.99) | 15.48 (0.97) |
| Strategy A, diag kernel | 1.07 (1.07) | 2.08 (1.04) | 4.01 (1.00) | 7.60 (0.95) | 12.24 (0.77) |
| Strategy A flat, diag kernel | 1.04 (1.04) | 2.01 (1.01) | 3.95 (0.99) | 7.35 (0.92) | 10.78 (0.67) |
| Strategy B, wavefront | 0.13 (0.13) | 0.09 (0.04) | 0.05 (0.01) | 0.03 (0.00) | 0.01 (0.00) |
| Strategy B, tiled | 0.97 (0.97) | 0.94 (0.47) | 0.91 (0.23) | 0.86 (0.11) | 0.64 (0.04) |

### length 128

| variant | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| Strategy A, static | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.07 (0.50) |
| Strategy A, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (1.00) | 15.89 (0.99) |
| Strategy A, guided | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.52) | 8.07 (0.50) |
| Strategy A flat, static | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (1.00) | 15.85 (0.99) |
| Strategy A flat, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (0.99) | 15.77 (0.99) |
| Strategy A flat, guided | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (1.00) | 15.86 (0.99) |
| Strategy A, diag kernel | 0.98 (0.98) | 1.96 (0.98) | 3.76 (0.94) | 7.01 (0.88) | 12.02 (0.75) |
| Strategy A flat, diag kernel | 0.99 (0.99) | 1.96 (0.98) | 3.73 (0.93) | 6.84 (0.85) | 11.75 (0.73) |
| Strategy B, wavefront | 0.16 (0.16) | 0.10 (0.05) | 0.06 (0.02) | 0.04 (0.01) | 0.02 (0.00) |
| Strategy B, tiled | 1.19 (1.19) | 1.51 (0.76) | 1.48 (0.37) | 1.44 (0.18) | 1.11 (0.07) |

### length 256

| variant | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| Strategy A, static | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.11 (0.51) |
| Strategy A, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (0.99) | 15.68 (0.98) |
| Strategy A, guided | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.19 (0.52) | 8.11 (0.51) |
| Strategy A flat, static | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.93 (0.99) | 15.68 (0.98) |
| Strategy A flat, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (0.99) | 15.54 (0.97) |
| Strategy A flat, guided | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (0.99) | 15.56 (0.97) |
| Strategy A, diag kernel | 1.01 (1.01) | 2.01 (1.00) | 3.92 (0.98) | 6.66 (0.83) | 13.06 (0.82) |
| Strategy A flat, diag kernel | 1.02 (1.02) | 2.06 (1.03) | 3.81 (0.95) | 6.69 (0.84) | 12.87 (0.80) |
| Strategy B, wavefront | 0.25 (0.25) | 0.17 (0.09) | 0.11 (0.03) | 0.08 (0.01) | 0.03 (0.00) |
| Strategy B, tiled | 1.31 (1.31) | 2.02 (1.01) | 2.75 (0.69) | 2.65 (0.33) | 1.81 (0.11) |

### length 512

| variant | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| Strategy A, static | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.11 (0.51) |
| Strategy A, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (1.00) | 15.76 (0.98) |
| Strategy A, guided | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.12 (0.51) |
| Strategy A flat, static | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (1.00) | 15.74 (0.98) |
| Strategy A flat, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.76 (0.99) |
| Strategy A flat, guided | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.77 (0.99) |
| Strategy A, diag kernel | 1.02 (1.02) | 1.96 (0.98) | 3.68 (0.92) | 6.61 (0.83) | 12.13 (0.76) |
| Strategy A flat, diag kernel | 0.95 (0.95) | 2.01 (1.01) | 3.75 (0.94) | 6.61 (0.83) | 12.18 (0.76) |
| Strategy B, wavefront | 0.40 (0.40) | 0.34 (0.17) | 0.23 (0.06) | 0.15 (0.02) | 0.06 (0.00) |
| Strategy B, tiled | 1.35 (1.35) | 2.35 (1.18) | 3.73 (0.93) | 5.06 (0.63) | 2.91 (0.18) |

### length 1024

| variant | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| Strategy A, static | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.12 (0.51) |
| Strategy A, dynamic | 1.00 (1.00) | 2.00 (1.00) | 4.00 (1.00) | 7.97 (1.00) | 15.83 (0.99) |
| Strategy A, guided | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.11 (0.51) |
| Strategy A flat, static | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.76 (0.98) |
| Strategy A flat, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.76 (0.98) |
| Strategy A flat, guided | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.78 (0.99) |
| Strategy A, diag kernel | 0.95 (0.95) | 1.93 (0.96) | 3.68 (0.92) | 6.39 (0.80) | 11.71 (0.73) |
| Strategy A flat, diag kernel | 0.95 (0.95) | 1.94 (0.97) | 3.66 (0.92) | 6.35 (0.79) | 11.71 (0.73) |
| Strategy B, wavefront | 0.52 (0.52) | 0.55 (0.28) | 0.43 (0.11) | 0.27 (0.03) | 0.11 (0.01) |
| Strategy B, tiled | 1.38 (1.38) | 2.58 (1.29) | 4.48 (1.12) | 7.05 (0.88) | 4.83 (0.30) |

## Best Strategy A variant vs best Strategy B variant per configuration

Cell: winner (best time of the other strategy / winner time). Strategy A candidates include the
diagonal-kernel variants (Adiag, Adflat), so the result is not decided by the kernel alone.
Same-kernel pairs: A-dynamic/Aflat vs Btiled (row-order cells), Adiag/Adflat vs B (diagonal order).

| length | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| 64 | A: Adiag-dynamic (4.1x) | A: Adiag-dynamic (8.3x) | A: Adiag-dynamic (16.6x) | A: Adiag-dynamic (33.3x) | A: Adiag-dynamic (71.6x) |
| 128 | A: Adflat-dynamic (5.6x) | A: Adiag-dynamic (8.7x) | A: Adiag-dynamic (17.0x) | A: Adiag-dynamic (32.7x) | A: Adiag-dynamic (72.5x) |
| 256 | A: Adflat-dynamic (4.1x) | A: Adflat-dynamic (8.5x) | A: Adiag-dynamic (11.8x) | A: Adflat-dynamic (20.9x) | A: Adiag-dynamic (59.9x) |
| 512 | A: Adiag-dynamic (2.5x) | A: Adflat-dynamic (5.9x) | A: Adflat-dynamic (8.4x) | A: Adiag-dynamic (10.9x) | A: Adflat-dynamic (34.9x) |
| 1024 | A: Adiag-dynamic (1.8x) | A: Adflat-dynamic (3.5x) | A: Adiag-dynamic (7.3x) | A: Adiag-dynamic (8.0x) | A: Adflat-dynamic (21.5x) |

Verification flags seen: PASS
