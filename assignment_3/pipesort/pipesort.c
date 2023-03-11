#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

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
    END = -2
};

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
    int message[length + 1];
    pthread_mutex_t lock[length + 1];
    pthread_cond_t item[length + 1];
    pthread_cond_t space[length + 1];
    for (long i  = 0; i < length + 1; ++i) {
        message[i] = CONSUMED;
        pthread_mutex_init(&lock[i], NULL);
        pthread_cond_init(&item[i], NULL);
        pthread_cond_init(&space[i], NULL);
    }
    profile_t profile[length];
    for (long i = 0; i < length; ++i) {
        profile[i].message = &message[i];
        profile[i].output = &message[i + 1];
        profile[i].message_lock = &lock[i];
        profile[i].output_lock = &lock[i + 1];
        profile[i].message_item = &item[i];
        profile[i].message_space = &space[i];
        profile[i].output_item = &item[i + 1];
        profile[i].output_space = &space[i + 1];
    }
    output_t output = {
        .message = &message[length],
        .message_lock = &lock[length],
        .message_item = &item[length],
        .message_space = &space[length]
    };
    pthread_t cal_thread[length];
    for (long i = 0; i < length; ++i)
        pthread_create(&cal_thread[i], NULL, comparator, &profile[i]);
    pthread_t output_thread;
    pthread_create(&output_thread, NULL, successor, &output);
    struct timespec before = {0};
    clock_gettime(CLOCK_MONOTONIC, &before);
    for (long i = 0; i < length; ++i) {
        pthread_mutex_lock(&lock[0]);
        while (message[0] != CONSUMED)
            pthread_cond_wait(&space[0], &lock[0]);
        message[0] = rand();
        pthread_cond_signal(&item[0]);
        pthread_mutex_unlock(&lock[0]);
    }
    for (int i = 0; i < 2; ++i) {
        pthread_mutex_lock(&lock[0]);
        while (message[0] != CONSUMED)
            pthread_cond_wait(&space[0], &lock[0]);
        message[0] = END;
        pthread_cond_signal(&item[0]);
        pthread_mutex_unlock(&lock[0]);
    }
    for (long i = 0; i < length; ++i)
        pthread_join(cal_thread[i], NULL);
    pthread_join(output_thread, NULL);
    struct timespec after = {0};
    clock_gettime(CLOCK_MONOTONIC, &after);
    for (long i = 0; i < length + 1; ++i) {
        pthread_mutex_destroy(&lock[i]);
        pthread_cond_destroy(&item[i]);
        pthread_cond_destroy(&space[i]);
    }
    double time = (double)(after.tv_sec - before.tv_sec) +
                  (double)(after.tv_nsec - before.tv_nsec) / 1e9;
    printf("Pipesort took: % .6e seconds \n", time);
    return 0;
}

static void *comparator(void *profile)
{
    profile_t *pf = profile;
    pthread_mutex_lock(pf->message_lock);
    if (*pf->message == CONSUMED)
        pthread_cond_wait(pf->message_item, pf->message_lock);
    int buf = *pf->message;
    *pf->message = CONSUMED;
    pthread_cond_signal(pf->message_space);
    pthread_mutex_unlock(pf->message_lock);
    pthread_mutex_lock(pf->output_lock);
    if (*pf->output != CONSUMED)
        pthread_cond_wait(pf->output_space, pf->output_lock);
    pthread_mutex_lock(pf->message_lock);
    if (*pf->message == CONSUMED)
        pthread_cond_wait(pf->message_item, pf->message_lock);
    while (*pf->message != END) {
        if (*pf->message <= buf) {
            *pf->output = *pf->message;
            pthread_cond_signal(pf->output_item);
            pthread_mutex_unlock(pf->output_lock);
            *pf->message = CONSUMED;
            pthread_cond_signal(pf->message_space);
            pthread_mutex_unlock(pf->message_lock);
        } else {
            *pf->output = buf;
            pthread_cond_signal(pf->output_item);
            pthread_mutex_unlock(pf->output_lock);
            buf = *pf->message;
            *pf->message = CONSUMED;
            pthread_cond_signal(pf->message_space);
            pthread_mutex_unlock(pf->message_lock);
        }
        pthread_mutex_lock(pf->output_lock);
        if (*pf->output != CONSUMED)
            pthread_cond_wait(pf->output_space, pf->output_lock);
        pthread_mutex_lock(pf->message_lock);
        if (*pf->message == CONSUMED)
            pthread_cond_wait(pf->message_item, pf->message_lock);
    }
    *pf->output = *pf->message;
    pthread_cond_signal(pf->output_item);
    pthread_mutex_unlock(pf->output_lock);
    *pf->message = CONSUMED;
    pthread_cond_signal(pf->message_space);
    pthread_mutex_unlock(pf->message_lock);
    while (buf != END) {
        pthread_mutex_lock(pf->output_lock);
        if (*pf->output != CONSUMED)
            pthread_cond_wait(pf->output_space, pf->output_lock);
        pthread_mutex_lock(pf->message_lock);
        if (*pf->message == CONSUMED)
            pthread_cond_wait(pf->message_item, pf->message_lock);
        *pf->output = buf;
        pthread_cond_signal(pf->output_item);
        pthread_mutex_unlock(pf->output_lock);
        buf = *pf->message;
        *pf->message = CONSUMED;
        pthread_cond_signal(pf->message_space);
        pthread_mutex_unlock(pf->message_lock);
    }
    pthread_mutex_lock(pf->output_lock);
    if (*pf->output != CONSUMED)
        pthread_cond_wait(pf->output_space, pf->output_lock);
    *pf->output = buf;
    pthread_cond_signal(pf->output_item);
    pthread_mutex_unlock(pf->output_lock);
    return NULL;
}

static void *successor(void *output)
{
    output_t *op = output;
    pthread_mutex_lock(op->message_lock);
    if (*op->message == CONSUMED)
        pthread_cond_wait(op->message_item, op->message_lock);
    *op->message = CONSUMED;
    pthread_cond_signal(op->message_space);
    pthread_mutex_unlock(op->message_lock);
    for (;;) {
        pthread_mutex_lock(op->message_lock);
        if (*op->message == CONSUMED)
            pthread_cond_wait(op->message_item, op->message_lock);
        if (*op->message == END) {
            pthread_mutex_unlock(op->message_lock);
            puts("");
            return NULL;
        }
        printf("%d ", *op->message);
        *op->message = CONSUMED;
        pthread_cond_signal(op->message_space);
        pthread_mutex_unlock(op->message_lock);
    }
}
