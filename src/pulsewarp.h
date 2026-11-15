/*
 * PulseWarp: parallel pairwise DTW similarity search for ECG heartbeats.
 * CSC 548 (Fall 2026) - Purtee Mahajan, Yash Dhavale
 */
#ifndef PULSEWARP_H
#define PULSEWARP_H

#include <stddef.h>

/* ------------------------------------------------------------------ data */

typedef struct {
    int     n;        /* number of heartbeats                       */
    int     len;      /* samples per heartbeat (after resampling)   */
    double *x;        /* n * len values, row-major: beat k at x + k*len */
    int    *label;    /* class label per beat (-1 if unknown)       */
} Dataset;

/* Load N beats from the Kaggle "ECG Heartbeat Categorization" MIT-BIH CSV
 * (mitbih_train.csv / mitbih_test.csv: 187 samples + class label per row).
 * Rows are picked with a fixed stride across the whole file so the subset
 * is deterministic and covers every class. Trailing zero padding is trimmed
 * (unless trim==0) and each beat is resampled to `len` samples.
 * Returns 0 on success. */
int  load_mitbih_csv(const char *path, int n, int len, int trim, Dataset *out);

/* Deterministic synthetic ECG-like beats (P wave, QRS, T wave with jitter),
 * used when the real CSV is not available. */
void make_synthetic(int n, int len, unsigned seed, Dataset *out);

/* Linear-interpolation resampling of src[0..src_len) to dst[0..dst_len). */
void resample_linear(const double *src, int src_len, double *dst, int dst_len);

void free_dataset(Dataset *d);

/* ------------------------------------------------------------------- DTW */

/* Reference serial DTW, row-by-row with two rolling rows.
 * D[0][0]=0, D[i][0]=D[0][j]=inf,
 * D[i][j] = |a[i-1]-b[j-1]| + min(D[i-1][j], D[i][j-1], D[i-1][j-1]).
 * work must hold 2*(m+1) doubles. */
double dtw_serial(const double *a, int n, const double *b, int m, double *work);

/* Same recurrence evaluated in anti-diagonal (wavefront) order, serially.
 * work must hold 3*(n+1) doubles. Bit-identical to dtw_serial. */
double dtw_diag_serial(const double *a, int n, const double *b, int m, double *work);

/* Wavefront DTW: MUST be called by every thread of an enclosing
 * `#pragma omp parallel` region (orphaned worksharing). Cells of each
 * anti-diagonal are split with `omp for`; its implicit barrier separates
 * successive diagonals. work (shared) must hold 3*(n+1) doubles. */
double dtw_wavefront(const double *a, int n, const double *b, int m, double *work);

/* Tiled wavefront: the grid is cut into tile x tile blocks; blocks on the
 * same block-anti-diagonal run in parallel, each block serially inside.
 * Called from inside a parallel region like dtw_wavefront.
 * work (shared) must hold (n+1)*(m+1) doubles. */
double dtw_tiled(const double *a, int n, const double *b, int m, int tile, double *work);

/* ------------------------------------------------------------- all-pairs */

typedef enum {
    STRAT_SERIAL = 0,   /* serial all-pairs loop, the golden reference    */
    STRAT_A,            /* Strategy A: omp parallel for over outer rows    */
    STRAT_AFLAT,        /* Strategy A: omp parallel for over flat pairs    */
    STRAT_ADIAG,        /* Strategy A, but each pair uses the diagonal kernel */
    STRAT_ADFLAT,       /* Flat Strategy A with diagonal-order DTW         */
    STRAT_BDIAG,        /* Strategy B step 1: diagonal order, serial      */
    STRAT_B,            /* Strategy B: wavefront inside each DTW          */
    STRAT_BTILED        /* Strategy B, tiled wavefront                    */
} Strategy;

/* out has n*(n-1)/2 entries; pair (i,j), i<j, is at pair_index(n,i,j). */
static inline size_t pair_index(int n, int i, int j)
{
    return (size_t)i * (2 * (size_t)n - i - 1) / 2 + (size_t)(j - i - 1);
}

void allpairs_serial(const Dataset *d, double *out);
/* Strategy A; schedule from omp_set_schedule / OMP_SCHEDULE.
 * diag_kernel=0 uses dtw_serial per pair, 1 uses dtw_diag_serial. */
void allpairs_A(const Dataset *d, int diag_kernel, double *out);
/* Flatten all (i,j) pairs to one parallel iteration space. */
void allpairs_Aflat(const Dataset *d, int diag_kernel, double *out);
/* Profile one Strategy-A pass. Arrays must hold omp_get_max_threads() slots;
 * each slot receives that worker's pair count and loop time in seconds. */
void allpairs_A_profile(const Dataset *d, int diag_kernel, double *out,
                        double *thread_seconds, size_t *thread_pairs);
void allpairs_Aflat_profile(const Dataset *d, int diag_kernel, double *out,
                            double *thread_seconds, size_t *thread_pairs);
void allpairs_Bdiag(const Dataset *d, double *out);
void allpairs_B(const Dataset *d, double *out);
void allpairs_Btiled(const Dataset *d, int tile, double *out);

#endif
