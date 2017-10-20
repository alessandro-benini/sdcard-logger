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

uint8_t dummy_clocks[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t CMD0[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
uint8_t CMD_ANSWER[] = {0xFF};

#define CMD1		0x41	/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)		/* SEND_OP_COND (SDC) */
#define CMD8		(8)					/* SEND_IF_COND */
#define CMD9		(9)					/* SEND_CSD */
#define CMD10		(10)				/* SEND_CID */
#define CMD12		(12)				/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)		/* SD_STATUS (SDC) */
#define CMD16		(16)				/* SET_BLOCKLEN */
#define CMD17		(17)				/* READ_SINGLE_BLOCK */
#define CMD18		(18)				/* READ_MULTIPLE_BLOCK */
#define CMD23		(23)				/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)		/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24		(24)				/* WRITE_BLOCK */
#define CMD25		(25)				/* WRITE_MULTIPLE_BLOCK */
#define CMD32		(32)				/* ERASE_ER_BLK_START */
#define CMD33		(33)				/* ERASE_ER_BLK_END */
#define CMD38		(38)				/* ERASE */
#define CMD55		(55)				/* APP_CMD */
#define CMD58		(58)				/* READ_OCR */

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

int spi_init(void);

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
		SD_SEND_ACMD41,
		SD_SEND_CMD1,
		SD_SEND_CMD58,
		SD_SEND_CMD16,
		SD_SUCCESS,
		SD_ERROR,		
	} init_phase;
	
	uint8_t response = 0x00;
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
				HAL_SPI_Transmit(&hspi2, CMD0, sizeof(CMD0), 10);
				init_phase = SD_WAIT_CMD0_ANSWER;
				break;
			case SD_WAIT_CMD0_ANSWER:
				HAL_SPI_Transmit(&hspi2, CMD_ANSWER, sizeof(CMD_ANSWER), 10);
				HAL_SPI_Receive(&hspi2,&response,sizeof(response),10);
				if(response == 0x01)
					init_phase = SD_SEND_ACMD41;
				break;
			case SD_SEND_ACMD41:
				break;
			case SD_SEND_CMD8:
				break;
			case SD_SEND_CMD58:
				break;
			case SD_SEND_CMD16:
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

int spi_init(void)
{
	CS_HIGH();
	HAL_Delay(10);
	return 0;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
