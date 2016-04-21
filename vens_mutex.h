
#ifndef MUTEX_H
#define MUTEX_H

// there are better ways to do a mutex, but this is the easiest.

extern int interrupt_en;

inline void mutex_lock()
{
	interrupt_en = SR & GIE; 	// save the current state of the GIE
	SR &= ~GIE;					// disable interrupts
}
inline void mutex_unlock()
{
	SR |= interrupt_en;			// restore interrupts to their state before lock
} 

#endif

