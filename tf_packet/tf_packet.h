/**
  ******************************************************************************
  * @file           : tf_packet.h
  * @brief          : Encoder and decoder for TF (USLP) FyCUS 2023
  *
  * @author         Rubén Torres Bermúdez <rubentorresbermudez@gmail.com>
  ******************************************************************************
  * @attention
  *
  *  Created on:     20.08.2023
  *
  *  Description:
  *		This library is used to packetize and decode data to communicate the
  *		prototype with ground station in FyCUS 2023.
  *
  *	 Example:
  *	 	uint8_t data[TF_PACKET_DATA_MAX_SIZE] = {100,13,32,76,12,98,34,12,65,23};
  *	    tfph.end_flag = TF_PACKET_NOT_TRUNCATED;
  *	    tfph.tfvn = TF_PACKET_TFVN;
  *	    tfph.mapid = TF_PACKET_DEFAULT_MAP;
  *	    tfph.vcid = TF_PACKET_DEFAULT_VCID;
  *	    tfph.scid = TF_PACKET_DEFAULT_SCID;
  *	    tfph.source_dest_id = TF_PACKET_SOURCE;
  *	    tf_packet_SetData(data, 10, NULL, 0, &tfph, &tfdf);
  *	    tf_packet_Packetize(0, &tfph, &tfdf, tf_buffer_out);
  *	    if(tf_packet_Decode(tf_buffer_out, tfph.length, &tfph, &tfdf) != HAL_OK)
  *	    {
  *	        Error_Handler();
  *	    }
  *		// Approximated time for max buffer: 8830 clock cycles
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
  *
  *  Copyright (C) 2023 Rubén Torres Bermúdez
  ******************************************************************************
  */

#ifndef INC_TF_PACKET_H_
#define INC_TF_PACKET_H_

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


#define TF_PACKET_MAX_SIZE						256
#define TF_PACKET_ECF_SIZE						2
#define TF_PACKET_PRIMARY_TRUNCATED_HEADER_SIZE	4
#define TF_PACKET_PRIMARY_BASE_HEADER_SIZE		7
#define TF_PACKET_DATA_HEADER_SIZE				1
#define TF_PACKET_VCDATA_MAX_SIZE				56
#define TF_PACKET_DATA_MAX_SIZE					(TF_PACKET_MAX_SIZE-TF_PACKET_PRIMARY_TRUNCATED_HEADER_SIZE-TF_PACKET_DATA_HEADER_SIZE-TF_PACKET_ECF_SIZE)


#define TF_PACKET_TFVN			0b1100
#define TF_PACKET_DEFAULT_SCID	0x5553
#define TF_PACKET_DEFAULT_VCID	0b111000
#define TF_PACKET_DEFAULT_MAPID	0b0000

#define TF_PACKET_SOURCE		0
#define TF_PACKET_DESTINATION	1

#define TF_PACKET_NOT_TRUNCATED	0
#define TF_PACKET_TRUNCATED		1

#define TF_PACKET_SEQUENCE_CONTROLLED	0
#define TF_PACKET_EXPEDITED				1

#define TF_PACKET_USER_DATA		0
#define TF_PACKET_INFO			1

#define TF_PACKET_OCF_EXIST			0
#define TF_PACKET_OCF_NOT_EXIST		1

#define TF_PACKET_DEFAULT_PROTOCOL_ID	0b00000000
#define TF_PACKET_DEFAULT_CONSTR_RULE	0b00000111


typedef struct
{
	uint8_t tfvn;
	uint16_t scid;
	uint8_t source_dest_id;
	uint8_t vcid;
	uint8_t mapid;
	uint8_t end_flag;
	uint16_t length;
	uint8_t bypass_flag;
	uint8_t command_flag;
	uint8_t ocf_flag;
	uint8_t vc_length;
	uint8_t vc_frame[TF_PACKET_VCDATA_MAX_SIZE];
}tfph_packet_t;


typedef struct
{
	uint8_t constr_rule;
	uint8_t protocol_id;
	uint8_t data[TF_PACKET_DATA_MAX_SIZE];
}tfdf_packet_t;







#ifndef STM32_MCU
uint16_t tf_packet_CRC16CCSDSCalculate(int16_t seed, uint8_t *buf, uint32_t len);
#else
HAL_StatusTypeDef tf_packet_CRC16CCSDSConfig();
#endif

HAL_StatusTypeDef tf_packet_Decode(uint8_t *buffer_in, uint32_t buffer_length,  tfph_packet_t *tfph, tfdf_packet_t *tfdf);
HAL_StatusTypeDef tf_packet_Packetize(uint8_t data_length, tfph_packet_t *tfph, tfdf_packet_t *tfdf, uint8_t *buffer_out);
HAL_StatusTypeDef tf_packet_SetData(uint8_t *data, uint8_t data_length, uint8_t *VCdata, uint8_t VCdata_length, tfph_packet_t *tfph, tfdf_packet_t *tfdf);


#ifdef __cplusplus
} // extern "C"
#endif


#endif /* INC_TF_PACKET_H_ */
