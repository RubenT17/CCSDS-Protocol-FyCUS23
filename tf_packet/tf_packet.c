/**
  ******************************************************************************
  * @file           : tf_packet.c
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
  ******************************************************************************
  */

#include "tf_packet.h"



HAL_StatusTypeDef tf_packet_Decode(uint8_t *buffer_in, uint32_t buffer_length,  tfph_packet_t *tfph, tfdf_packet_t *tfdf)
{
	tfph->tfvn = (buffer_in[0] & 0b11110000)>>4;
	tfph->scid = ((buffer_in[0] & 0b00001111)<<12) | (buffer_in[1] << 4) | ((buffer_in[2] & 0b11110000)>>4);
	tfph->source_dest_id = (buffer_in[2] & 0b00001000)>>3;
	tfph->vcid = ((buffer_in[2] & 0b00000111)<<3) | ((buffer_in[3] & 0b11100000) >>5);
	tfph->mapid = (buffer_in[3] & 0b00011110) >>1;
	tfph->end_flag = buffer_in[3] & 0b00000001;

	if(!tfph->end_flag)
	{
		tfph->length = (buffer_in[4]<<8) | buffer_in[5];
		tfph->bypass_flag = (buffer_in[6] & 0b10000000) >>7;
		tfph->command_flag = (buffer_in[6] & 0b01000000) >>6;
//		tfph->spare = (buffer_in[6] & 0b00110000) >>4;
		tfph->ocf_flag = (buffer_in[6] & 0b00001000) >>3;
		tfph->vc_length = buffer_in[6] & 0b00000111;
		memcpy(tfph->vc_frame, &buffer_in[7], tfph->vc_length);

		tfdf->constr_rule = (buffer_in[7+tfph->vc_length] & 0b11100000) >>5;
		tfdf->protocol_id = buffer_in[7+tfph->vc_length] & 0b00011111;


		if(tfph->length - TF_PACKET_PRIMARY_BASE_HEADER_SIZE - tfph->vc_length - TF_PACKET_DATA_HEADER_SIZE - TF_PACKET_ECF_SIZE < 0)	return HAL_ERROR;
		memcpy(tfdf->data, &buffer_in[8+tfph->vc_length], tfph->length - TF_PACKET_PRIMARY_BASE_HEADER_SIZE - tfph->vc_length - TF_PACKET_DATA_HEADER_SIZE - TF_PACKET_ECF_SIZE);

#ifdef STM32_MCU
		uint16_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buffer_in, tfph->length - TF_PACKET_ECF_SIZE) & 0xFFFF;
#else
		uint16_t calculated_crc = bus_packet_CRC16CCSDSCalculate(0, buffer_in, tfph->length - TF_PACKET_ECF_SIZE) & 0xFFFF;
#endif

		uint16_t ecf = (buffer_in[tfph->length-TF_PACKET_ECF_SIZE]<<8) | buffer_in[tfph->length-TF_PACKET_ECF_SIZE+1];

		if(calculated_crc != ecf)	return HAL_ERROR;
	}

	else
	{
		tfdf->constr_rule = (buffer_in[4] & 0b11100000) >>5;
		tfdf->protocol_id = buffer_in[4] & 0b00011111;
		if(buffer_length - TF_PACKET_PRIMARY_TRUNCATED_HEADER_SIZE - TF_PACKET_DATA_HEADER_SIZE - TF_PACKET_ECF_SIZE < 0)	return HAL_ERROR;
		memcpy(tfdf->data, &buffer_in[5], buffer_length - TF_PACKET_PRIMARY_TRUNCATED_HEADER_SIZE - TF_PACKET_DATA_HEADER_SIZE - TF_PACKET_ECF_SIZE);

#ifdef STM32_MCU
		uint16_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buffer_in, buffer_length - TF_PACKET_ECF_SIZE) & 0xFFFF;
#else
		uint16_t calculated_crc = bus_packet_CRC16CCSDSCalculate(0, buffer_in, buffer_length - TF_PACKET_ECF_SIZE) & 0xFFFF;
#endif

		uint16_t ecf = buffer_in[buffer_length-TF_PACKET_ECF_SIZE]<<8 | buffer_in[buffer_length-TF_PACKET_ECF_SIZE+1];

		if(calculated_crc == ecf)	return HAL_ERROR;
	}


	return HAL_OK;
}



