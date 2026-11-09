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
