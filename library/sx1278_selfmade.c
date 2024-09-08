#include "sx1278_selfmade.h"


//set default value 
LoRa default_lora()
{
	LoRa df_lora;
	
	df_lora.frequency             = 433;
	df_lora.spredingFactor        = SF_7;
	df_lora.bandWidth             = BW_125KHz;
	df_lora.crcRate               = CR_4_5;
	df_lora.power                 = POWER_20db;
	df_lora.overCurrentProtection = 100;  // bao ve qua dong 100mA
	df_lora.preamble              = 8;
	
	return df_lora;
	
}

//reset 
void LoRa_reset1(LoRa* _lora)
{
	HAL_GPIO_WritePin(_lora->RS_port, _lora->RS_pin, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(_lora->RS_port, _lora->RS_pin, GPIO_PIN_SET);
	HAL_Delay(100);
	
}

//read a resgister by an address with length byte send one time and length byte want reciept 
void lora_readReg(LoRa* _lora, uint8_t* address, uint16_t g_length, uint8_t* doc, uint16_t r_length)
{
	HAL_GPIO_WritePin(_lora->CS_port, _lora->CS_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(_lora->hSPIx, address, g_length, TRANSMIT_TIMEOUT);
	while(HAL_SPI_GetState(_lora->hSPIx) != HAL_SPI_STATE_READY);					//check bit TX
	HAL_SPI_Receive(_lora->hSPIx, doc, r_length, RECEIVE_TIMEOUT);
	
	while(HAL_SPI_GetState(_lora->hSPIx) != HAL_SPI_STATE_READY);					//check bit RX
	HAL_GPIO_WritePin(_lora->CS_port, _lora->CS_pin, GPIO_PIN_SET);
	
}

//read a register by an address and return read
uint8_t lora_read(LoRa* _lora, uint8_t address)
{
	uint8_t read_data;
	uint8_t data_addr;
	
	//set up bit 7 = 0 for read
	data_addr = address & 0x7F;
	
	lora_readReg(_lora, &data_addr, 1, &read_data, 1); 
	
	return read_data;
	
}

//write register with length send and receipt
void lora_writeReg(LoRa* _lora, uint8_t* address, uint16_t g_length, uint8_t* ghi, uint16_t r_length)
{
	HAL_GPIO_WritePin(_lora->CS_port, _lora->CS_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(_lora->hSPIx, address, g_length, TRANSMIT_TIMEOUT);
	while(HAL_SPI_GetState(_lora->hSPIx) != HAL_SPI_STATE_READY);
	HAL_SPI_Transmit(_lora->hSPIx, ghi, r_length, TRANSMIT_MODE);
	
	while(HAL_SPI_GetState(_lora->hSPIx) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(_lora->CS_port, _lora->CS_pin, GPIO_PIN_SET);
	
}	

//set bit 7 = 1 for write 
void lora_write(LoRa* _lora, uint8_t address, uint8_t value)
{
	uint8_t write_data;
	uint8_t data_addr;
	
	//set bit 7
	data_addr = address | 0x80;
	write_data = value;
	
	lora_writeReg(_lora, &data_addr, 1, &write_data, 1);
	
}

//select mode
void LoRa_mode(LoRa* _lora, int mode)
{
	uint8_t read;
	uint8_t data;
	
	read = lora_read(_lora, RegOpMode);
	data = read;
	
	//use low 3 bits to select mode 
	if(mode == SLEEP_MODE)
	{
		data = (read & 0xF8) | 0x00;
		_lora->current_mode = SLEEP_MODE;
	} else if (mode == STNBY_MODE)
	{
		data = (read & 0xF8) | 0x01;
		_lora->current_mode = STNBY_MODE;
	} else if (mode == TRANSMIT_MODE)
	{
		data = (read & 0xF8) | 0x03;
		_lora->current_mode = TRANSMIT_MODE;
	} else if (mode == RXCONTIN_MODE)
	{
		data = (read & 0xF8) | 0x05;
		_lora->current_mode = RXCONTIN_MODE;
	} else if (mode == RXSINGLE_MODE)
	{
		data = (read & 0xF8) | 0x06;
		_lora->current_mode = RXSINGLE_MODE;
	}
	
	lora_write(_lora, RegOpMode, data);
	
}

// set the LowDataRateOptimization.Is is mandated for when the symbol length exceeds 16ms.
void lora_lowDataRateOpt(LoRa* _lora, uint8_t value)
{
	uint8_t data;
	uint8_t read;
	
	read = lora_read(_lora, RegModemConfig3);
	
	//if value true thi set bit 3 to turn on mode 
	if(value)
		data = read | 0x08;
	else 
		data = read & 0xF7;
	
	lora_write(_lora, RegModemConfig3, data);
	HAL_Delay(10);
	
}

// the LowDataRateOptimization automation.
void lora_setAutoLDO(LoRa* _lora)
{
	double BW[] = {7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0, 500.0};
	
	//check wheter the data rate exceeds 16ms or not. 
	lora_lowDataRateOpt(_lora, (long)((1 << _lora->spredingFactor) / ((double)BW[_lora->bandWidth])) > 16.0);

}

// set frequency
void lora_setFreq(LoRa* _lora, int freq)
{
	uint8_t data;
	uint32_t F;
	
	F = (freq * 524288) >> 5;
	
	//write Msb.
	data = F >> 16;
	lora_write(_lora, RegFrMsb, data);
	HAL_Delay(5);
	
	//write Mid.
	data = F >> 8;
	lora_write(_lora, RegFrMid, data);
	HAL_Delay(5);
	
	//write Lsb.
	data = F >> 0; 
	lora_write(_lora, RegFrLsb, data);
	HAL_Delay(5);
	
}

// set spreading factor.(7-12)
void lora_setSpreadingFactor(LoRa* _lora, int sf)
{
	uint8_t data;
	uint8_t read;
	
	if(sf > 12)
		sf = 12;
	if(sf < 7)
		sf = 7;
	read = lora_read(_lora, RegModemConfig2);
	HAL_Delay(10);
	
	data = (sf << 4) + (read & 0x0F);
	lora_write(_lora, RegModemConfig2, data);
	HAL_Delay(10);
	
	lora_setAutoLDO(_lora);
	
}

//set power gain. 
void lora_setPower(LoRa* _lora, uint8_t power)
{
	lora_write(_lora, RegPaConfig, power);
	HAL_Delay(10);
	
}

//adjust overcurrent protection for transmitter circuit.

void lora_setOCP(LoRa* _lora, uint8_t current)
{
	uint8_t ocpTrim = 0;
	
	if(current < 45)
		current = 45;
	if(current > 240)
		current = 240;
	if(current <= 120)
		ocpTrim = (current - 45) / 5;
	else if(current <= 240)
		ocpTrim = (current + 30) / 10;
	
	ocpTrim = ocpTrim + (1 << 5);
	lora_write(_lora, RegOcp, ocpTrim);
	HAL_Delay(10);
	
}

// set timout msb to 0xFF and CRC enable.
void lora_setTO_CRCon(LoRa* _lora)
{
	uint8_t read;
	uint8_t data;
	
	read = lora_read(_lora, RegModemConfig2);
	
	data = read | 0x07;
	lora_write(_lora, RegModemConfig2, data);
	HAL_Delay(10);
	
}

//set syncWord
void lora_setSW(LoRa* _lora, uint8_t syncword)
{
	lora_write(_lora, RegSyncWord, syncword);
	HAL_Delay(10);
	
}

//write data Fifo.
void lora_writeFifo(LoRa* _lora, uint8_t address, uint8_t* value, uint8_t length)
{
	uint8_t addr;
	addr = address | 0x80;
	
	HAL_GPIO_WritePin(_lora->CS_port, _lora->CS_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(_lora->hSPIx, &addr, 1, TRANSMIT_TIMEOUT);
	while(HAL_SPI_GetState(_lora->hSPIx) != HAL_SPI_STATE_READY);
	
	//write fifo
	HAL_SPI_Transmit(_lora->hSPIx, value, length, TRANSMIT_TIMEOUT);
	while(HAL_SPI_GetState(_lora->hSPIx) != HAL_SPI_STATE_READY);
	
	HAL_GPIO_WritePin(_lora->CS_port, _lora->CS_pin, GPIO_PIN_SET);
	
}

//check the lora instruct values.
uint8_t lora_available(LoRa* _lora)
{
	return 1;
}

//transmit data
uint8_t lora_transmit(LoRa* _lora, uint8_t* data, uint8_t length, uint16_t timeout)
{
	volatile uint8_t read;
	
	int mode = _lora->current_mode;
	LoRa_mode(_lora, STNBY_MODE);
	read = lora_read(_lora, RegFiFoTxBaseAddr);
	lora_write(_lora, RegFiFoTxBaseAddr, read);
	lora_write(_lora, RegPayloadLength, length);
	lora_writeFifo(_lora, RegFiFo, data, length);
	LoRa_mode(_lora, TRANSMIT_MODE);
	
	while(1)
	{
		read = lora_read(_lora, RegIrqFlags);
		
		if((read & 0x08) != 0)
		{
			lora_write(_lora, RegIrqFlags, 0xFF);
			LoRa_mode(_lora, mode);
			
			return 1;
		}
		else
		{
			if(--timeout == 0)
			{
				LoRa_mode(_lora, mode);
				return 0;
			}
		}
		HAL_Delay(1);
	}
	
}

//start receiving continuously
void lora_starReceive(LoRa* _lora)
{
	LoRa_mode(_lora, RXCONTIN_MODE);
	
}

//receive data
uint8_t lora_receive(LoRa* _lora, uint8_t* data, uint8_t length)
{
	volatile uint8_t read;
	volatile uint8_t number_of_bytes;
	uint8_t min = 0;
	
	for(int i = 0; i < length; i++) 
		data[i] = 0;
	
	LoRa_mode(_lora, STNBY_MODE);
	read = lora_read(_lora, RegIrqFlags);
	if((read & 0x50) != 0)
	{
		lora_write(_lora, RegIrqFlags, 0xFF);
		number_of_bytes = lora_read(_lora, RegRxNbBytes);
		read = lora_read(_lora, RegFiFoRxCurrentAddr);
		lora_write(_lora, RegFiFoAddPtr, read);
		min = length >= number_of_bytes ? number_of_bytes : length;
		
		for(int i = 0; i < min; i++)
			data[i] = lora_read(_lora, RegFiFo);
	}
	LoRa_mode(_lora, RXCONTIN_MODE);
	
	return min;
	
}

//Measure signal strength
int lora_getRSSI(LoRa* _lora)
{
	uint8_t read;
	read = lora_read(_lora, RegPktRssiValue);
	
	return -164 + read;
	
}

//init lora
uint16_t lora_init(LoRa* _lora)
{
	uint8_t data;
	uint8_t read;
	
	if(lora_available(_lora))
	{
		LoRa_mode(_lora, SLEEP_MODE);
		HAL_Delay(10);
		
		//turn on lora mode
		read = lora_read(_lora, RegOpMode);
		HAL_Delay(10);
		data = read | 0x80;
		lora_write(_lora, RegOpMode, data);
		HAL_Delay(100);
		
		//set freq
		lora_setFreq(_lora, _lora->frequency);
		
		//set power gain
		lora_setPower(_lora, _lora->power);
		
		//set overcurrent protection.
		lora_setOCP(_lora, _lora->overCurrentProtection);
		
		//set LNA gain.
		lora_write(_lora, RegLna, 0x23); //bit 0:1 11 150%, 001 max gain
		
		//set spreading factor, CRC on, and timout MSB LCB
		lora_setTO_CRCon(_lora);
		lora_setSpreadingFactor(_lora, _lora->spredingFactor);
		lora_write(_lora, RegSymbTimeoutL, 0xFF);
		
		//set bandwith, coding rate and expilicit.
		// 8 bit RegModemConfig --> | X | X | X | X | X | X | X | X |
		//       bits represent --> |   bandwidth   |     CR    |I/E|
		data = 0;
		data = (_lora->bandWidth << 4) + (_lora->crcRate << 1);
		lora_write(_lora, RegModemConfig1, data);
		lora_setAutoLDO(_lora);
		
		//set preamble.
		lora_write(_lora, RegPreambleMsb, _lora->preamble >> 8);
		lora_write(_lora, RegPreambleLsb, _lora->preamble >> 0);
		
		//DIO mapping: 
		read = lora_read(_lora, RegDioMapping1);
		data = read | 0x3F;
		lora_write(_lora, RegDioMapping1, data);
		
		LoRa_mode(_lora, STNBY_MODE);
		_lora->current_mode = STNBY_MODE;
		HAL_Delay(10);
		
		read = lora_read(_lora, RegVersion);
		if(read == 0x12)
			return LORA_OK;
		else
			return LORA_NOT_FOUND;
		
	}
	else 
	{
		return LORA_UNAVAILABLE;
	}
}
