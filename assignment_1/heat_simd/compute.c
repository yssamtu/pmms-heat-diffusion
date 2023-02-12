#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <immintrin.h>
#include "compute.h"
// #include <stdio.h>

static inline double get_lower_elem(const double *restrict row, size_t size,
                                    size_t i);
static inline double get_upper_elem(const double *restrict row, size_t size,
                                    size_t i);
static inline const double *get_lower_row_index(const double *restrict init,
                                                const struct parameters *restrict p,
                                                size_t i);
static inline const double *get_upper_row_index(const double *restrict init,
                                                const struct parameters *restrict p,
                                                size_t i);
static void get_sum_neighbours(const double *restrict init,
                                 const struct parameters *restrict p,
                                 size_t i, size_t j,
                                 double w_direct, double w_diagonal,
                                 double *restrict sum_neighbours);
static bool caculate_metrix_with_st(const double *restrict init,
                                    double *restrict avg,
                                    const struct parameters *restrict p,
                                    struct results *restrict r,
                                    double w_direct, double w_diagonal);
static bool caculate_metrix(const double *restrict init, double *restrict avg,
                            const struct parameters *restrict p,
                            struct results *restrict r,
                            double w_direct, double w_diagonal);

void do_compute(const struct parameters* p, struct results *r)
{
    const double sqrt2 = sqrt(2);
    const double w_direct = sqrt2 / (sqrt2 + 1) / 4;
    const double w_diagonal = 1 / (sqrt2 + 1) / 4;
    const size_t size = p->N * p->M;
    double t1[size] __attribute__ ((aligned(32)));
    memcpy(t1, p->tinit, sizeof(t1));
    double t2[size] __attribute__ ((aligned(32)));
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
    // for (size_t i = 0; i < p->N; ++i) {
    //     for (size_t j = 0; j < p->M; ++j) {
    //         printf("%lf\t", avg[i * p->M + j]);
    //     }
    //     printf("\n");
    // }
}

static inline double get_lower_elem(const double *restrict row, size_t size,
                                    size_t i)
{
    return i ? row[i - 1] : row[size - 1];
}

static inline double get_upper_elem(const double *restrict row, size_t size,
                                    size_t i)
{
    return i == size - 1 ? row[0] : row[i + 1];
}

static inline const double *get_lower_row_index(const double *restrict init,
                                                const struct parameters *restrict p,
                                                size_t i)
{
    return !i ? p->tinit : init + i * p->M - p->M;
}

static inline const double *get_upper_row_index(const double *restrict init,
                                                const struct parameters *restrict p,
                                                size_t i)
{
    return i == p->N - 1 ? p->tinit + i * p->M : init + i * p->M + p->M;
}

static void get_sum_neighbours(const double *restrict init,
                               const struct parameters *restrict p,
                               size_t i, size_t j,
                               double w_direct, double w_diagonal,
                               double *restrict sum_neighbours)
{
    const double *row = init + i * p->M;
    double lower_elem[4] __attribute__ ((aligned(32))) = {0};
    for (int k = 0; k < 4; ++k)
        lower_elem[k] = get_lower_elem(row, p->M, j + k);
    __m256d lower_elem_vec = _mm256_load_pd(lower_elem);
    double upper_elem[4] __attribute__ ((aligned(32))) = {0};
    for (int k = 0; k < 4; ++k)
        upper_elem[k] = get_upper_elem(row, p->M, j + k);
    __m256d upper_elem_vec = _mm256_load_pd(upper_elem);
    __m256d sum_vec = _mm256_add_pd(lower_elem_vec, upper_elem_vec);
    __m256d w_direct_vec = _mm256_set1_pd(w_direct);
    sum_vec = _mm256_mul_pd(sum_vec, w_direct_vec);
    row = get_lower_row_index(init, p, i);
    for (int k = 0; k < 4; ++k)
        lower_elem[k] = get_lower_elem(row, p->M, j + k);
    lower_elem_vec = _mm256_load_pd(lower_elem);
    for (int k = 0; k < 4; ++k)
        upper_elem[k] = get_upper_elem(row, p->M, j + k);
    upper_elem_vec = _mm256_load_pd(upper_elem);
    __m256d tmp_vec = _mm256_add_pd(lower_elem_vec, upper_elem_vec);
    __m256d w_diagonal_vec = _mm256_set1_pd(w_diagonal);
    tmp_vec = _mm256_mul_pd(tmp_vec, w_diagonal_vec);
    sum_vec = _mm256_add_pd(sum_vec, tmp_vec);
    __m256d curr_elem_vec = _mm256_load_pd(row + j);
    tmp_vec = _mm256_mul_pd(curr_elem_vec, w_direct_vec);
    sum_vec = _mm256_add_pd(sum_vec, tmp_vec);
    row = get_upper_row_index(init, p, i);
    for (int k = 0; k < 4; ++k)
        lower_elem[k] = get_lower_elem(row, p->M, j + k);
    lower_elem_vec = _mm256_load_pd(lower_elem);
    for (int k = 0; k < 4; ++k)
        upper_elem[k] = get_upper_elem(row, p->M, j + k);
    upper_elem_vec = _mm256_load_pd(upper_elem);
    tmp_vec = _mm256_add_pd(lower_elem_vec, upper_elem_vec);
    tmp_vec = _mm256_mul_pd(tmp_vec, w_diagonal_vec);
    sum_vec = _mm256_add_pd(sum_vec, tmp_vec);
    tmp_vec = _mm256_mul_pd(curr_elem_vec, w_direct_vec);
    sum_vec = _mm256_add_pd(sum_vec, tmp_vec);
    _mm256_store_pd(sum_neighbours, sum_vec);
}

