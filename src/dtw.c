#include "pulsewarp.h"

#include <math.h>
#include <omp.h>

/* Every variant uses exactly this cell update, with the same argument
 * order, so all strategies produce bit-identical distances. */
static inline double cell(double ai, double bj, double up, double left, double diag)
{
    double m = up < left ? up : left;
    m = m < diag ? m : diag;
    return fabs(ai - bj) + m;
}

/* ------------------------------------------------------------ row order */

double dtw_serial(const double *a, int n, const double *b, int m, double *work)
{
    double *prev = work, *cur = work + (m + 1);
    prev[0] = 0.0;
    for (int j = 1; j <= m; j++) prev[j] = INFINITY;
    for (int i = 1; i <= n; i++) {
        cur[0] = INFINITY;
        const double ai = a[i - 1];
        for (int j = 1; j <= m; j++)
            cur[j] = cell(ai, b[j - 1], prev[j], cur[j - 1], prev[j - 1]);
        double *t = prev; prev = cur; cur = t;
    }
    return prev[m];
}

/* ------------------------------------------------------- diagonal order
 *
 * Diagonal s holds the D cells (i, j) with i + j = s, s = 0 .. n+m, stored
 * by i in a buffer of n+1 doubles. Three buffers rotate:
 *   d2 = diagonal s-2, d1 = diagonal s-1, d0 = diagonal s.
 * For an interior cell (i, j = s-i):
 *   up   D[i-1][j]   -> d1[i-1]
 *   left D[i][j-1]   -> d1[i]
 *   diag D[i-1][j-1] -> d2[i-1]
 * Boundary cells D[0][s] (when s<=m) and D[s][0] (when s<=n) are written
 * explicitly, so every read hits a value written on that diagonal.
 */

static inline void diag_bounds(int s, int n, int m, int *lo, int *hi)
{
    *lo = s - m > 1 ? s - m : 1;      /* interior cells only: i>=1, j>=1 */
    *hi = s - 1 < n ? s - 1 : n;
}

double dtw_diag_serial(const double *a, int n, const double *b, int m, double *work)
{
    double *d2 = work, *d1 = work + (n + 1), *d0 = work + 2 * (n + 1);
    for (int s = 0; s <= n + m; s++) {
        if (s <= m) d0[0] = (s == 0) ? 0.0 : INFINITY;  /* D[0][s] */
        if (s >= 1 && s <= n) d0[s] = INFINITY;          /* D[s][0] */
        int lo, hi;
        diag_bounds(s, n, m, &lo, &hi);
        for (int i = lo; i <= hi; i++)
            d0[i] = cell(a[i - 1], b[s - i - 1], d1[i - 1], d1[i], d2[i - 1]);
        double *t = d2; d2 = d1; d1 = d0; d0 = t;
    }
    return d1[n];   /* diagonal n+m, cell i=n */
}

double dtw_wavefront(const double *a, int n, const double *b, int m, double *work)
{
    /* each thread rotates its own copies of the (shared) buffer pointers */
    double *d2 = work, *d1 = work + (n + 1), *d0 = work + 2 * (n + 1);
    const int master = omp_get_thread_num() == 0;
    for (int s = 0; s <= n + m; s++) {
        /* boundary cells are outside the omp-for range, so thread 0 can write
         * them without a race; the loop's barrier publishes them. */
        if (master) {
            if (s <= m) d0[0] = (s == 0) ? 0.0 : INFINITY;
            if (s >= 1 && s <= n) d0[s] = INFINITY;
        }
        int lo, hi;
        diag_bounds(s, n, m, &lo, &hi);
        #pragma omp for schedule(static)
        for (int i = lo; i <= hi; i++)
            d0[i] = cell(a[i - 1], b[s - i - 1], d1[i - 1], d1[i], d2[i - 1]);
        /* implicit barrier: diagonal s complete before s+1 starts */
        double *t = d2; d2 = d1; d1 = d0; d0 = t;
    }
    return d1[n];
}

/* ----------------------------------------------------------- tiled
 * Full (n+1) x (m+1) matrix, row-major. Tiles of tile x tile interior cells.
 * Tile (ti, tj) depends on (ti-1, tj), (ti, tj-1), (ti-1, tj-1), so tiles on
 * the same tile-anti-diagonal ti + tj = t are independent.
 */
double dtw_tiled(const double *a, int n, const double *b, int m, int tile, double *D)
{
    const int W = m + 1;
    const int TI = (n + tile - 1) / tile, TJ = (m + tile - 1) / tile;

    #pragma omp single
    {
        D[0] = 0.0;
        for (int j = 1; j <= m; j++) D[j] = INFINITY;
        for (int i = 1; i <= n; i++) D[(size_t)i * W] = INFINITY;
    }   /* implicit barrier */

    for (int t = 0; t <= TI + TJ - 2; t++) {
        int lo = t - (TJ - 1) > 0 ? t - (TJ - 1) : 0;
        int hi = t < TI - 1 ? t : TI - 1;
        #pragma omp for schedule(dynamic, 1)
        for (int ti = lo; ti <= hi; ti++) {
            int tj = t - ti;
            int i0 = ti * tile + 1, i1 = i0 + tile - 1; if (i1 > n) i1 = n;
            int j0 = tj * tile + 1, j1 = j0 + tile - 1; if (j1 > m) j1 = m;
            for (int i = i0; i <= i1; i++) {
                double *row = D + (size_t)i * W, *prow = row - W;
                const double ai = a[i - 1];
                for (int j = j0; j <= j1; j++)
                    row[j] = cell(ai, b[j - 1], prow[j], row[j - 1], prow[j - 1]);
            }
        }
    }
    return D[(size_t)n * W + m];
}
