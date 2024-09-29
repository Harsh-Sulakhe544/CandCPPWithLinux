#include <assert.h>
#include "thread_pool.h"

#include<stddef.h> // size_t 

// The consumer code : 
static void *worker(void *arg) {
    ThreadPool *tp = (ThreadPool *)arg;
    while (true) {
        pthread_mutex_lock(&tp->mu);
        // wait for the condition: a non-empty queue
        while (tp->queue.empty()) {
            pthread_cond_wait(&tp->not_empty, &tp->mu);
        }

        // got the job
        Work w = tp->queue.front();
        tp->queue.pop_front();
        pthread_mutex_unlock(&tp->mu);

        // do the work
        w.f(w.arg);
    }
    return NULL;
}

// The thread_pool_init is for initialization and starting threads. pthread types are initialized by pthread_xxx_init functions and the pthread_create starts a thread with the target function worker.
void thread_pool_init(ThreadPool *tp, size_t num_threads) {
    assert(num_threads > 0); // number f threads must be greater than 0 

    int rv = pthread_mutex_init(&tp->mu, NULL);
    assert(rv == 0);
    rv = pthread_cond_init(&tp->not_empty, NULL);
    assert(rv == 0);

    tp->threads.resize(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        int rv = pthread_create(&tp->threads[i], NULL, &worker, tp);
        assert(rv == 0);
    }
}

//The producer code:
void thread_pool_queue(ThreadPool *tp, void (*f)(void *), void *arg) {
    Work w;
    w.f = f;
    w.arg = arg;

    pthread_mutex_lock(&tp->mu);
    tp->queue.push_back(w);
    pthread_cond_signal(&tp->not_empty);
    pthread_mutex_unlock(&tp->mu);
}

