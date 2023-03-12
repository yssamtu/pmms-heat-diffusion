#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
    int *message;
    int *output;
    pthread_mutex_t *message_lock;
    pthread_mutex_t *output_lock;
    pthread_cond_t *message_item;
    pthread_cond_t *message_space;
    pthread_cond_t *output_item;
    pthread_cond_t *output_space;
} profile_t;

typedef struct {
    int *message;
    pthread_mutex_t *message_lock;
    pthread_cond_t *message_item;
    pthread_cond_t *message_space;
} output_t;

enum {
    CONSUMED = -1,
    END = -2,
    BUF_SIZE = 4088
};

static inline void next_message(int **now, int *head);
static inline void next_lock(pthread_mutex_t **now, pthread_mutex_t *head);
static void *comparator(void *profile);
static void *successor(void *output);

int main(int argc, char *argv[])
{
    int c;
    unsigned seed = 42;
    long length = 1e4;
    while((c = getopt(argc, argv, "l:s:")) != -1) {
        switch(c) {
            case 'l':
                length = atol(optarg);
                break;
            case 's':
                seed = atoi(optarg);
                break;
            case '?':
                fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                return -1;
            default:
                return -1;
        }
    }
    srand(seed);
    int message[(length + 1) * BUF_SIZE];
    pthread_mutex_t lock[(length + 1) * BUF_SIZE];
    pthread_cond_t item[length + 1];
    pthread_cond_t space[length + 1];
    for (long i = 0; i < BUF_SIZE * (length + 1); ++i) {
        message[i] = CONSUMED;
        pthread_mutex_init(&lock[i], NULL);
    }
    for (long i = 0; i < length + 1; ++i) {
        pthread_cond_init(&item[i], NULL);
        pthread_cond_init(&space[i], NULL);
    }
    int (*message_block)[length + 1][BUF_SIZE] = (int (*)[length + 1][BUF_SIZE])message;
    pthread_mutex_t (*lock_block)[length + 1][BUF_SIZE] = (pthread_mutex_t (*)[length + 1][BUF_SIZE])lock;
    profile_t profile[length];
    for (long i = 0; i < length; ++i) {
        profile[i].message = (*message_block)[i];
        profile[i].output = (*message_block)[i + 1];
        profile[i].message_lock = (*lock_block)[i];
        profile[i].output_lock = (*lock_block)[i + 1];
        profile[i].message_item = &item[i];
        profile[i].message_space = &space[i];
        profile[i].output_item = &item[i + 1];
        profile[i].output_space = &space[i + 1];
    }
    output_t output = {
        .message = (*message_block)[length],
        .message_lock = (*lock_block)[length],
        .message_item = &item[length],
        .message_space = &space[length]
    };
    pthread_t cal_thread[length];
    for (long i = 0; i < length; ++i)
        if (pthread_create(&cal_thread[i], NULL, comparator, &profile[i]))
            perror("");
    pthread_t output_thread;
    if (pthread_create(&output_thread, NULL, successor, &output))
        perror("");
    struct timespec before = {0};
    clock_gettime(CLOCK_MONOTONIC, &before);
    int *input = &message[0];
    pthread_mutex_t *input_lock = &lock[0];
    for (long i = 0; i < length; ++i) {
        pthread_mutex_lock(input_lock);
        while (*input != CONSUMED)
            pthread_cond_wait(&space[0], input_lock);
        *input = rand();
        pthread_cond_signal(&item[0]);
        pthread_mutex_unlock(input_lock);
        next_message(&input, &message[0]);
        next_lock(&input_lock, &lock[0]);
    }
    for (int i = 0; i < 2; ++i) {
        pthread_mutex_lock(input_lock);
        while (*input != CONSUMED)
            pthread_cond_wait(&space[0], input_lock);
        *input = END;
        pthread_cond_signal(&item[0]);
        pthread_mutex_unlock(input_lock);
        next_message(&input, &message[0]);
        next_lock(&input_lock, &lock[0]);
    }
    for (long i = 0; i < length; ++i)
        pthread_join(cal_thread[i], NULL);
    pthread_join(output_thread, NULL);
    struct timespec after = {0};
    clock_gettime(CLOCK_MONOTONIC, &after);
    for (long i = 0; i < BUF_SIZE * (length + 1); ++i)
        pthread_mutex_destroy(&lock[i]);
    for (long i = 0; i < length + 1; ++i) {
        pthread_cond_destroy(&item[i]);
        pthread_cond_destroy(&space[i]);
    }
    double time = (double)(after.tv_sec - before.tv_sec) +
                  (double)(after.tv_nsec - before.tv_nsec) / 1e9;
    printf("Pipesort took: % .6e seconds \n", time);
    return 0;
}

