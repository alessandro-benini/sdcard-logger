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
#include <stdlib.h>
#include "ff_gen_drv.h"

#include "spi.h"

#define _USE_WRITE 1

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define CS_HIGH() HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_SET);
#define CS_LOW()  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_RESET);

#define DEFAULT_TIMEOUT 10

/* Definitions for MMC/SDC command */
#define CMD0	  0   /* GO_IDLE_STATE */
#define CMD1	  1   /* SEND_OP_COND (MMC) */
#define ACMD41  41  /* SEND_OP_COND (SDC) */
#define CMD8	  8   /* SEND_IF_COND */
#define CMD9	  9   /* SEND_CSD */
#define CMD10	  10  /* SEND_CID */
#define CMD12	  12  /* STOP_TRANSMISSION */
#define ACMD13  13  /* SD_STATUS (SDC) */
#define CMD16	  16  /* SET_BLOCKLEN */
#define CMD17	  17  /* READ_SINGLE_BLOCK */
#define CMD18	  18  /* READ_MULTIPLE_BLOCK */
#define CMD23	  23  /* SET_BLOCK_COUNT (MMC) */
#define ACMD23  23  /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	  24  /* WRITE_BLOCK */
#define CMD25	  25  /* WRITE_MULTIPLE_BLOCK */
#define CMD55	  55  /* APP_CMD */
#define CMD58   58  /* READ_OCR */

#define DATA_TOKEN_CMD24    0xFE
#define DATA_TOKEN_CMD25    0xFC
#define STOP_TOKEN_CMD25    0xFD

#define SECTOR_SIZE 512
#define DATA_PACKET_LEN SECTOR_SIZE+3

