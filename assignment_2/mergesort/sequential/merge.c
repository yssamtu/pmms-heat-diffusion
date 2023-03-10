#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/* Ordering of the vector */
typedef enum Ordering {ASCENDING, DESCENDING, RANDOM} Order;

int debug = 0;

void print_v(int *v, long l)
{
    printf("\n");
    for (long i = 0; i < l; ++i) {
        if (i && i % 10 == 0)
            printf("\n");
        printf("%d ", v[i]);
    }
    printf("\n");
}

/*merge two list*/
void merge(int *v, long left, long mid, long right, int *temp)
{
    long i = left;
    long j = mid;
    long k = left;
    while (i < mid && j < right) {
        if (v[i] <= v[j])
            temp[k++] = v[i++];
        else
            temp[k++] = v[j++];
    }
    while (i < mid) {
        temp[k++] = v[i++];
    }
    while (j < right) {
        temp[k++] = v[j++];
    }
    memcpy(v+left ,temp+left,(right-left)*sizeof(int));
}
/*split the array untill the length of sub-array is 1*/
void mergeSort_UpToDown(int *v, long left, long right, int *temp)
{
    if (right-left <= 1)
        return;
    long mid = left + (right-left)/2;
    mergeSort_UpToDown(v, left, mid, temp);
    mergeSort_UpToDown(v, mid, right, temp);

    merge(v, left, mid, right, temp);
}

/* Sort vector v of l elements using mergesort, up to down*/
void msort(int *v, long l)
{
    int *temp = (int *)malloc(l * sizeof(int));
    long left = 0;
    long right = l;
    mergeSort_UpToDown(v, left, right, temp);
    free(temp);
}

int main(int argc, char **argv)
{

    int c;
    int seed = 42;
    long length = 1e4;
    int num_threads = 1;
    Order order = ASCENDING;
    int *vector;

    struct timespec before, after;

    /* Read command-line options. */
    while((c = getopt(argc, argv, "adrgp:l:s:")) != -1) {
        switch(c) {
            case 'a':
                order = ASCENDING;
                break;
            case 'd':
                order = DESCENDING;
                break;
            case 'r':
                order = RANDOM;
                break;
            case 'l':
                length = atol(optarg);
                break;
            case 'g':
                debug = 1;
                break;
            case 's':
                seed = atoi(optarg);
                break;
            case 'p':
                num_threads = atoi(optarg);
                break;
            case '?':
                if (optopt == 'l' || optopt == 's')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option '-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                return -1;
            default:
                return -1;
        }
    }

    /* Seed such that we can always reproduce the same random vector */
    srand(seed);

    /* Allocate vector. */
    vector = (int *)malloc(length * sizeof(int));
    if (vector == NULL) {
        fprintf(stderr, "Malloc failed...\n");
        return -1;
    }
    /* Fill vector. */
    switch(order) {
        case ASCENDING:
            for (long i = 0; i < length; ++i) {
                vector[i] = (int)i;
            }
            break;
        case DESCENDING:
            for (long i = 0; i < length; ++i) {
                vector[i] = (int)(length - i);
            }
            break;
        case RANDOM:
            for (long i = 0; i < length; ++i) {
                vector[i] = rand();
            }
            break;
    }

    if (debug)
        print_v(vector, length);

    clock_gettime(CLOCK_MONOTONIC, &before);

    /* Sort */
    msort(vector, length);

    clock_gettime(CLOCK_MONOTONIC, &after);
    double time = (double)(after.tv_sec - before.tv_sec) +
                  (double)(after.tv_nsec - before.tv_nsec) / 1e9;

    printf("Mergesort took: % .6e seconds \n", time);

    if (debug)
        print_v(vector, length);

    return 0;
}

