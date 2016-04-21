

#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <sys/types.h>

/* -- EXAMPLE --
void foo()
{
	char buf1[] = "hello";
	char buf2[6] = {0,0,0,0,0,0};
	
	i2c_masterInit();
	
	i2c_packet_t p1 = i2c_createPacket(0x42, 0x12, I2C_TX, buf1, strlen(buf1));
	i2c_packet_t p2 = i2c_createPacket(0x24, 0x1A, I2C_RX, buf2, 5);
	
	i2c_addToQueue(p1);
	i2c_addToQueue(p2);
	
	while(1)
	{
		i2c_packet_status_t p1_status, p2_status;
		
		// get most recent status
		p1_status = i2c_getPacketStatus(p1);
		p2_status = i2c_getPacketStatus(p2);
		
		
		if(p1_status == I2C_FAILED)
			// retry to send
			i2c_addToQueue(p1);
		else if(p1_status == I2C_FINISHED)
			// successfully sent!
			i2c_destroyPacket(p1);
		
		
		if(p2_status == I2C_FAILED)
			// give up 
			i2c_destroyPacket(p2)
		else if(p2_status == I2C_FINISHED)
			// successfully got data!
			printf("%s\n", buf2);
	}
}
*/

typedef enum i2c_packet_status
{
	I2C_CREATED,
	I2C_IN_QUEUE,
	I2C_PROCESSING,
	I2C_FINISHED,
	I2C_FAILED
} i2c_packet_status_t;

typedef enum i2c_direction
{
	I2C_TX, I2C_RX
} i2c_direction_t;

// opaque i2c_packet for public access
typedef struct i2c_packet * i2c_packet_t;

void i2c_masterInit();
i2c_packet_t i2c_createPacket(uint8_t addr, uint8_t subAddr, i2c_direction_t direction,
		const uint8_t data[], uint8_t length);
void i2c_destroyPacket(i2c_packet_t p);
ssize_t i2c_addToQueue(i2c_packet_t p);
i2c_packet_status_t i2c_getPacketStatus(i2c_packet_t p);

#endif