uint8_t dummy_crc[] = {0xAB,0xAB};

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
uint8_t send_cmd(BYTE cmd, DWORD arg, uint8_t* res, short res_len);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (BYTE pdrv)
{
  /* USER CODE BEGIN INIT */
  uint8_t dummy_clocks[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  
  Stat = STA_NOINIT;

  enum initialization_state 
  {
    SD_POWER_CYCLE = 0,
    SD_SEND_CMD0,
    SD_SEND_CMD8,
    SD_SEND_CMD55,
    SD_SEND_ACMD41,
    SD_SEND_CMD1,
    SD_SEND_CMD58,
    SD_SEND_CMD16,
    SD_SUCCESS,
    SD_ERROR,		
  } init_phase;

  uint8_t r1 = 0xFF;
  uint8_t r3_7[5] = {0};
  
  DWORD arg = 0;
  init_phase = SD_POWER_CYCLE;

  // Set SPI clock rate to 250 Kbit/s for the initialization phase
  set_spi_clock_slow();
  
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
        send_cmd(CMD0,arg,(uint8_t*)&r1,sizeof(r1));
        if(r1 == 0x01)
          init_phase = SD_SEND_CMD8;
        else
          init_phase = SD_ERROR;
        break;
      case SD_SEND_CMD8:
        arg = 0x000001AA;
        send_cmd(CMD8,arg,r3_7,sizeof(r3_7)); 
        if(r3_7[3] == 0x01 && r3_7[4] == 0xAA)
          init_phase = SD_SEND_CMD55;
        else
          init_phase = SD_ERROR;
        break;
      case SD_SEND_CMD55:	
        arg = 0x00000000;
        send_cmd(CMD55,arg,(uint8_t*)&r1,sizeof(r1));
        if(r1 == 0x01)
          init_phase = SD_SEND_ACMD41;
        else
          init_phase = SD_ERROR;
        break;
      case SD_SEND_ACMD41:
        arg = 0x40000000;
        send_cmd(ACMD41,arg,(uint8_t*)&r1,sizeof(r1));
        if(r1 == 0x00)
          init_phase = SD_SEND_CMD58;
        else if(r1 == 0x01)
          init_phase = SD_SEND_CMD55;
        else
          init_phase = SD_ERROR;
        break;
      case SD_SEND_CMD58:
        arg = 0x00000000;
        send_cmd(CMD58,arg,r3_7,sizeof(r3_7));
        if(r3_7[1] & (1<<6))
        {
          // SD Version 2+ (Block address)
          Stat = ~STA_NOINIT;
          init_phase = SD_SUCCESS;
        }
        else
          // SD Version 2+ (byte address)
          init_phase = SD_SEND_CMD16;
        break;
      case SD_SEND_CMD16:
        // Force block size to 512 bytes to work with FAT file system
        arg = 0x00000200;
        send_cmd(CMD16,arg,(uint8_t*)&r1,sizeof(r1));
        Stat = ~STA_NOINIT;
        init_phase = SD_SUCCESS;
        break;
      case SD_ERROR:
        Stat = STA_NODISK;
        break;
      default:
        // Something went wrong - Try to re-init
        init_phase = SD_POWER_CYCLE;
        spi_init();
      break;				
    } 
  }

  // De-assert chip select line
  CS_HIGH();
  
  // Set SPI clock back to 16Mbit/s for standard use
  // set_spi_clock_fast();
  
  return Stat;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status 
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (BYTE pdrv)
{
  /* USER CODE BEGIN STATUS */
  
  // FOr the moment I assume to have only 1 physical drive (0)
  if(pdrv)
    return STA_NODISK;
  else
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
DRESULT USER_read (BYTE pdrv,BYTE *buff,DWORD sector,UINT count)
{
  /* USER CODE BEGIN READ */
  
  CS_LOW();
  
  // Response byte for CMD18 (r1)
  uint8_t r1 = 0xFF;
  
  // data packet during the reading process. I assume sector size of 512 bytes.
  uint8_t* data_packet = (uint8_t*)malloc(sizeof(uint8_t)*SECTOR_SIZE);
  
  // Send command
  send_cmd(CMD18,sector,(uint8_t*)&r1,sizeof(r1));
  
  if(r1 != 0x01)
  {
    CS_HIGH();
    return RES_ERROR;
  }
  else
  {
    // I assume that a sector is 512 bytes
    for(uint8_t i = 0; i < count; i++)
    {
      // DATA_PACKET_LEN is 515 bytes: 1 byte for the Token, 512 bytes of data, 2 bytes for the CRC
      HAL_SPI_Receive(&hspi2, (uint8_t*)&data_packet, DATA_PACKET_LEN, DEFAULT_TIMEOUT);
      memcpy(buff+(SECTOR_SIZE*i),data_packet+1,SECTOR_SIZE);
    }
    // After reading the sectors, I need to send the CMD12 to stop the flow
    send_cmd(CMD18,sector,(uint8_t*)&r1,sizeof(r1));
  }
  
  CS_HIGH();
  
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
DRESULT USER_write (BYTE pdrv,const BYTE *buff,DWORD sector,UINT count)
{ 
  /* USER CODE BEGIN WRITE */
  
  CS_LOW();
  
  // Response byte for CMD25 (r1)
  uint8_t r1 = 0xFF;
  uint8_t data_response = 0x00;
  
  uint8_t busy = 0x00;
  uint8_t stop_write = STOP_TOKEN_CMD25;
  
  uint8_t data_packet[DATA_PACKET_LEN];
  
  uint8_t cmd12_receive[15];
  
  data_packet[0] = DATA_TOKEN_CMD25;
  data_packet[DATA_PACKET_LEN-2] = 0xAB;
  data_packet[DATA_PACKET_LEN-1] = 0xAB;
  
  // Send command
  send_cmd(CMD25,sector,(uint8_t*)&r1,sizeof(r1));
  
  if(r1 != 0x00)
  {
    CS_HIGH();
    return RES_ERROR;
  }
  else
  {
    for(uint8_t i = 0; i < count; i++)
    {
      memcpy(data_packet+1,buff+((count-1)*SECTOR_SIZE),SECTOR_SIZE);
      HAL_SPI_Transmit(&hspi2,(uint8_t*)&data_packet, DATA_PACKET_LEN, DEFAULT_TIMEOUT);
      HAL_SPI_Receive(&hspi2, (uint8_t*)&data_response, 1, DEFAULT_TIMEOUT);
      
      // Wait until the sdcard is busy
      // Todo: Evaluate the use of a timer
      while(busy == 0x00)
      {
        HAL_SPI_Receive(&hspi2, (uint8_t*)&busy, 1, DEFAULT_TIMEOUT);
      }
      
    }
    // After writing the sectors, I need to send the stop token
    HAL_SPI_Transmit(&hspi2,(uint8_t*)&stop_write, 1, DEFAULT_TIMEOUT);
    HAL_SPI_Receive(&hspi2, (uint8_t*)&cmd12_receive, sizeof(cmd12_receive), DEFAULT_TIMEOUT);
  }
  
  CS_HIGH();
  
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
DRESULT USER_ioctl (BYTE pdrv,BYTE cmd,void *buff)
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

/**
  * @brief  Send a command to the SD cards using SPI protocol
  * @param  cmd: Command to be sent
  * @param  arg: Argument (4 bytes)
  * @param  res: Pointer to the array that will contain the response
  * @param  len: lenght of the array that will contain the response
  * @retval None
  */
uint8_t send_cmd(BYTE cmd, DWORD arg, uint8_t* res, short res_len)
{
	// cmd packet is of fixed lenght
	uint8_t cmd_packet[6] = {0};
	uint8_t MOSI_high = 0xFF;
  
  uint8_t CMD12_answer = 0x00;
	
	// First byte is the command
	// The cmd_packet must start with 01, therefore we add 0x40 to the cmd byte
  cmd_packet[0] = 0x40 | cmd;
	
	// Four bytes for the argument
	for(uint8_t i = 1; i<=4; i++)
		cmd_packet[i] = (uint8_t)(arg >> (4-i)*8);
	
	// Add crc: it must be correct for CMD0 and CMD 8 only; for other commands, a dummy crc (0x01) is set
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
  
  // Answer from the SD card
  if(cmd != CMD12)
  {
    HAL_SPI_Transmit(&hspi2, (uint8_t*)&MOSI_high, sizeof(MOSI_high), DEFAULT_TIMEOUT);
    HAL_SPI_Receive(&hspi2, res, res_len, DEFAULT_TIMEOUT);
    return 0;
  }
  // CMD12 answer needs special handling
  else
  {
    HAL_SPI_Transmit(&hspi2, (uint8_t*)&MOSI_high, sizeof(MOSI_high), DEFAULT_TIMEOUT);
    // First byte of answer after issuing command 12 must be discarded
    HAL_SPI_Receive(&hspi2, (uint8_t*)&CMD12_answer, sizeof(CMD12_answer), DEFAULT_TIMEOUT);
    // After this first byte, the line DO (MISO) goes low and remains low until the sd card is busy
    while(CMD12_answer != 0xFF)
    {
      HAL_SPI_Receive(&hspi2, (uint8_t*)&CMD12_answer, sizeof(CMD12_answer), DEFAULT_TIMEOUT);
    }
    // Return value will consider also a timeout
    return 0;
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
