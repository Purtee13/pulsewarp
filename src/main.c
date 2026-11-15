/*
 * pulsewarp: all-pairs DTW over N ECG heartbeats, one strategy per run.
 *
 *   ./pulsewarp --input data/mitbih_train.csv --n 300 --len 256 \
 *               --strategy A --schedule dynamic --threads 16 --reps 5 \
 *               --golden results/golden/golden_N300_L256.bin
 *
 * Prints one CSV result row to stdout (diagnostics go to stderr).
 */
#define _POSIX_C_SOURCE 200809L
#include "pulsewarp.h"

#include <getopt.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char MAGIC[8] = "PWDTW01";

static void usage(const char *p)
{
    fprintf(stderr,
        "usage: %s [options]\n"
        "  --input FILE        MIT-BIH heartbeat CSV (Kaggle mitbih_train.csv)\n"
        "  --synthetic         use synthetic ECG-like beats instead of --input\n"
        "  --n N               number of heartbeats (default 300)\n"
        "  --len L             resampled length per beat (default 256)\n"
        "  --no-trim           keep the CSV's trailing zero padding\n"
        "  --strategy S        serial | A | Aflat | Adiag | Adflat | Bdiag | B | Btiled\n"
        "  --schedule S[,c]    Strategy A loop schedule: static|dynamic|guided (default static)\n"
        "  --threads T         OpenMP threads (default: OMP_NUM_THREADS)\n"
        "  --tile T            tile size for Btiled (default 64)\n"
        "  --reps R            timed repetitions, median reported (default 1)\n"
        "  --out FILE          write the distance vector (binary)\n"
        "  --golden FILE       verify against a saved serial distance vector\n"
        "  --profile FILE      write per-thread Strategy-A work profile CSV\n"
        "  --header            print the CSV header line first\n", p);
}

static int write_dist(const char *path, int n, int len, const double *v, size_t cnt)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    int32_t hdr[2] = { n, len };
    uint64_t c = cnt;
    fwrite(MAGIC, 1, 8, f);
    fwrite(hdr, sizeof hdr, 1, f);
    fwrite(&c, sizeof c, 1, f);
    size_t w = fwrite(v, sizeof(double), cnt, f);
    fclose(f);
    return w == cnt ? 0 : -1;
}

/* returns max relative error, or -1 if the file does not match the run */
static double verify(const char *path, int n, int len, const double *v, size_t cnt, double *max_abs)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }
    char mg[8]; int32_t hdr[2]; uint64_t c;
    if (fread(mg, 1, 8, f) != 8 || memcmp(mg, MAGIC, 8) ||
        fread(hdr, sizeof hdr, 1, f) != 1 || fread(&c, sizeof c, 1, f) != 1 ||
        hdr[0] != n || hdr[1] != len || c != cnt) {
        fprintf(stderr, "%s: header does not match this run (n=%d len=%d)\n", path, n, len);
        fclose(f); return -1;
    }
    double *g = malloc(cnt * sizeof(double));
    if (fread(g, sizeof(double), cnt, f) != cnt) { fclose(f); free(g); return -1; }
    fclose(f);
    double mr = 0, ma = 0;
    for (size_t k = 0; k < cnt; k++) {
        double e = fabs(v[k] - g[k]);
        double r = e / (fabs(g[k]) > 1e-12 ? fabs(g[k]) : 1e-12);
        if (e > ma) ma = e;
        if (r > mr) mr = r;
    }
    free(g);
    *max_abs = ma;
    return mr;
}

