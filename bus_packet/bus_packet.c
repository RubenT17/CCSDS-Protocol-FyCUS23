/**
  ******************************************************************************
  * @file           : bus_packet.c
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
  *
  *  Copyright (C) 2023 Rubén Torres Bermúdez
  *
  ******************************************************************************
  */

#include "bus_packet.h"


const uint8_t BUS_PACKET_FRAME_SYNC[4] = {0x1A, 0xCF, 0xFC, 0x1D};


/**
 * Decode data buffer that contain a bus packet
 * @param buffer Data buffer with a bus packet to decode
 * @param packet Pointer to bus packet structure to save data
 * @return HAL status
 */
HAL_StatusTypeDef bus_packet_Decode(uint8_t *buffer, bus_packet_t *packet)
{
	uint8_t length = buffer[1] & 0b01111111;
	uint8_t ecf_flag = (buffer[1] & 0b10000000)>>7;

	if(length-BUS_PACKET_ECF_SIZE-BUS_PACKET_HEADER_SIZE < 0)	return HAL_ERROR;

	if (ecf_flag)	// If there is CRC, check it.
	{
		if(length-BUS_PACKET_ECF_SIZE < 0)		return HAL_ERROR;
		uint8_t crc_data[length-BUS_PACKET_ECF_SIZE];
		memcpy(crc_data, buffer, length-BUS_PACKET_ECF_SIZE);

#ifdef STM32_MCU
		uint16_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)crc_data, length-BUS_PACKET_ECF_SIZE) & 0xFFFF;
#else
		uint16_t calculated_crc = bus_packet_CRC16CCSDSCalculate(0, crc_data, length-BUS_PACKET_ECF_SIZE) & 0xFFFF;
#endif
		uint16_t ecf = buffer[length-BUS_PACKET_ECF_SIZE]<<8 | buffer[length-BUS_PACKET_ECF_SIZE+ 1];

		if(calculated_crc == ecf)	packet->ecf = ecf;
		else 						return HAL_ERROR;
	}

	// Save data
	packet->packet_type = buffer[0]>>7;
	packet->apid = buffer[0] & 0b01111111;
	packet->ecf_flag = ecf_flag;
	packet->length = length;

	memcpy(packet->data, &buffer[BUS_PACKET_HEADER_SIZE], length-BUS_PACKET_ECF_SIZE-BUS_PACKET_HEADER_SIZE);

	return HAL_OK;
}


/**
 * Encode data into a bus packet structure
 * @param type
 * 		@arg BUS_PACKET_TYPE_TM if a TM data is contained
 * 		@arg BUS_PACKET_TYPE_TC if a TM data is contained
 * @param apid APID number for contained data
 * @param ecf_flag
 * 		@arg BUS_PACKET_ECF_NOT_EXIST if an Error Control Field will be not encoded
 * 		@arg BUS_PACKET_ECF_EXIST if an Error Control Field will be encoded
 * @param data Pointer to data that will be encoded
 * @param data_length Data length
 * @param packet Pointer to bus packet structure to save data
 * @return HAL status
 */
HAL_StatusTypeDef bus_packet_Encode(uint8_t type, uint8_t apid, uint8_t ecf_flag, uint8_t *data, uint32_t data_length, bus_packet_t *packet)
{
	if((data_length+BUS_PACKET_HEADER_SIZE+BUS_PACKET_ECF_SIZE) > BUS_PACKET_BUS_SIZE)
		return HAL_ERROR;

	packet->packet_type = type & 0x01;
	packet->apid = apid & 0b01111111;
	packet->ecf_flag = ecf_flag & 0x01;
	packet->length = data_length + BUS_PACKET_HEADER_SIZE;

	// Save data
	memcpy(packet->data, data, data_length);
	memset(&packet->data[data_length], 0, BUS_PACKET_DATA_SIZE-data_length);

	if (packet->ecf_flag)	// There is CRC?
	{
		packet->length += BUS_PACKET_ECF_SIZE;

		uint8_t crc_data[packet->length-BUS_PACKET_ECF_SIZE];
		crc_data[0] = (packet->packet_type<<7) | (packet->apid & 0b01111111);
		crc_data[1] = (packet->ecf_flag<<7) | (packet->length & 0b01111111);

		memcpy(&crc_data[2], packet->data, data_length);

#ifdef STM32_MCU
		packet->ecf = HAL_CRC_Calculate(&hcrc, (uint32_t *)crc_data, packet->length-BUS_PACKET_ECF_SIZE) & 0xFFFF;
#else
		packet->ecf = bus_packet_CRC16CCSDSCalculate(0, crc_data, packet->length-BUS_PACKET_ECF_SIZE) & 0xFFFF;
#endif
	}

	return HAL_OK;
}


