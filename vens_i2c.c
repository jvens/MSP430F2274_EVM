#include <stdint.h>
#include <stdlib.h>	// for malloc

#include "vens_fifo.h"
#include "vens_i2c.h"
#include "vens_mutex.h"

//-- DATA TYPES --//

struct i2c_packet
{
	const uint8_t addr;
	const uint8_t subAddr;
	const uint8_t *data;
	const size_t length;
	uint8_t curPos;
	i2c_direction_t direction;
	i2c_packet_status_t *status;
};

//-- LOCAL VARIABLES --//

/// the i2c fifo
static volatile struct fifo * i2c_fifo;

/// flag for if the i2c is currently transmitting
static volatile int i2c_isTransmitting = 0;

/// the i2c_packet currently being transmitted
static volatile i2c_packet_t i2c_cur_packet = NULL;

//-- PROTOTYPES --//
static void i2c_startTransacting();
static void i2c_onIsr();

//-- INTERRUPTS --//
#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCIAB0TX_ISR()
{
	i2c_onIsr();	// call onIsr().  Must be another function so we can call from both ISRs and 
					// mainline code.
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCIAB0RX_ISR()
{
	// if we recieived a NACK
	if(UCB0STAT & UCNACKIFG)
	{
		P3OUT |= BIT4;
		UCB0STAT &= ~UCNACKIFG;	// clear NACK flag
		if(i2c_cur_packet)
		{
			i2c_cur_packet->status = I2C_FAILED;
			// send a the next packet in the FIFO
			i2c_onIsr();
		}
	}
}

// -- FUNCTIONS --//

// init the I2C module
void i2c_masterInit()
{
	// create the I2C fifo
	i2c_fifo = fifo_init();
	
	//Master, I2C, Synchronous
	UCB0CTL1 |= UCSWRST;
	UCB0CTL0 |= UCMST | UCMODE1 | UCMODE0 | UCSYNC;

	//SMCLK, Transmitter,
	UCB0CTL1 |= UCSSEL1 | UCTR;
	UCB0BR0 = 10; //Divide by 10 prescalar -- 1MHZ SMCLK

	//config ports
	P3SEL |= BIT1 | BIT2; //See device specific datasheet for selecting alternate pin functions

	//Undo Reset
	UCB0CTL1 &= ~UCSWRST;

	//Interrupt Enable
	UCB0I2CIE |=  UCNACKIE;	//Start Condition and Nack IE
	IE2 |= UCB0TXIE | UCB0RXIE;			//TX ISR -- TX/RX Buffer
										//RX ISR -- I2C Status information
	//TODO: Enable Interupts
}

/**
 * Create a new I2C packet from the given data
 */
i2c_packet_t i2c_createPacket(const uint8_t addr, const uint8_t subAddr, 
		const i2c_direction_t direction, const uint8_t data[], size_t length)
{
	i2c_packet_t p = malloc(sizeof(struct i2c_packet));
	p->addr = addr;
	p->subAddr = subAddr;
	p->direction = direction;
	p->data = data;
	p->length = length;
	p->status = I2C_CREATED;
	return p;
}

/**
 * Destroy a packet created by i2c_createPacket;
 */
void i2c_destroyPacket(i2c_packet_t p)
{
	free(p);
}


/**
 * add a packet created by i2c_createPacket() to the tx fifo queue.
 * @returns -1 on failure or number of items in queue on success
 */
ssize_t i2c_addToQueue(i2c_packet_t p)
{
	if(p->status == I2C_IN_QUEUE || p->status == I2C_TRANSMITTING)
		return -1;
	
	p->curPos = 0;
	ssize_t rv = fifo_push(i2c_fifo, p);
	if(rv == -1)
	{
		// fifo was full… handle that case
		return rv;
	}
	p->status = I2C_IN_QUEUE;
	i2c_startTransacting();
	return rv;
}

i2c_packet_status_t i2c_getPacketStatus(i2c_packet_t p)
{
	return p->status;
}

/**
 * Ensure that the i2c is transmitting data
 */
static void i2c_startTransacting()
{
	mutex_lock();// needed to make sure the i2c doesn't finish after we check
	if(i2c_isTransmiting)
	{
		// the i2c is already transmitting, nothing to do
		Return 0;
	}
	else
	{
		i2c_isTransmitting = 1;	
		// call the onIsr() to to start the transmittion
		i2c_onIsr();
	}
	mutex_unlock();
}

/**
 * Called each time the I2C recieives or isis ready to send another byte.
 */
static void i2c_onIsr()
{
	static enum {ADDRESS, SUBADDRESS, REPEAT_START, TX_DATA, RX_DATA, DONE} step = DONE;
	
	if(i2c_cur_packet == NULL || 					// if there is not a packet to send
			i2c_cur_packet->status == I2C_SENT ||	// if the packet has finished sending
			i2c_cur_packet->status == I2C_FAILED)	// if the packet failed to send
	{
		// get a new packet from the FIFO
		i2c_packet_t p;
		ssize_t rv = fifo_pop(i2c_fifo, &p);
		if(rv < 0)
		{
			// nothing in the fifo, 
			i2c_cur_packet = NULL;
			step = DONE;
			i2c_isTransmitting = 0;
			UCB0CTL1 |= UCTXSTP; // send NACK followed by Stop Condition
			IFG2 &= ~UCB0TXIFG;	// clear the interrupt flag
			return;
		}
		else
		{
			// set the status of the packet
			*i2c_cur_packet->status = I2C_PROCESSING;
			step = ADDRESS;						// set next step to send address
		}
	}
	
	// enter the state machine
	switch(step)
	{
		// send the address and start bit
		case ADDRESS:
			UCB0I2CSA = i2c_cur_packet->addr; 	// set address
			UCB0CTL1 |= UCTR; 					// Set in tx mode
			UCB0CTL1 |= UCTXSTT;				// generate start condition
			step = SUBADDRESS; 					// set next state to send subaddress
			break;
		
		// send the subaddress of the register to access
		case SUBADDRESS:
			UCB0TXBUF = i2c_cur_packet->subAddr;	// send the sub address
			if(i2c_cur_packet->direction == I2C_TX)
				step = DATA;					// set next state to start sending data
			else
				step = REPEAT_START;			// set next state to switch to rx mode
			break;
			
		// switch to Rx mode and send repeated start command
		case REPEAT_START:
			UCB0CTL1 &= ~UCTR;					// switch to Rx mode
			UCB0CTL1 |= UCSTTIFG;				// Send Repeated start
			step = RX_DATA;						// set next state to start receiving data
			
		// send data to the slave
		case TX_DATA:
			// send the next byte
			UCB0TXBUF = i2c_cur_packet->data[i2c_cur_packet->curPos++];
			if(i2c_cur_packet->curPos == i2c_cur_packet->length) // if this was the last byte
			{
				*i2c_cur_packet->status = I2C_FINISHED;	// set status to sent
				step = DONE;					// set next step to done
			}
			break;
			
		// receive data from slave
		case RX_DATA:
			// get the byte
			i2c_cur_packet->data[i2c_cur_packet->curPos++] = UCB0RXBUF;
			if(i2c_cur_packet->curPos == i2c_cur_packet->length) // if this was the last byte
			{
				*i2c_cur_packet->status = I2C_FINISHED;
				step = DONE;	// set next step
				// recurse into self to start next packet
				i2c_onIsr();
			}
		
		// the packet is finshed.  Never should get here
		case DONE:
		default:
			// WHAT IS HAPPENING, HOW DID I GET HERE?
			break;
	}
	return;
}



