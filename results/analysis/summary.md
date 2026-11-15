# PulseWarp results

## Serial baselines (s)

| length | row order | diagonal order |
|---:|---:|---:|
| 64 | 0.229 | 0.061 |
| 128 | 1.098 | 0.164 |
| 256 | 4.768 | 0.574 |
| 512 | 19.835 | 2.383 |
| 1024 | 80.897 | 9.119 |

## Speedup vs same strategy at 1 thread (efficiency in brackets)

Speedup and efficiency use the same kernel at one thread as the baseline.
The row-order serial baseline is available separately in `summary.csv` as `speedup_vs_row_serial`.

### length 64

| variant | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| Strategy A, static | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.19 (0.52) | 7.93 (0.50) |
| Strategy A, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.95 (0.99) | 15.53 (0.97) |
| Strategy A, guided | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.52) | 7.85 (0.49) |
| Strategy A flat, static | 1.00 (1.00) | 1.99 (1.00) | 3.98 (1.00) | 7.93 (0.99) | 15.44 (0.96) |
| Strategy A flat, dynamic | 1.00 (1.00) | 1.99 (1.00) | 3.97 (0.99) | 7.91 (0.99) | 14.85 (0.93) |
| Strategy A flat, guided | 1.00 (1.00) | 1.99 (1.00) | 3.98 (1.00) | 7.93 (0.99) | 15.47 (0.97) |
| Strategy A, diag kernel | 1.00 (1.00) | 1.94 (0.97) | 3.74 (0.94) | 7.09 (0.89) | 11.42 (0.71) |
| Strategy A flat, diag kernel | 1.00 (1.00) | 1.94 (0.97) | 3.81 (0.95) | 7.09 (0.89) | 10.39 (0.65) |
| Strategy B, wavefront | 1.00 (1.00) | 0.68 (0.34) | 0.39 (0.10) | 0.27 (0.03) | 0.09 (0.01) |
| Strategy B, tiled | 1.00 (1.00) | 0.97 (0.48) | 0.93 (0.23) | 0.88 (0.11) | 0.66 (0.04) |

### length 128

| variant | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| Strategy A, static | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.07 (0.50) |
| Strategy A, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (1.00) | 15.89 (0.99) |
| Strategy A, guided | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.07 (0.50) |
| Strategy A flat, static | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (1.00) | 15.85 (0.99) |
| Strategy A flat, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (1.00) | 15.78 (0.99) |
| Strategy A flat, guided | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (1.00) | 15.86 (0.99) |
| Strategy A, diag kernel | 1.00 (1.00) | 1.99 (1.00) | 3.83 (0.96) | 7.13 (0.89) | 12.23 (0.76) |
| Strategy A flat, diag kernel | 1.00 (1.00) | 1.98 (0.99) | 3.78 (0.94) | 6.92 (0.87) | 11.89 (0.74) |
| Strategy B, wavefront | 1.00 (1.00) | 0.61 (0.30) | 0.39 (0.10) | 0.27 (0.03) | 0.10 (0.01) |
| Strategy B, tiled | 1.00 (1.00) | 1.27 (0.64) | 1.25 (0.31) | 1.21 (0.15) | 0.94 (0.06) |

### length 256

| variant | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| Strategy A, static | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.11 (0.51) |
| Strategy A, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (0.99) | 15.68 (0.98) |
| Strategy A, guided | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.19 (0.52) | 8.11 (0.51) |
| Strategy A flat, static | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.93 (0.99) | 15.68 (0.98) |
| Strategy A flat, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (0.99) | 15.54 (0.97) |
| Strategy A flat, guided | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.96 (0.99) | 15.55 (0.97) |
| Strategy A, diag kernel | 1.00 (1.00) | 1.99 (1.00) | 3.89 (0.97) | 6.61 (0.83) | 12.96 (0.81) |
| Strategy A flat, diag kernel | 1.00 (1.00) | 2.02 (1.01) | 3.74 (0.94) | 6.56 (0.82) | 12.63 (0.79) |
| Strategy B, wavefront | 1.00 (1.00) | 0.70 (0.35) | 0.43 (0.11) | 0.30 (0.04) | 0.11 (0.01) |
| Strategy B, tiled | 1.00 (1.00) | 1.54 (0.77) | 2.11 (0.53) | 2.03 (0.25) | 1.38 (0.09) |

### length 512

| variant | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| Strategy A, static | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.11 (0.51) |
| Strategy A, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.76 (0.98) |
| Strategy A, guided | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.12 (0.51) |
| Strategy A flat, static | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.74 (0.98) |
| Strategy A flat, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.77 (0.99) |
| Strategy A flat, guided | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.77 (0.99) |
| Strategy A, diag kernel | 1.00 (1.00) | 1.92 (0.96) | 3.60 (0.90) | 6.48 (0.81) | 11.88 (0.74) |
| Strategy A flat, diag kernel | 1.00 (1.00) | 2.12 (1.06) | 3.95 (0.99) | 6.98 (0.87) | 12.85 (0.80) |
| Strategy B, wavefront | 1.00 (1.00) | 0.85 (0.42) | 0.58 (0.14) | 0.37 (0.05) | 0.14 (0.01) |
| Strategy B, tiled | 1.00 (1.00) | 1.74 (0.87) | 2.76 (0.69) | 3.75 (0.47) | 2.15 (0.13) |

### length 1024

| variant | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| Strategy A, static | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.52) | 8.11 (0.51) |
| Strategy A, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.83 (0.99) |
| Strategy A, guided | 1.00 (1.00) | 1.33 (0.67) | 2.28 (0.57) | 4.20 (0.53) | 8.11 (0.51) |
| Strategy A flat, static | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.76 (0.98) |
| Strategy A flat, dynamic | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.76 (0.98) |
| Strategy A flat, guided | 1.00 (1.00) | 2.00 (1.00) | 3.99 (1.00) | 7.97 (1.00) | 15.78 (0.99) |
| Strategy A, diag kernel | 1.00 (1.00) | 2.02 (1.01) | 3.86 (0.96) | 6.71 (0.84) | 12.29 (0.77) |
| Strategy A flat, diag kernel | 1.00 (1.00) | 2.04 (1.02) | 3.85 (0.96) | 6.67 (0.83) | 12.30 (0.77) |
| Strategy B, wavefront | 1.00 (1.00) | 1.05 (0.53) | 0.83 (0.21) | 0.52 (0.07) | 0.20 (0.01) |
| Strategy B, tiled | 1.00 (1.00) | 1.87 (0.94) | 3.25 (0.81) | 5.13 (0.64) | 3.51 (0.22) |

## Fastest matched diagonal-kernel variant by length and thread count

This compares Adiag/Adflat with B/Btiled, using the diagonal-order DTW kernel.

| length | p=1 | p=2 | p=4 | p=8 | p=16 |
|---|---:|---:|---:|---:|---:|
| 64 | Adiag-dynamic | Adiag-dynamic | Adiag-dynamic | Adiag-dynamic | Adiag-dynamic |
| 128 | Adflat-dynamic | Adiag-dynamic | Adiag-dynamic | Adiag-dynamic | Adiag-dynamic |
| 256 | Adflat-dynamic | Adflat-dynamic | Adiag-dynamic | Adflat-dynamic | Adiag-dynamic |
| 512 | Adiag-dynamic | Adflat-dynamic | Adflat-dynamic | Adiag-dynamic | Adflat-dynamic |
| 1024 | Adiag-dynamic | Adflat-dynamic | Adiag-dynamic | Adiag-dynamic | Adflat-dynamic |

Verification flags seen: PASS
