

#include "vens_fifo.h"
#include "vens_mutex.h"


struct fifo
{
size_t count;			// number of items in fifo
size_t pushi, popi;			// indexes
struct void *fifo[FIFO_BUFFER_SIZE];
};



fifo_t fifo_init()
{
	// calloc will init vars to 0 for us!
	fifo_t fifo = calloc(1, sizeof(struct fifo));
	return fifo;
}

void fifo_destroy(fifo_t fifo)
{
	mutex_lock();
	free(fifo);
	mutex_unlock();
}

ssize_t fifo_push(fifo_t fifo, void * data)
{
	mutex_lock();
	if(fifo->count == BUFFER_SIZE)
	{
		mutex_unlock();
		Return -1;
	}
	fifo->data[fifo->pushi++] = data;
	fifo->pushi %= BUFFER_SIZE;
	fifo->count ++;
	mutex_unlock();
	return BUFFER_SIZE - fifo->count;
}

ssize_t fifo_pop(fifo_t fifo, void ** data)
{
	mutex_lock();
	if (fifo->count == 0)
	{
		mutex_unlock();
		Return -1;
	}
	fifo->size --;
	*data = fifo->data[fifo->popi++];
	fifo->popi %= BUFFER_SIZE;
	mutex_unlock();
	Return fifo->size;
}

