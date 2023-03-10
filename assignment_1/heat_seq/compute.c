#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "compute.h"
#include "ref1.c"

void do_compute(const struct parameters * restrict p, struct results *restrict r)
{
    const double sqrt2 = sqrt(2);
    const double w_direct = sqrt2 / (sqrt2 + 1) / 4;
    const double w_diagonal = 1 / (sqrt2 + 1) / 4;
    const size_t size = p->N * p->M;
    double t1[size];
    memcpy(t1, p->tinit, sizeof(t1));
    double t2[size];
    const double *init = t1;
    double *avg = t2;
    struct timespec before = {0};
    clock_gettime(CLOCK_MONOTONIC, &before);
    for (size_t iter = 1; iter <= p->maxiter; ++iter) {
        if (iter % p->period == 0 || iter == p->maxiter) {
            r->niter = iter;
            r->tmin = p->io_tmax;
            r->tmax = p->io_tmin;
            r->maxdiff = 0.0;
            r->tavg = 0.0;
            struct timespec after = {0};
            if (caculate_metrix_with_st(init, avg, p, r, w_direct, w_diagonal)) {
                clock_gettime(CLOCK_MONOTONIC, &after);
                r->time = (double)(after.tv_sec - before.tv_sec) +
                          (double)(after.tv_nsec - before.tv_nsec) / 1e9;
                break;
            } else {
                clock_gettime(CLOCK_MONOTONIC, &after);
                r->time = (double)(after.tv_sec - before.tv_sec) +
                          (double)(after.tv_nsec - before.tv_nsec) / 1e9;
                if (p->printreports)
                    report_results(p, r);
            }
        } else {
            if (caculate_metrix(init, avg, p, r, w_direct, w_diagonal)) {
                struct timespec after = {0};
                clock_gettime(CLOCK_MONOTONIC, &after);
                r->time = (double)(after.tv_sec - before.tv_sec) +
                          (double)(after.tv_nsec - before.tv_nsec) / 1e9;
                r->niter = iter;
                r->tmin = p->io_tmax;
                r->tmax = p->io_tmin;
                r->maxdiff = 0.0;
                r->tavg = 0.0;
                for (size_t i = 0; i < size; ++i) {
                    if (avg[i] < r->tmin)
                        r->tmin = avg[i];
                    if (avg[i] > r->tmax)
                        r->tmax = avg[i];
                    double diff = fabs(avg[i] - init[i]);
                    if (diff > r->maxdiff)
                        r->maxdiff = diff;
                    r->tavg += avg[i];
                }
                r->tavg /= size;
                break;
            }
        }
        double *tmp = avg;
        avg = (double *)init;
        init = tmp;
    }
}