/**
 * Packetize an encoded space packet to be transmitted
 * @param buffer Pointer to a data buffer for to be transmitted
 * @param packet Pointer to a space packet to be packetized
 */
void bus_packet_Packetize(uint8_t *buffer, bus_packet_t *packet)
{

	buffer[0] = (packet->packet_type<<7) | (packet->apid & 0b01111111);
	buffer[1] = (packet->ecf_flag<<7) | (packet->length & 0b01111111);


	if (packet->ecf_flag)
	{
		memcpy(&buffer[BUS_PACKET_HEADER_SIZE], packet->data, packet->length-BUS_PACKET_ECF_SIZE);

		buffer[packet->length-BUS_PACKET_ECF_SIZE] = packet->ecf>>8;
		buffer[packet->length-BUS_PACKET_ECF_SIZE+1] = packet->ecf & 0xFF;
	}
	else
		memcpy(&buffer[BUS_PACKET_HEADER_SIZE], packet->data, packet->length);

	buffer[packet->length] = 0;		// String terminator
}

/**
 * Encode and packetize data into a buffer for to be transmitted
 * @param type
 * 		@arg BUS_PACKET_TYPE_TM if a TM data is contained
 * 		@arg BUS_PACKET_TYPE_TC if a TM data is contained
 * @param apid APID number for contained data
 * @param ecf_flag
 * 		@arg BUS_PACKET_ECF_NOT_EXIST if an Error Control Field will be not encoded
 * 		@arg BUS_PACKET_ECF_EXIST if an Error Control Field will be encoded
 * @param data Pointer to data that will be encoded
 * @param data_length Data length
 * @param buffer_out Pointer to a data buffer for to be transmitted
 * @return HAL status
 */
HAL_StatusTypeDef bus_packet_EncodePacketize(uint8_t type, uint8_t apid, uint8_t ecf_flag, uint8_t *data, uint32_t data_length, uint8_t *buffer_out)
{
	uint32_t length = data_length + BUS_PACKET_HEADER_SIZE + BUS_PACKET_ECF_SIZE;

	if(length > BUS_PACKET_BUS_SIZE)	return HAL_ERROR;

	memcpy(&buffer_out[BUS_PACKET_HEADER_SIZE], data, data_length);


	buffer_out[0] = (type<<7) | (apid & 0b01111111);
	buffer_out[1] = (ecf_flag<<7) | (length & 0b01111111);

	if (ecf_flag)	// There is CRC?
	{
#ifdef STM32_MCU
		uint16_t ecf = HAL_CRC_Calculate(&hcrc, (uint32_t *)buffer_out, length-BUS_PACKET_ECF_SIZE);
#else
		uint16_t ecf = bus_packet_CRC16CCSDSCalculate(0, buffer_out, length-BUS_PACKET_ECF_SIZE);
#endif
		buffer_out[length-BUS_PACKET_ECF_SIZE] = ecf>>8;
		buffer_out[length-BUS_PACKET_ECF_SIZE+1] = ecf & 0xFF;
	}

	return HAL_OK;
}


/**
 * Detect next flag sync based on the input flag and input data
 * @param flag Last flag of your Sync frame
 * @param received_data Current data received
 * @return Current flag of your Sync frame
 */