static inline void next_message(int **now, int *head)
{
    *now = head + (*now - head + 1) % BUF_SIZE;
}

static inline void next_lock(pthread_mutex_t **now, pthread_mutex_t *head)
{
    *now = head + (*now - head + 1) % BUF_SIZE;
}

static void *comparator(void *profile)
{
    profile_t *pf = profile;
    int *message = pf->message;
    int *output = pf->output;
    pthread_mutex_t *message_lock = pf->message_lock;
    pthread_mutex_t *output_lock = pf->output_lock;
    pthread_mutex_lock(message_lock);
    if (*message == CONSUMED)
        pthread_cond_wait(pf->message_item, message_lock);
    int buf = *message;
    *message = CONSUMED;
    pthread_cond_signal(pf->message_space);
    pthread_mutex_unlock(message_lock);
    next_message(&message, pf->message);
    next_lock(&message_lock, pf->message_lock);
    pthread_mutex_lock(output_lock);
    if (*output != CONSUMED)
        pthread_cond_wait(pf->output_space, output_lock);
    pthread_mutex_lock(message_lock);
    if (*message == CONSUMED)
        pthread_cond_wait(pf->message_item, message_lock);
    while (*message != END) {
        if (*message <= buf) {
            *output = *message;
            pthread_cond_signal(pf->output_item);
            pthread_mutex_unlock(output_lock);
            *message = CONSUMED;
            pthread_cond_signal(pf->message_space);
            pthread_mutex_unlock(message_lock);
        } else {
            *output = buf;
            pthread_cond_signal(pf->output_item);
            pthread_mutex_unlock(output_lock);
            buf = *message;
            *message = CONSUMED;
            pthread_cond_signal(pf->message_space);
            pthread_mutex_unlock(message_lock);
        }
        next_message(&output, pf->output);
        next_lock(&output_lock, pf->output_lock);
        next_message(&message, pf->message);
        next_lock(&message_lock, pf->message_lock);
        pthread_mutex_lock(output_lock);
        if (*output != CONSUMED)
            pthread_cond_wait(pf->output_space, output_lock);
        pthread_mutex_lock(message_lock);
        if (*message == CONSUMED)
            pthread_cond_wait(pf->message_item, message_lock);
    }
    *output = *message;
    pthread_cond_signal(pf->output_item);
    pthread_mutex_unlock(output_lock);
    *message = CONSUMED;
    pthread_cond_signal(pf->message_space);
    pthread_mutex_unlock(message_lock);
    next_message(&output, pf->output);
    next_lock(&output_lock, pf->output_lock);
    next_message(&message, pf->message);
    next_lock(&message_lock, pf->message_lock);
    while (buf != END) {
        pthread_mutex_lock(output_lock);
        if (*output != CONSUMED)
            pthread_cond_wait(pf->output_space, output_lock);
        pthread_mutex_lock(message_lock);
        if (*message == CONSUMED)
            pthread_cond_wait(pf->message_item, message_lock);
        *output = buf;
        pthread_cond_signal(pf->output_item);
        pthread_mutex_unlock(output_lock);
        buf = *message;
        *message = CONSUMED;
        pthread_cond_signal(pf->message_space);
        pthread_mutex_unlock(message_lock);
        next_message(&output, pf->output);
        next_lock(&output_lock, pf->output_lock);
        next_message(&message, pf->message);
        next_lock(&message_lock, pf->message_lock);
    }
    pthread_mutex_lock(output_lock);
    if (*output != CONSUMED)
        pthread_cond_wait(pf->output_space, output_lock);
    *output = buf;
    pthread_cond_signal(pf->output_item);
    pthread_mutex_unlock(output_lock);
    return NULL;
}

static void *successor(void *output)
{
    output_t *op = output;
    int *message = op->message;
    pthread_mutex_t *message_lock = op->message_lock;
    pthread_mutex_lock(message_lock);
    if (*message == CONSUMED)
        pthread_cond_wait(op->message_item, message_lock);
    *message = CONSUMED;
    pthread_cond_signal(op->message_space);
    pthread_mutex_unlock(message_lock);
    next_message(&message, op->message);
    next_lock(&message_lock, op->message_lock);
    for (;;) {
        pthread_mutex_lock(message_lock);
        if (*message == CONSUMED)
            pthread_cond_wait(op->message_item, message_lock);
        if (*message == END) {
            pthread_mutex_unlock(message_lock);
            puts("");
            return NULL;
        }
        printf("%d ", *message);
        *message = CONSUMED;
        pthread_cond_signal(op->message_space);
        pthread_mutex_unlock(message_lock);
        next_message(&message, op->message);
        next_lock(&message_lock, op->message_lock);
    }
}
