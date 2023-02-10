#include <stdbool.h>

inline double get_lower_elem(const double row[], size_t size, size_t i)
{
    return i ? row[i - 1] : row[size - 1];
}

inline double get_upper_elem(const double row[], size_t size, size_t i)
{
    return i == size - 1 ? row[0] : row[i + 1];
}

inline size_t get_lower_row_index(size_t i)
{
    return !i ? i : i - 1;
}

inline size_t get_upper_row_index(size_t num_row, size_t i)
{
    return i == num_row - 1 ? i : i + 1;
}

double get_sum_neighbours(const double arr[], const struct parameters *p, size_t i, size_t j, double w_direct, double w_diagonal)
{
    size_t r_offset = i * p->M;
    double sum = (get_lower_elem(arr + r_offset, p->M, j) + get_upper_elem(arr + r_offset, p->M, j)) * w_direct;
    r_offset = get_lower_row_index(i) * p->M;
    sum += (get_lower_elem(arr + r_offset, p->M, j) + get_upper_elem(arr + r_offset, p->M, j)) * w_diagonal + arr[r_offset + j] * w_direct;
    r_offset = get_upper_row_index(p->N, i) * p->M;
    sum += (get_lower_elem(arr + r_offset, p->M, j) + get_upper_elem(arr + r_offset, p->M, j)) * w_diagonal + arr[r_offset + j] * w_direct;
    return sum;
}

bool caculate_metrix_with_st(const double init[], double avg[], const struct parameters *p, struct results *r, const double w_direct, const double w_diagonal)
{
    bool stop = true;
    size_t num = p->N * p->M;
    for (size_t i = 0; i < p->N; ++i) {
        size_t r_offset = i * p->M;
        for (size_t j = 0; j < p->M; ++j) {
            size_t offset = r_offset + j;
            double nb_conductivity = (1 - p->conductivity[offset]);
            avg[offset] = p->conductivity[offset] * init[offset];
            avg[offset] += nb_conductivity * get_sum_neighbours(init, p, i, j, w_direct, w_diagonal);
            if (avg[offset] < r->tmin)
                r->tmin = avg[offset];
            if (avg[offset] > r->tmax)
                r->tmax = avg[offset];
            double diff = fabs(avg[offset] - init[offset]);
            if (diff > r->maxdiff)
                r->maxdiff = diff;
            r->tavg += avg[offset] / num;
            stop &= diff < p->threshold;
        }
    }
    return stop;
}

bool caculate_metrix(const double init[], double avg[], const struct parameters *p, struct results *r, const double w_direct, const double w_diagonal)
{
    bool stop = true;
    for (size_t i = 0; i < p->N; ++i) {
        size_t r_offset = i * p->M;
        for (size_t j = 0; j < p->M; ++j) {
            size_t offset = r_offset + j;
            double nb_conductivity = (1 - p->conductivity[offset]);
            avg[offset] = p->conductivity[offset] * init[offset];
            avg[offset] += nb_conductivity * get_sum_neighbours(init, p, i, j, w_direct, w_diagonal);
            stop &= fabs(avg[offset] - init[offset]) < p->threshold;
        }
    }
    return stop;
}