HAL_StatusTypeDef tf_packet_Packetize(uint8_t data_length, tfph_packet_t *tfph, tfdf_packet_t *tfdf, uint8_t *buffer_out)
{
	buffer_out[0] = (tfph->tfvn<<4) | ((tfph->scid & 0xF000)>>12);
	buffer_out[1] = (tfph->scid & 0x0FF0)>>4;
	buffer_out[2] = ((tfph->scid & 0x000F)<<4) | (tfph->source_dest_id<<3) | ((tfph->vcid & 0b111000)>>3);
	buffer_out[3] = ((tfph->vcid & 0b000111)<<5) | (tfph->mapid<<1) | (tfph->end_flag);


	if(!tfph->end_flag)
	{
		if(tfph->length > TF_PACKET_MAX_SIZE)	return HAL_ERROR;

		buffer_out[4] = (tfph->length & 0xFF00)>>8;
		buffer_out[5] = tfph->length & 0x00FF;
		buffer_out[6] = (tfph->bypass_flag<<7) | (tfph->command_flag<<6) /* | (spare)*/ | (tfph->ocf_flag<<3) | tfph->vc_length;
		memcpy(&buffer_out[7], tfph->vc_frame, tfph->vc_length);

		buffer_out[7+tfph->vc_length] = (tfdf->constr_rule<<5) | tfdf->protocol_id;

		memcpy(&buffer_out[8+tfph->vc_length], tfdf->data, tfph->length - TF_PACKET_PRIMARY_BASE_HEADER_SIZE - tfph->vc_length - TF_PACKET_DATA_HEADER_SIZE - TF_PACKET_ECF_SIZE);

#ifdef STM32_MCU
		uint16_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buffer_out, tfph->length - TF_PACKET_ECF_SIZE) & 0xFFFF;
#else
		uint16_t calculated_crc = bus_packet_CRC16CCSDSCalculate(0, buffer_out, tfph->length - TF_PACKET_ECF_SIZE) & 0xFFFF;
#endif

		buffer_out[tfph->length-TF_PACKET_ECF_SIZE] = (calculated_crc & 0xFF00)>>8;
		buffer_out[tfph->length-TF_PACKET_ECF_SIZE+1] = calculated_crc & 0x00FF;
	}


	else
	{
		if(data_length > TF_PACKET_DATA_MAX_SIZE)		return HAL_ERROR;

		buffer_out[7] = (tfdf->constr_rule<<5) | tfdf->protocol_id;
		memcpy(&buffer_out[8], tfdf->data, data_length);

#ifdef STM32_MCU
		uint16_t calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)buffer_out, data_length + TF_PACKET_PRIMARY_TRUNCATED_HEADER_SIZE + TF_PACKET_DATA_HEADER_SIZE) & 0xFFFF;
#else
		uint16_t calculated_crc = bus_packet_CRC16CCSDSCalculate(0, buffer_out, data_length + TF_PACKET_PRIMARY_TRUNCATED_HEADER_SIZE + TF_PACKET_DATA_HEADER_SIZE) & 0xFFFF;
#endif

		buffer_out[data_length + TF_PACKET_PRIMARY_TRUNCATED_HEADER_SIZE + TF_PACKET_DATA_HEADER_SIZE] = calculated_crc & 0x00FF;
		buffer_out[data_length + TF_PACKET_PRIMARY_TRUNCATED_HEADER_SIZE + TF_PACKET_DATA_HEADER_SIZE+1] = (calculated_crc & 0xFF00)>>8;
	}

	 return HAL_OK;
}



HAL_StatusTypeDef tf_packet_SetData(uint8_t *data, uint8_t data_length, uint8_t *VCdata, uint8_t VCdata_length, tfph_packet_t *tfph, tfdf_packet_t *tfdf)
{
	if(data_length > TF_PACKET_DATA_MAX_SIZE) 	return HAL_ERROR;
	if(VCdata_length > TF_PACKET_VCDATA_MAX_SIZE)	return HAL_ERROR;

	if(!tfph->end_flag)
	{
		tfph->vc_length = VCdata_length;
		tfph->length = data_length + VCdata_length + TF_PACKET_PRIMARY_BASE_HEADER_SIZE + TF_PACKET_DATA_HEADER_SIZE + TF_PACKET_ECF_SIZE;

		if(tfph->length > (TF_PACKET_DATA_MAX_SIZE - (TF_PACKET_PRIMARY_BASE_HEADER_SIZE - TF_PACKET_PRIMARY_TRUNCATED_HEADER_SIZE)) )
			return HAL_ERROR;

		memcpy(tfph->vc_frame, VCdata, VCdata_length);

	}

	memcpy(tfdf->data, data, data_length);

	return HAL_OK;
}




#ifdef STM32_MCU
HAL_StatusTypeDef tf_packet_CRC16CCSDSConfig()
{
	if(HAL_CRCEx_Polynomial_Set(&hcrc, 0x1021, CRC_POLYLENGTH_16B) != HAL_OK)					return HAL_ERROR;
	else if(HAL_CRCEx_Input_Data_Reverse(&hcrc, CRC_INPUTDATA_INVERSION_NONE) != HAL_OK)		return HAL_ERROR;
	else if(HAL_CRCEx_Output_Data_Reverse(&hcrc, CRC_OUTPUTDATA_INVERSION_DISABLE) != HAL_OK)	return HAL_ERROR;
	else																						return HAL_OK;
}


#else
uint16_t tf_packet_CRC16CCSDSCalculate(int16_t seed, uint8_t *buf, uint32_t len)
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
