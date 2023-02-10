#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "compute.h"
#include "ref1.c"

void do_compute(const struct parameters *p, struct results *r)
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
    clock_t start = clock();
    for (size_t iter = 0; iter < p->maxiter; ++iter) {
        if (iter % p->period == 0) {
            r->niter = iter;
            r->tmin = p->io_tmax;
            r->tmax = p->io_tmin;
            r->maxdiff = 0.0;
            r->tavg = 0.0;
            if (caculate_metrix_with_st(init, avg, p, r, w_direct, w_diagonal)) {
                r->time = (double)(clock() - start) / CLOCKS_PER_SEC;
                break;
            } else {
                r->time = (double)(clock() - start) / CLOCKS_PER_SEC;
                start = clock();
            }
        } else {
            if (caculate_metrix(init, avg, p, r, w_direct, w_diagonal))
                break;
        }
        double *tmp = avg;
        avg = (double *)init;
        init = tmp;
    }
}