static bool caculate_metrix_with_st(const double *restrict init,
                                    double *restrict avg,
                                    const struct parameters *restrict p,
                                    struct results *restrict r,
                                    double w_direct, double w_diagonal)
{
    for (size_t i = 0, r_offset = 0; i < p->N; ++i, r_offset += p->M) {
        for (size_t j = 0, offset = r_offset; j < p->M; j += 4, offset += 4) {
            double sum_neighbours[4] __attribute__ ((aligned(32))) = {0};
            get_sum_neighbours(init, p, i, j, w_direct, w_diagonal,
                               sum_neighbours);
            __m256d init_vec = _mm256_load_pd(init + offset);
            __m256d sum_neighbours_vec = _mm256_load_pd(sum_neighbours);
            __m256d conductivity_vec = _mm256_load_pd(p->conductivity + offset);
            __m256d avg_vec = _mm256_sub_pd(init_vec, sum_neighbours_vec);
            avg_vec = _mm256_mul_pd(avg_vec, conductivity_vec);
            avg_vec = _mm256_add_pd(avg_vec, sum_neighbours_vec);
            _mm256_store_pd(avg + offset, avg_vec);
            for (int k = 0; k < 4; ++k) {
                if (avg[offset + k] < r->tmin)
                    r->tmin = avg[offset];
                if (avg[offset + k] > r->tmax)
                    r->tmax = avg[offset];
            }
            __m256d diff_vec = _mm256_sub_pd(avg_vec, init_vec);
            double diff[4] __attribute__ ((aligned(32))) = {0};
            _mm256_store_pd(diff, diff_vec);
            for (int k = 0; k < 4; ++k) {
                double absdiff = fabs(avg[offset + k] - init[offset + k]);\
                if (absdiff > r->maxdiff)
                    r->maxdiff = absdiff;
            }
            r->tavg += avg[offset];
        }
    }
    r->tavg /= p->N * p->M;
    return r->maxdiff < p->threshold;
}

static bool caculate_metrix(const double *restrict init, double *restrict avg,
                            const struct parameters *restrict p,
                            struct results *restrict r,
                            double w_direct, double w_diagonal)
{
    bool stop = true;
    for (size_t i = 0, r_offset = 0; i < p->N; ++i, r_offset += p->M) {
        for (size_t j = 0, offset = r_offset; j < p->M; j += 4, offset += 4) {
            double sum_neighbours[4] __attribute__ ((aligned(32))) = {0};
            get_sum_neighbours(init, p, i, j, w_direct, w_diagonal,
                               sum_neighbours);
            __m256d init_vec = _mm256_load_pd(init + offset);
            __m256d sum_neighbours_vec = _mm256_load_pd(sum_neighbours);
            __m256d conductivity_vec = _mm256_load_pd(p->conductivity + offset);
            __m256d avg_vec = _mm256_sub_pd(init_vec, sum_neighbours_vec);
            avg_vec = _mm256_mul_pd(avg_vec, conductivity_vec);
            avg_vec = _mm256_add_pd(avg_vec, sum_neighbours_vec);
            _mm256_store_pd(avg + offset, avg_vec);
            __m256d diff_vec = _mm256_sub_pd(avg_vec, init_vec);
            double diff[4] __attribute__ ((aligned(32))) = {0};
            _mm256_store_pd(diff, diff_vec);
            for (int k = 0; k < 4; ++k)
                stop &= fabs(diff[k]) < p->threshold;
        }
    }
    return stop;
}
