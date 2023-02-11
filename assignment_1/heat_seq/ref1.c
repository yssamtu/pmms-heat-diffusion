#include <stdbool.h>

inline double get_lower_elem(const double *restrict row, size_t size, size_t i)
{
    return i ? row[i - 1] : row[size - 1];
}

inline double get_upper_elem(const double *restrict row, size_t size, size_t i)
{
    return i == size - 1 ? row[0] : row[i + 1];
}

inline double *get_lower_row_index(const double *restrict init,
                                   const struct parameters *restrict p,
                                   size_t i)
{
    return !i ? p->tinit : init + i * p->M - p->M;
}

inline double *get_upper_row_index(const double *restrict init,
                                   const struct parameters *restrict p,
                                   size_t i)
{
    return i == p->N - 1 ? p->tinit + i * p->M : init + i * p->M + p->M;
}

double get_sum_neighbours(const double *restrict init,
                          const struct parameters *restrict p,
                          size_t i, size_t j,
                          double w_direct, double w_diagonal)
{
    const double *row = init + i * p->M;
    double sum = (get_lower_elem(row, p->M, j) + get_upper_elem(row, p->M, j)) *
                 w_direct;
    row = get_lower_row_index(init, p, i);
    sum += (get_lower_elem(row, p->M, j) + get_upper_elem(row, p->M, j)) *
           w_diagonal + row[j] * w_direct;
    row = get_upper_row_index(init, p, i);
    sum += (get_lower_elem(row, p->M, j) + get_upper_elem(row, p->M, j)) *
           w_diagonal + row[j] * w_direct;
    return sum;
}

bool caculate_metrix_with_st(const double *restrict init, double *restrict avg,
                             const struct parameters *restrict p,
                             struct results *restrict r,
                             double w_direct, double w_diagonal)
{
    bool stop = true;
    for (size_t i = 0, r_offset = 0; i < p->N; ++i, r_offset += p->M) {
        for (size_t j = 0, offset = r_offset; j < p->M; ++j, ++offset) {
            double sum_neighbours = get_sum_neighbours(init, p, i, j,
                                                       w_direct, w_diagonal);
            avg[offset] = sum_neighbours + p->conductivity[offset] *
                                           (init[offset] - sum_neighbours);
            if (avg[offset] < r->tmin)
                r->tmin = avg[offset];
            if (avg[offset] > r->tmax)
                r->tmax = avg[offset];
            double diff = fabs(avg[offset] - init[offset]);
            if (diff > r->maxdiff)
                r->maxdiff = diff;
            stop &= diff < p->threshold;
            r->tavg += avg[offset];
        }
    }
    r->tavg /= p->N * p->M;
    return stop;
}

bool caculate_metrix(const double *restrict init, double *restrict avg,
                     const struct parameters *restrict p,
                     struct results *restrict r,
                     double w_direct, double w_diagonal)
{
    bool stop = true;
    for (size_t i = 0, r_offset = 0; i < p->N; ++i, r_offset += p->M) {
        for (size_t j = 0, offset = r_offset; j < p->M; ++j, ++offset) {
            double sum_neighbours = get_sum_neighbours(init, p, i, j,
                                                       w_direct, w_diagonal);
            avg[offset] = sum_neighbours + p->conductivity[offset] *
                                           (init[offset] - sum_neighbours);
            stop &= fabs(avg[offset] - init[offset]) < p->threshold;
        }
    }
    return stop;
}
