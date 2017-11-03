/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/* 
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future. 
 * Kept to ensure backward compatibility with previous CubeMx versions when 
 * migrating projects. 
 * User code previously added there should be copied in the new user sections before 
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define CS_HIGH() HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_SET);
#define CS_LOW()  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_RESET);

#define DEFAULT_TIMEOUT 10

uint8_t dummy_clocks[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* Definitions for MMC/SDC command */
#define CMD0	  (0)	  /* GO_IDLE_STATE */
#define CMD1	  (1)	  /* SEND_OP_COND (MMC) */
#define	ACMD41	(41)	    /* SEND_OP_COND (SDC) */
#define CMD8	  (8)	  /* SEND_IF_COND */
#define CMD9	  (9)	  /* SEND_CSD */
#define CMD10	  (10)	  /* SEND_CID */
#define CMD12	  (12)	  /* STOP_TRANSMISSION */
#define ACMD13	(13)	  /* SD_STATUS (SDC) */
#define CMD16	  (16)	  /* SET_BLOCKLEN */
#define CMD17	  (17)	  /* READ_SINGLE_BLOCK */
#define CMD18	  (18)	  /* READ_MULTIPLE_BLOCK */
#define CMD23	  (23)	  /* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(23)	  /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	  (24)	  /* WRITE_BLOCK */
#define CMD25	  (25)	  /* WRITE_MULTIPLE_BLOCK */
#define CMD55	  (55)	  /* APP_CMD */
#define CMD58   (58)	  /* READ_OCR */

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;
extern SPI_HandleTypeDef hspi2;

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
           
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);  
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read, 
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */  
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

void spi_init(void);
uint8_t send_cmd(BYTE cmd, DWORD arg);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */


