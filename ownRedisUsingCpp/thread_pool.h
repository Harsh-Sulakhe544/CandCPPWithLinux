#pragma once

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <vector>
#include <deque>


struct Work {
    void (*f)(void *) = NULL; // Function pointer for the callback
    void *arg = NULL;         // Argument to be passed to the callback
};

struct ThreadPool {
    std::vector<pthread_t> threads; // Vector to hold thread identifiers
    std::deque<Work> queue;          // Queue to hold work items
    pthread_mutex_t mu;              // Mutex for synchronizing access to the queue
    pthread_cond_t not_empty;        // Condition variable to signal when the queue is not empty
};


void thread_pool_init(ThreadPool *tp, size_t num_threads);
void thread_pool_queue(ThreadPool *tp, void (*f)(void *), void *arg);
