#include "pulsewarp.h"

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

static double *xmalloc(size_t count)
{
    double *p = malloc(count * sizeof(double));
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}

#define BEAT(d, k) ((d)->x + (size_t)(k) * (d)->len)

/* Golden reference: plain serial loop over all N(N-1)/2 pairs. */
void allpairs_serial(const Dataset *d, double *out)
{
    const int n = d->n, L = d->len;
    double *work = xmalloc(2 * (size_t)(L + 1));
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            out[pair_index(n, i, j)] = dtw_serial(BEAT(d, i), L, BEAT(d, j), L, work);
    free(work);
}

/* Strategy A: parallel for over the outer beat index. Row i owns N-1-i pairs,
 * so the triangular loop is unbalanced under schedule(static) and balanced
 * under dynamic/guided. The schedule comes from schedule(runtime), i.e.
 * OMP_SCHEDULE or omp_set_schedule(), so one binary covers all of them.
 * Each thread owns a private two-row work buffer; there is no synchronisation
 * between pairs. */
void allpairs_A(const Dataset *d, int diag_kernel, double *out)
{
    const int n = d->n, L = d->len;
    #pragma omp parallel
    {
        double *work = xmalloc(3 * (size_t)(L + 1));
        #pragma omp for schedule(runtime)
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                out[pair_index(n, i, j)] = diag_kernel
                    ? dtw_diag_serial(BEAT(d, i), L, BEAT(d, j), L, work)
                    : dtw_serial(BEAT(d, i), L, BEAT(d, j), L, work);
        free(work);
    }
}

/* Flat Strategy A: precompute pair endpoints once, then distribute every
 * pair as an equal-sized OpenMP iteration. This avoids triangular row skew. */
static void make_pair_endpoints(int n, size_t **pi_out, size_t **pj_out, size_t *count_out)
{
    size_t count = (size_t)n * (n - 1) / 2;
    size_t *pi = malloc(count * sizeof(*pi)), *pj = malloc(count * sizeof(*pj));
    if (!pi || !pj) { fprintf(stderr, "out of memory for flattened pairs\n"); exit(1); }
    size_t k = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++, k++) { pi[k] = (size_t)i; pj[k] = (size_t)j; }
    *pi_out = pi; *pj_out = pj; *count_out = count;
}

void allpairs_Aflat(const Dataset *d, int diag_kernel, double *out)
{
    size_t *pi, *pj, count;
    make_pair_endpoints(d->n, &pi, &pj, &count);
    const int L = d->len;
    #pragma omp parallel
    {
        double *work = xmalloc(3 * (size_t)(L + 1));
        #pragma omp for schedule(runtime)
        for (size_t k = 0; k < count; k++) {
            const double *a = BEAT(d, pi[k]), *b = BEAT(d, pj[k]);
            out[k] = diag_kernel ? dtw_diag_serial(a, L, b, L, work)
                                 : dtw_serial(a, L, b, L, work);
        }
        free(work);
    }
    free(pi); free(pj);
}

/* Profile Strategy A without including time spent waiting at the worksharing
 * barrier. nowait lets each worker record its own completion time; the
 * parallel-region barrier still protects the shared output and work buffers. */
void allpairs_A_profile(const Dataset *d, int diag_kernel, double *out,
                        double *thread_seconds, size_t *thread_pairs)
{
    const int n = d->n, L = d->len;
    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        double *work = xmalloc(3 * (size_t)(L + 1));
        size_t count = 0;
        double start = omp_get_wtime();
        #pragma omp for schedule(runtime) nowait
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) {
                out[pair_index(n, i, j)] = diag_kernel
                    ? dtw_diag_serial(BEAT(d, i), L, BEAT(d, j), L, work)
                    : dtw_serial(BEAT(d, i), L, BEAT(d, j), L, work);
                count++;
            }
        thread_seconds[tid] = omp_get_wtime() - start;
        thread_pairs[tid] = count;
        free(work);
    }
}

void allpairs_Aflat_profile(const Dataset *d, int diag_kernel, double *out,
                            double *thread_seconds, size_t *thread_pairs)
{
    size_t *pi, *pj, count;
    make_pair_endpoints(d->n, &pi, &pj, &count);
    const int L = d->len;
    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        double *work = xmalloc(3 * (size_t)(L + 1));
        size_t assigned = 0;
        double start = omp_get_wtime();
        #pragma omp for schedule(runtime) nowait
        for (size_t k = 0; k < count; k++) {
            const double *a = BEAT(d, pi[k]), *b = BEAT(d, pj[k]);
            out[k] = diag_kernel ? dtw_diag_serial(a, L, b, L, work)
                                 : dtw_serial(a, L, b, L, work);
            assigned++;
        }
        thread_seconds[tid] = omp_get_wtime() - start;
        thread_pairs[tid] = assigned;
        free(work);
    }
    free(pi); free(pj);
}

/* Strategy B, step 1: same loop, DTW evaluated in diagonal order (serial). */
void allpairs_Bdiag(const Dataset *d, double *out)
{
    const int n = d->n, L = d->len;
    double *work = xmalloc(3 * (size_t)(L + 1));
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            out[pair_index(n, i, j)] = dtw_diag_serial(BEAT(d, i), L, BEAT(d, j), L, work);
    free(work);
}

/* Strategy B: pairs are processed one at a time; the team parallelises the
 * cells of each anti-diagonal. The parallel region is opened once for the
 * whole run so the cost measured is the per-diagonal barrier, not a fork/join
 * per DTW. */
void allpairs_B(const Dataset *d, double *out)
{
    const int n = d->n, L = d->len;
    double *work = xmalloc(3 * (size_t)(L + 1));   /* shared by the team */
    #pragma omp parallel
    {
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) {
                double v = dtw_wavefront(BEAT(d, i), L, BEAT(d, j), L, work);
                if (omp_get_thread_num() == 0) out[pair_index(n, i, j)] = v;
            }
    }
    free(work);
}

/* Strategy B, tiled: block-anti-diagonal wavefront, fewer and coarser
 * barriers (one per tile diagonal instead of one per cell diagonal). */
void allpairs_Btiled(const Dataset *d, int tile, double *out)
{
    const int n = d->n, L = d->len;
    double *D = xmalloc((size_t)(L + 1) * (L + 1));
    #pragma omp parallel
    {
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) {
                double v = dtw_tiled(BEAT(d, i), L, BEAT(d, j), L, tile, D);
                if (omp_get_thread_num() == 0) out[pair_index(n, i, j)] = v;
            }
    }
    free(D);
}