DSTATUS USER_initialize (BYTE pdrv)
{
  /* USER CODE BEGIN INIT */
  Stat = STA_NOINIT;
	
	enum initialization_state 
	{
    SD_POWER_CYCLE = 0,
		SD_SEND_CMD0,
		SD_WAIT_CMD0_ANSWER,
		SD_SEND_CMD8,
		SD_SEND_CMD55,
		SD_SEND_ACMD41,
		SD_SEND_CMD1,
		SD_SEND_CMD58,
		SD_SEND_CMD16,
		SD_SUCCESS,
		SD_ERROR,		
	} init_phase;
	
	uint8_t response = 0x00;
	DWORD arg = 0;
	init_phase = SD_POWER_CYCLE;
	
	spi_init();
	
	while(init_phase < SD_SUCCESS)
	{
		switch(init_phase)
		{
			case SD_POWER_CYCLE:
				// Wait 1 ms
				HAL_Delay(1);
				HAL_SPI_Transmit(&hspi2, dummy_clocks, sizeof(dummy_clocks), 10);
				init_phase = SD_SEND_CMD0;
				break;
			case SD_SEND_CMD0:
				CS_LOW();
				response = send_cmd(CMD0,arg);
				if(response == 0x01)
					init_phase = SD_SEND_CMD8;
				else
					init_phase = SD_ERROR;
				break;
			case SD_SEND_CMD8:
				arg = 0x000001AA;
				response = send_cmd(CMD8,arg);
				if(response == 0x01)
					init_phase = SD_SEND_CMD55;
				else
					init_phase = SD_ERROR;
				break;
			case SD_SEND_CMD55:	
				arg = 0x00000000;
				response = send_cmd(CMD55,arg);
				if(response == 0x01)
				{
					init_phase = SD_SEND_ACMD41;
				}
				else
					init_phase = SD_ERROR;
				break;
			case SD_SEND_ACMD41:
        
        // HAL_Delay(2000);
				arg = 0x40000000;
				response = send_cmd(ACMD41,arg);
      
				if(response == 0x00)
					init_phase = SD_SEND_CMD58;
				else if(response == 0x01)
          init_phase = SD_SEND_CMD55;
        else
          init_phase = SD_ERROR;
        
				break;
        
			case SD_SEND_CMD58:
				arg = 0x00000000;
				response = send_cmd(CMD58,arg);
				break;
			case SD_ERROR:
				CS_HIGH();
				Stat = STA_NODISK;
				break;
			default:
					// Something went wrong - Try to re-init
					init_phase = SD_POWER_CYCLE;
					spi_init();
				break;				
		}
	}
	
  return Stat;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status 
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
  Stat = STA_NOINIT;
  return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s) 
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
  return RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)  
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{ 
  /* USER CODE BEGIN WRITE */
  /* USER CODE HERE */
   return RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation  
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
   DRESULT res = RES_ERROR;
   return res;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

void spi_init(void)
{
	CS_HIGH();
	HAL_Delay(10);
}

uint8_t send_cmd(BYTE cmd, DWORD arg)
{
	// cmd packet is of fixed lenght
	uint8_t cmd_packet[6] = {0};
	
	// Response
	uint8_t cmd_response = 0xFF;
	// R1 is 1 byte only and it is used for most commands
	uint8_t r1 = 0xFF;
	// Commands R3 and R7 are 5 bytes long, (R1 + trailing 32-bit data)
	uint8_t r3_7[5] = {0};
	
	// First byte is the command
	// The cmd_packet must start with 01, therefore we add 0x40 to the cmd byte
  cmd_packet[0] = 0x40 | cmd;
	
	// Four bytes for the argument
	for(uint8_t i = 1; i<=4; i++)
		cmd_packet[i] = (uint8_t)(arg >> (4-i)*8);
	
	// Add crc: it must be correct for CMD0 and CMD 8 only; for other commands, we use a dummy crc (0x01)
	if(cmd == CMD0)
		cmd_packet[5] = 0x95;
	else if(cmd == CMD8)
		cmd_packet[5] = 0x87;
  else if(cmd == CMD55)
    cmd_packet[5] = 0x65;
	else if(cmd == ACMD41)
		cmd_packet[5] = 0x77;
	else
		cmd_packet[5] = 0x01;
	
	// Send the command
	HAL_SPI_Transmit(&hspi2, cmd_packet, sizeof(cmd_packet), DEFAULT_TIMEOUT);
	
	// Receive the answer from SDcard
	
	switch(cmd)
	{
		
		case CMD0:
			// Try 3 times to get the answer
			for(uint8_t j = 0; j<3; j++)
			{
				HAL_SPI_Transmit(&hspi2, (uint8_t*)&cmd_response, sizeof(cmd_response), DEFAULT_TIMEOUT);
				HAL_SPI_Receive(&hspi2,&r1,sizeof(r1),DEFAULT_TIMEOUT);
				if(r1 != 0xFF)
					return r1;
			}
			break;
			
		case CMD8:
			HAL_SPI_Transmit(&hspi2, (uint8_t*)&cmd_response, sizeof(cmd_response), DEFAULT_TIMEOUT);
			HAL_SPI_Receive(&hspi2,r3_7,sizeof(r3_7),DEFAULT_TIMEOUT);
			if( r3_7[3] == 0x01 && r3_7[4] == 0xAA)
				return 0x01;
			break;
			
		case CMD55:
			HAL_SPI_Transmit(&hspi2, (uint8_t*)&cmd_response, sizeof(cmd_response), DEFAULT_TIMEOUT);
			HAL_SPI_Receive(&hspi2,&r1,sizeof(r1),DEFAULT_TIMEOUT);
			if(r1 != 0xFF)
				return r1;
			break;
			
		case ACMD41:		
				HAL_SPI_Transmit(&hspi2, (uint8_t*)&cmd_response, sizeof(cmd_response), DEFAULT_TIMEOUT);
				HAL_SPI_Receive(&hspi2,&r1,sizeof(r1),DEFAULT_TIMEOUT);
				return r1;
      break;
			
		case CMD58:
			HAL_SPI_Transmit(&hspi2, (uint8_t*)&cmd_response, sizeof(cmd_response), DEFAULT_TIMEOUT);
			HAL_SPI_Receive(&hspi2,r3_7,sizeof(r3_7),DEFAULT_TIMEOUT);
			if( r3_7[1] & (1<<7))
				return 0x01;
			else
				return 0x00;
			break;
			
	}
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