bus_sync_flag_t bus_packet_SyncFrameDetect(bus_sync_flag_t flag, uint8_t received_data)
{
  switch(flag)
  {
  case BUS_PACKET_SYNC_FIND:
  default:
    _first_sync_byte:
    if (received_data != BUS_PACKET_FRAME_SYNC[0])
      flag = BUS_PACKET_SYNC_FIND;
    else flag = (bus_sync_flag_t)(flag << 1);
    break;

  case BUS_PACKET_SYNC_2:
    if (received_data != BUS_PACKET_FRAME_SYNC[1])
    {
      goto _first_sync_byte;
    }
    else flag = (bus_sync_flag_t)(flag << 1);
    break;

  case BUS_PACKET_SYNC_3:
    if (received_data != BUS_PACKET_FRAME_SYNC[2])
    {
      goto _first_sync_byte;
    }
    else flag = (bus_sync_flag_t)(flag << 1);
    break;

  case BUS_PACKET_SYNC_4:
    if (received_data != BUS_PACKET_FRAME_SYNC[3])
    {
      goto _first_sync_byte;
    }
    else flag = (bus_sync_flag_t)(flag << 1);
    break;
  }
  return flag;


  /*
  		EXAMPLE FOR SYNC DATA

  void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
  {
  	static uint8_t buffer_pos = 0;
  	static bus_sync_flag_t  sync_flag = BUS_PACKET_SYNC_FIND;
  	static uint16_t packet_length = 0;

  	if (sync_flag != BUS_PACKET_SYNC_COMPLETED)
  	{
  		sync_flag = bus_packet_SyncFrameDetect(sync_flag, buffer_in[buffer_pos]);

  		if(sync_flag == BUS_PACKET_SYNC_COMPLETED)
  		{
  			buffer_pos=0;
  			HAL_UART_Receive_IT(huart, &buffer_in[buffer_pos], 1);
  		}
  		else
  		{
  			buffer_pos++;
  			if(buffer_pos >= BUS_PACKET_BUS_SIZE)
  				buffer_pos=0;

  			HAL_UART_Receive_IT(huart, &buffer_in[buffer_pos], 1);
  		}
  	}

  	else
  	{
  		if(buffer_pos > 0)
  		{
  			packet_length = buffer_in[1] & 0b01111111;

  			if (buffer_pos >= (packet_length-1))
  			{
  				buffer_pos = 0;
  				sync_flag = BUS_PACKET_SYNC_FIND;
  				flag_compute = 1;
  			}
  			else buffer_pos++;
  		}
  		else	buffer_pos++;

  		if(buffer_pos < BUS_PACKET_BUS_SIZE)
  			HAL_UART_Receive_IT(huart, &buffer_in[buffer_pos], 1);
  		else Error_Handler();
  	}
  }
  */
}








#ifdef STM32_MCU
/**
 * Configuration of CRC in a STM32 microcontroller
 * @return HAL status
 */
HAL_StatusTypeDef bus_packet_CRC16CCSDSConfig()
{
	if(HAL_CRC_DeInit(&hcrc) != HAL_OK)
			return HAL_ERROR;
	hcrc.Instance = CRC;
	hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
	hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_DISABLE;
	hcrc.Init.InitValue = 0;
	hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
	hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
	hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
	if (HAL_CRC_Init(&hcrc) != HAL_OK)
		return HAL_ERROR;
	if(HAL_CRCEx_Polynomial_Set(&hcrc, 0x1021, CRC_POLYLENGTH_16B) != HAL_OK)
		return HAL_ERROR;
	else return HAL_OK;
}


#else
uint16_t bus_packet_CRC16CCSDSCalculate(int16_t seed, uint8_t *buf, uint32_t len)
{
    unsigned short crc;
    unsigned short ch, xor_flag;
    int i, count;
    crc = seed;
    count = 0;
    while (len--) {
        ch = buf[count++];
        ch<<=8;
        for(i=0; i<8; i++)
        {
            if ((crc ^ ch) & 0x8000)    xor_flag = 1;
            else                        xor_flag = 0;

            crc = crc << 1;
            if (xor_flag)               crc = crc ^ 0x1021;

            ch = ch << 1;
        }
    }
    return (unsigned short)crc;
}

#endif