static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv)
{
    const char *input = NULL, *outp = NULL, *golden = NULL, *profile = NULL;
    const char *strat_s = "serial", *sched_s = "static";
    int synthetic = 0, n = 300, len = 256, trim = 1, threads = 0, tile = 64, reps = 1, header = 0;

    static struct option opts[] = {
        {"input", 1, 0, 'i'}, {"synthetic", 0, 0, 'y'}, {"n", 1, 0, 'n'},
        {"len", 1, 0, 'l'}, {"no-trim", 0, 0, 'z'}, {"strategy", 1, 0, 's'},
        {"schedule", 1, 0, 'c'}, {"threads", 1, 0, 't'}, {"tile", 1, 0, 'b'},
        {"reps", 1, 0, 'r'}, {"out", 1, 0, 'o'}, {"golden", 1, 0, 'g'},
        {"profile", 1, 0, 'p'},
        {"header", 0, 0, 'H'}, {"help", 0, 0, 'h'}, {0, 0, 0, 0}};
    int ch;
    while ((ch = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (ch) {
        case 'i': input = optarg; break;
        case 'y': synthetic = 1; break;
        case 'n': n = atoi(optarg); break;
        case 'l': len = atoi(optarg); break;
        case 'z': trim = 0; break;
        case 's': strat_s = optarg; break;
        case 'c': sched_s = optarg; break;
        case 't': threads = atoi(optarg); break;
        case 'b': tile = atoi(optarg); break;
        case 'r': reps = atoi(optarg); break;
        case 'o': outp = optarg; break;
        case 'g': golden = optarg; break;
        case 'p': profile = optarg; break;
        case 'H': header = 1; break;
        default: usage(argv[0]); return 1;
        }
    }
    if (n < 2 || len < 2 || reps < 1 || tile < 1 || (!input && !synthetic)) {
        usage(argv[0]);
        return 1;
    }

    Strategy st;
    if      (!strcmp(strat_s, "serial")) st = STRAT_SERIAL;
    else if (!strcmp(strat_s, "A"))      st = STRAT_A;
    else if (!strcmp(strat_s, "Aflat"))  st = STRAT_AFLAT;
    else if (!strcmp(strat_s, "Adiag"))  st = STRAT_ADIAG;
    else if (!strcmp(strat_s, "Adflat")) st = STRAT_ADFLAT;
    else if (!strcmp(strat_s, "Bdiag"))  st = STRAT_BDIAG;
    else if (!strcmp(strat_s, "B"))      st = STRAT_B;
    else if (!strcmp(strat_s, "Btiled")) st = STRAT_BTILED;
    else { fprintf(stderr, "unknown strategy %s\n", strat_s); return 1; }
    if (profile && st != STRAT_A && st != STRAT_ADIAG && st != STRAT_AFLAT && st != STRAT_ADFLAT) {
        fprintf(stderr, "--profile is supported only with Strategy-A variants\n");
        return 1;
    }

    /* schedule for Strategy A */
    char sched_name[32], sched_label[64];
    snprintf(sched_name, sizeof sched_name, "%s", sched_s);
    int chunk = 0;
    char *sep = strpbrk(sched_name, ",:");
    if (sep) { *sep = 0; chunk = atoi(sep + 1); }
    if (chunk > 0) snprintf(sched_label, sizeof sched_label, "%s:%d", sched_name, chunk);
    else snprintf(sched_label, sizeof sched_label, "%s", sched_name);
    omp_sched_t kind;
    if      (!strcmp(sched_name, "static"))  kind = omp_sched_static;
    else if (!strcmp(sched_name, "dynamic")) kind = omp_sched_dynamic;
    else if (!strcmp(sched_name, "guided"))  kind = omp_sched_guided;
    else { fprintf(stderr, "unknown schedule %s\n", sched_s); return 1; }
    omp_set_schedule(kind, chunk);

    if (threads > 0) omp_set_num_threads(threads);
    int nthreads = (st == STRAT_SERIAL || st == STRAT_BDIAG) ? 1 : omp_get_max_threads();

    Dataset d;
    if (synthetic) make_synthetic(n, len, 548u, &d);
    else if (load_mitbih_csv(input, n, len, trim, &d)) return 1;

    size_t cnt = (size_t)n * (n - 1) / 2;
    double *dist = malloc(cnt * sizeof(double));
    double *times = malloc(reps * sizeof(double));
    if (!dist || !times) { fprintf(stderr, "out of memory\n"); return 1; }

    for (int r = 0; r < reps; r++) {
        double t0 = omp_get_wtime();
        switch (st) {
        case STRAT_SERIAL: allpairs_serial(&d, dist); break;
        case STRAT_A:      allpairs_A(&d, 0, dist); break;
        case STRAT_AFLAT:  allpairs_Aflat(&d, 0, dist); break;
        case STRAT_ADIAG:  allpairs_A(&d, 1, dist); break;
        case STRAT_ADFLAT: allpairs_Aflat(&d, 1, dist); break;
        case STRAT_BDIAG:  allpairs_Bdiag(&d, dist); break;
        case STRAT_B:      allpairs_B(&d, dist); break;
        case STRAT_BTILED: allpairs_Btiled(&d, tile, dist); break;
        }
        times[r] = omp_get_wtime() - t0;
        fprintf(stderr, "[%s] rep %d/%d: %.6f s\n", strat_s, r + 1, reps, times[r]);
    }

    if (profile) {
        int cap = omp_get_max_threads();
        double *thread_seconds = calloc((size_t)cap, sizeof(double));
        size_t *thread_pairs = calloc((size_t)cap, sizeof(size_t));
        if (!thread_seconds || !thread_pairs) {
            fprintf(stderr, "out of memory for profile\n"); return 1;
        }
        if (st == STRAT_A || st == STRAT_ADIAG)
            allpairs_A_profile(&d, st == STRAT_ADIAG, dist, thread_seconds, thread_pairs);
        else
            allpairs_Aflat_profile(&d, st == STRAT_ADFLAT, dist, thread_seconds, thread_pairs);
        FILE *pf = fopen(profile, "w");
        if (!pf) { perror(profile); return 1; }
        fprintf(pf, "thread,pairs,elapsed_s,strategy,schedule,n,len,threads\n");
        for (int t = 0; t < cap; t++)
            fprintf(pf, "%d,%zu,%.9f,%s,%s,%d,%d,%d\n", t, thread_pairs[t],
                    thread_seconds[t], strat_s, sched_label, n, len, cap);
        fclose(pf);
        free(thread_seconds); free(thread_pairs);
        fprintf(stderr, "wrote per-thread profile to %s\n", profile);
    }

    double checksum = 0;
    for (size_t k = 0; k < cnt; k++) checksum += dist[k];

    const char *vstat = "skipped";
    double max_rel = NAN, max_abs = NAN;
    if (golden) {
        max_rel = verify(golden, n, len, dist, cnt, &max_abs);
        vstat = (max_rel < 0) ? "ERROR" : (max_rel <= 1e-9 ? "PASS" : "FAIL");
        fprintf(stderr, "verify vs %s: %s (max_abs=%.3e max_rel=%.3e)\n",
                golden, vstat, max_abs, max_rel);
    }
    if (outp) {
        if (write_dist(outp, n, len, dist, cnt)) { fprintf(stderr, "write failed\n"); return 1; }
        fprintf(stderr, "wrote %zu distances to %s\n", cnt, outp);
    }

    double *sorted = malloc(reps * sizeof(double));
    memcpy(sorted, times, reps * sizeof(double));
    qsort(sorted, reps, sizeof(double), cmp_d);
    double median = (reps % 2) ? sorted[reps / 2] : 0.5 * (sorted[reps / 2 - 1] + sorted[reps / 2]);

    const char *sched_out = (st == STRAT_A || st == STRAT_AFLAT || st == STRAT_ADIAG || st == STRAT_ADFLAT)
                          ? sched_label : (st == STRAT_BTILED ? "tile" : "-");
    if (header)
        printf("strategy,schedule,tile,threads,n,len,pairs,reps,median_s,min_s,max_s,checksum,verify,max_rel_err,times\n");
    printf("%s,%s,%d,%d,%d,%d,%zu,%d,%.6f,%.6f,%.6f,%.10e,%s,%.3e,",
           strat_s, sched_out, st == STRAT_BTILED ? tile : 0, nthreads, n, len, cnt, reps,
           median, sorted[0], sorted[reps - 1], checksum, vstat, max_rel);
    for (int r = 0; r < reps; r++) printf("%s%.6f", r ? ";" : "", times[r]);
    printf("\n");

    free(sorted); free(times); free(dist); free_dataset(&d);
    return (golden && strcmp(vstat, "PASS")) ? 2 : 0;
}
