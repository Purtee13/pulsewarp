#define _POSIX_C_SOURCE 200809L
#include "pulsewarp.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void resample_linear(const double *src, int src_len, double *dst, int dst_len)
{
    if (src_len <= 1) {
        for (int t = 0; t < dst_len; t++) dst[t] = src_len ? src[0] : 0.0;
        return;
    }
    if (dst_len == 1) { dst[0] = src[0]; return; }
    const double scale = (double)(src_len - 1) / (double)(dst_len - 1);
    for (int t = 0; t < dst_len; t++) {
        double x = t * scale;
        int i0 = (int)x;
        if (i0 >= src_len - 1) { dst[t] = src[src_len - 1]; continue; }
        double f = x - i0;
        dst[t] = src[i0] * (1.0 - f) + src[i0 + 1] * f;
    }
}

static void alloc_dataset(Dataset *d, int n, int len)
{
    d->n = n;
    d->len = len;
    d->x = malloc((size_t)n * len * sizeof(double));
    d->label = malloc((size_t)n * sizeof(int));
    if (!d->x || !d->label) { fprintf(stderr, "out of memory\n"); exit(1); }
}

void free_dataset(Dataset *d)
{
    free(d->x);
    free(d->label);
    d->x = NULL;
    d->label = NULL;
}

/* Parse one CSV line into vals; returns the number of fields parsed. */
static int parse_line(char *line, double *vals, int maxv)
{
    int k = 0;
    char *p = line;
    while (*p && k < maxv) {
        char *end;
        double v = strtod(p, &end);
        if (end == p) break;
        vals[k++] = v;
        p = end;
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        if (*p == '\r' || *p == '\n') break;
    }
    return k;
}

int load_mitbih_csv(const char *path, int n, int len, int trim, Dataset *out)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }

    /* pass 1: count data rows */
    char *line = NULL;
    size_t cap = 0;
    long rows = 0;
    while (getline(&line, &cap, f) > 0)
        if (line[0] >= '0' && line[0] <= '9') rows++;   /* skip header / blank */
        else if (line[0] == '-' || line[0] == '.') rows++;
    if (rows < n) {
        fprintf(stderr, "%s has %ld rows, need %d\n", path, rows, n);
        fclose(f); free(line); return -1;
    }

    alloc_dataset(out, n, len);
    const int MAXV = 4096;
    double *vals = malloc(MAXV * sizeof(double));
    long stride = rows / n;                 /* evenly spaced, covers all classes */

    rewind(f);
    long r = 0;
    int k = 0;
    while (k < n && getline(&line, &cap, f) > 0) {
        if (!((line[0] >= '0' && line[0] <= '9') || line[0] == '-' || line[0] == '.'))
            continue;
        if (r++ % stride != 0) continue;
        int nv = parse_line(line, vals, MAXV);
        if (nv < 3) continue;
        int nsamp = nv - 1;                 /* last column is the class label */
        out->label[k] = (int)vals[nv - 1];
        if (trim) {                          /* drop trailing zero padding */
            while (nsamp > 2 && vals[nsamp - 1] == 0.0) nsamp--;
        }
        resample_linear(vals, nsamp, out->x + (size_t)k * len, len);
        k++;
    }
    free(vals);
    free(line);
    fclose(f);
    if (k < n) {
        fprintf(stderr, "only read %d of %d beats\n", k, n);
        free_dataset(out);
        return -1;
    }
    return 0;
}

/* small deterministic LCG so results do not depend on libc's rand() */
static double urand(unsigned *s)
{
    *s = *s * 1664525u + 1013904223u;
    return (*s >> 8) * (1.0 / 16777216.0);
}

static double bump(double t, double c, double w, double a)
{
    double z = (t - c) / w;
    return a * exp(-0.5 * z * z);
}

void make_synthetic(int n, int len, unsigned seed, Dataset *out)
{
    alloc_dataset(out, n, len);
    const int base = 187;                       /* same native length as MIT-BIH CSV */
    double raw[187];
    unsigned s = seed ? seed : 12345u;
    for (int k = 0; k < n; k++) {
        double shift = (urand(&s) - 0.5) * 0.10;  /* timing jitter */
        double stretch = 0.85 + 0.3 * urand(&s);
        double qrs_amp = 0.7 + 0.3 * urand(&s);
        double t_amp = 0.15 + 0.2 * urand(&s);
        int cls = (int)(urand(&s) * 5);
        double wide = (cls == 2) ? 2.5 : 1.0;      /* "ventricular" beats: wide QRS */
        for (int i = 0; i < base; i++) {
            double t = (double)i / (base - 1);
            double u = (t - shift) / stretch;
            raw[i] = bump(u, 0.12, 0.025, 0.12)               /* P   */
                   - bump(u, 0.22, 0.008 * wide, 0.10)        /* Q   */
                   + bump(u, 0.25, 0.012 * wide, qrs_amp)     /* R   */
                   - bump(u, 0.28, 0.010 * wide, 0.15)        /* S   */
                   + bump(u, 0.50, 0.050, t_amp)              /* T   */
                   + 0.01 * (urand(&s) - 0.5);                /* noise */
        }
        out->label[k] = cls;
        resample_linear(raw, base, out->x + (size_t)k * len, len);
    }
}
