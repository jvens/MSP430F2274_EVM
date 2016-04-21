

#ifndef FIFO_H
#define FIFO_H

#define FIFO_BUFFER_SIZE 16
#include <sys/types.h>

typedef struct fifo* fifo_t;


/**
 * Create a new fifo object
 */
struct fifo* fifo_init();
/**
 * Destroy a previously created fifo object
 */
void fifo_destroy(fifo_t *);

/**
 * Push data to the fifo.
 * @param f the fifo to push to
 * @param d the data to push
 * @return the number of slots currenlty in the FIFO (after the push) or -1 if the FIFO was full
 *         and the push failed.
 */
ssize_t fifo_push(fifo_t* f, void * d);

/**
 * Pop data from the fifo.
 * @param f the fifo to pop from
 * @param d address to store the poped data in
 * @return the number of items currently in the fifo (after the pop) or -1 if the FIFO was empty
 *         and the pop failed.
 */
ssize_t fifo_pop(fifo_t* f, void ** d);

#endif

