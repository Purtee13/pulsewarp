/* Unit tests: hand-checked DTW values, and every strategy against dtw_serial
 * on random sequences of unequal and edge-case lengths. */
#include "../src/pulsewarp.h"

#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static double run_wavefront(const double *a, int n, const double *b, int m, int threads)
{
    double *work = malloc(3 * (size_t)(n + 1) * sizeof(double)), r = 0;
    #pragma omp parallel num_threads(threads)
    {
        double v = dtw_wavefront(a, n, b, m, work);
        #pragma omp master
        r = v;
    }
    free(work);
    return r;
}

static double run_tiled(const double *a, int n, const double *b, int m, int tile, int threads)
{
    double *D = malloc((size_t)(n + 1) * (m + 1) * sizeof(double)), r = 0;
    #pragma omp parallel num_threads(threads)
    {
        double v = dtw_tiled(a, n, b, m, tile, D);
        #pragma omp master
        r = v;
    }
    free(D);
    return r;
}

int main(void)
{
    double work[4096];

    /* 1. identical sequences -> 0 */
    double s1[] = {1, 2, 3, 4, 3, 2};
    CHECK(dtw_serial(s1, 6, s1, 6, work) == 0.0, "identical != 0");

    /* 2. time-shifted copy: DTW absorbs the stretch exactly */
    double a[] = {0, 1, 2, 3};
    double b[] = {0, 0, 1, 2, 3};
    CHECK(dtw_serial(a, 4, b, 5, work) == 0.0, "stretched copy != 0");

    /* 3. hand-computed: a={1,2,3}, b={2,2,4}
     * cost |ai-bj|: [1 1 3; 0 0 2; 1 1 1]
     * D: [1 2 5; 1 1 3; 2 2 2] -> 2 */
    double c1[] = {1, 2, 3}, c2[] = {2, 2, 4};
    CHECK(dtw_serial(c1, 3, c2, 3, work) == 2.0, "hand example: got %g", dtw_serial(c1, 3, c2, 3, work));

    /* 4. symmetry */
    CHECK(dtw_serial(c1, 3, c2, 3, work) == dtw_serial(c2, 3, c1, 3, work), "not symmetric");

    /* 5. resampling: endpoints kept, linear midpoints */
    double src[] = {0, 10}, dst[5];
    resample_linear(src, 2, dst, 5);
    CHECK(dst[0] == 0 && dst[4] == 10 && fabs(dst[2] - 5) < 1e-12 && fabs(dst[1] - 2.5) < 1e-12, "resample");

    /* 6. every variant bit-identical to serial on random inputs */
    unsigned seed = 7;
    int lens[][2] = {{1, 1}, {1, 7}, {7, 1}, {2, 2}, {5, 13}, {13, 5}, {64, 64}, {100, 37}, {37, 100}, {257, 300}};
    int tiles[] = {1, 3, 16, 64};
    int nthreads[] = {1, 2, 3, 4, 8};
    for (size_t t = 0; t < sizeof lens / sizeof lens[0]; t++) {
        int n = lens[t][0], m = lens[t][1];
        double *x = malloc(n * sizeof(double)), *y = malloc(m * sizeof(double));
        for (int i = 0; i < n; i++) { seed = seed * 1103515245u + 12345u; x[i] = (seed >> 16) / 65536.0; }
        for (int i = 0; i < m; i++) { seed = seed * 1103515245u + 12345u; y[i] = (seed >> 16) / 65536.0; }
        double *w = malloc((3 * (size_t)(n + m + 2)) * sizeof(double));
        double ref = dtw_serial(x, n, y, m, w);
        double dg = dtw_diag_serial(x, n, y, m, w);
        CHECK(dg == ref, "diag %dx%d: %.17g vs %.17g", n, m, dg, ref);
        for (size_t p = 0; p < sizeof nthreads / sizeof nthreads[0]; p++) {
            double wf = run_wavefront(x, n, y, m, nthreads[p]);
            CHECK(wf == ref, "wavefront %dx%d p=%d: %.17g vs %.17g", n, m, nthreads[p], wf, ref);
            for (size_t q = 0; q < sizeof tiles / sizeof tiles[0]; q++) {
                double tl = run_tiled(x, n, y, m, tiles[q], nthreads[p]);
                CHECK(tl == ref, "tiled %dx%d tile=%d p=%d: %.17g vs %.17g", n, m, tiles[q], nthreads[p], tl, ref);
            }
        }
        free(x); free(y); free(w);
    }

    /* 7. all-pairs drivers agree with serial on a small synthetic set */
    Dataset d;
    make_synthetic(24, 50, 1, &d);
    size_t cnt = 24 * 23 / 2;
    double *g = malloc(cnt * sizeof(double)), *o = malloc(cnt * sizeof(double));
    allpairs_serial(&d, g);
    omp_set_num_threads(4);
    omp_sched_t kinds[] = {omp_sched_static, omp_sched_dynamic, omp_sched_guided};
    for (int k = 0; k < 3; k++) {
        omp_set_schedule(kinds[k], 0);
        allpairs_A(&d, 0, o);
        CHECK(!memcmp(g, o, cnt * sizeof(double)), "allpairs_A schedule %d", k);
        allpairs_Aflat(&d, 0, o);
        CHECK(!memcmp(g, o, cnt * sizeof(double)), "allpairs_Aflat schedule %d", k);
        allpairs_A(&d, 1, o);
        CHECK(!memcmp(g, o, cnt * sizeof(double)), "allpairs_A diag kernel schedule %d", k);
        allpairs_Aflat(&d, 1, o);
        CHECK(!memcmp(g, o, cnt * sizeof(double)), "allpairs_Adflat schedule %d", k);
    }
    allpairs_Bdiag(&d, o); CHECK(!memcmp(g, o, cnt * sizeof(double)), "allpairs_Bdiag");
    allpairs_B(&d, o);     CHECK(!memcmp(g, o, cnt * sizeof(double)), "allpairs_B");
    allpairs_Btiled(&d, 16, o); CHECK(!memcmp(g, o, cnt * sizeof(double)), "allpairs_Btiled");
    free(g); free(o); free_dataset(&d);

    if (fails) { printf("%d test(s) FAILED\n", fails); return 1; }
    printf("all tests passed\n");
    return 0;
}
