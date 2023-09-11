/**
  ******************************************************************************
  * @file           : bus_packet.h
  * @brief          : Encoder and decoder for bus packet FyCUS 2023
  *
  * @author         Rubén Torres Bermúdez <rubentorresbermudez@gmail.com>
  ******************************************************************************
  * @attention
  *
  *  Created on:     11.08.2023
  *
  *  Description:
  *		This library is used to encode, packetize, decode and sync data to
  *		communicate different systems connected to the internal bus of
  *		FyCUS 2023 prototype.
  *
  *	 Example:
  *	 	bus_packet_t packet = {0};
  *	 	uint8_t data[BUS_PACKET_DATA_SIZE] = {100,1,12,234,34,3};
  *	 	buffer_in[BUS_PACKET_BUS_SIZE] = {0};
  *	 	bus_packet_EncodePacketize(1, 90, 1, data, 6, buffer_in);
  *		if (bus_packet_Decode(buffer_in, &packet) != HAL_OK)
  *			Error_Handle();
  *		// Approximated time for max buffer: 8330 clock cycles
  *
  *
  *	 Warning:
  *		With STM32, define your own CRC handle and make sure that is correctly
  *		configured:
  *	  		bus_packet_CRC16CCSDSConfig();
  *
  *	  	- Poly 16 bits: X^16 + X^12 + X^5 + 1
  *	  	- 8 bits input
  *	  	- Input and output without bit inversion
  *
  ******************************************************************************
  */

#ifndef INC_BUS_PACKET_H_
#define INC_BUS_PACKET_H_

#ifdef __cplusplus
extern "C" {
#endif

#define	STM32_MCU	// Comment this define if crc16 is not computed in STM32 MCU



#ifdef STM32_MCU
#include "main.h"
#endif

#include <stdint.h>
#include <string.h>


#ifdef STM32_MCU
extern CRC_HandleTypeDef hcrc;  // In STM32, define your own CRC handle
#endif



#ifndef STM32_MCU
#define HAL_OK      0x00
#define HAL_ERROR   0x01
#define HAL_BUSY    0x02
#define HAL_TIMEOUT 0x03
#define HAL_StatusTypeDef uint8_t
#endif

#define BUS_PACKET_BUS_SIZE			127
#define BUS_PACKET_ECF_SIZE			2
#define BUS_PACKET_HEADER_SIZE		2
#define BUS_PACKET_DATA_SIZE		(BUS_PACKET_BUS_SIZE-BUS_PACKET_ECF_SIZE-BUS_PACKET_HEADER_SIZE)
#define BUS_PACKET_FRAME_SYNC_SIZE	4

#define BUS_PACKET_TYPE_TM			0
#define BUS_PACKET_TYPE_TC			1

#define BUS_PACKET_ECF_NOT_EXIST	0
#define BUS_PACKET_ECF_EXIST		1


typedef struct
{
	uint8_t packet_type;
	uint8_t apid;
	uint8_t ecf_flag;
	uint8_t length;
	uint8_t data[BUS_PACKET_DATA_SIZE];
	uint16_t ecf;
}bus_packet_t;


typedef enum
{
	BUS_PACKET_SYNC_FIND 		= 0b00000001,
	BUS_PACKET_SYNC_2 			= 0b00000010,
	BUS_PACKET_SYNC_3			= 0b00000100,
	BUS_PACKET_SYNC_4			= 0b00001000,
	BUS_PACKET_SYNC_COMPLETED 	= 0b00010000,
}bus_sync_flag_t;


extern const uint8_t BUS_PACKET_FRAME_SYNC[4];






#ifndef STM32_MCU
uint16_t bus_packet_CRC16CCSDSCalculate(int16_t seed, uint8_t *buf, uint32_t len);
#else
HAL_StatusTypeDef bus_packet_CRC16CCSDSConfig();
#endif

HAL_StatusTypeDef bus_packet_Decode(uint8_t *buffer, bus_packet_t *packet);
HAL_StatusTypeDef bus_packet_Encode(uint8_t type, uint8_t apid, uint8_t ecf_flag, uint8_t *data, uint32_t length, bus_packet_t *packet);
void bus_packet_Packetize(uint8_t *buffer, bus_packet_t *packet);
HAL_StatusTypeDef bus_packet_EncodePacketize(uint8_t type, uint8_t apid, uint8_t ecf_flag, uint8_t *data, uint32_t data_length, uint8_t *buffer_out);

bus_sync_flag_t bus_packet_SyncFrameDetect(bus_sync_flag_t flag, uint8_t received_data);

static inline uint8_t bus_packet_GetLength(uint8_t *buffer) {return buffer[1]&0b01111111;}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* INC_BUS_PACKET_H_ */
