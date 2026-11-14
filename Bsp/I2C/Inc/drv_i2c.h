#ifndef __DRV_I2C_H__
#define __DRV_I2C_H__

#include "stm32g4xx_hal.h"
#include "drv_stdio.h"
#include <stdint.h>

static inline void i2c_detect(I2C_HandleTypeDef *hi2c){
  printf("\r\nI2C Scanner starting...\r\n");
  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
  for(int i = 0; i < 128; i+= 16){
    printf("%02x:", i);
    for(int j = 0; j < 16; j++){
      //fflush(stdout);
      uint8_t address = i + j;

      if(address < 0x03 || address > 0x77){
        printf("  ");
        continue;
      }

      HAL_StatusTypeDef status;
      status = HAL_I2C_IsDeviceReady(hi2c, (address << 1), 3, 100);

      if(status == HAL_OK){
        printf("%02x ", address);
      }else{
        printf("-- ");
      } 
    }
    printf("\r\n");
  }
  printf("Scan finished.\r\n");
}

static inline void who_am_i(I2C_HandleTypeDef *hi2cx, uint8_t device_addr, uint8_t who_am_i_addr){
    uint8_t who_am_i = 0;
    if(HAL_I2C_Mem_Read(hi2cx, (device_addr << 1), who_am_i_addr, I2C_MEMADD_SIZE_8BIT, &who_am_i, 1, 100) == HAL_OK){
      printf("WHO_AM_I: 0x%02x\r\n", who_am_i);
    }
}

#endif // !__DRV_I2C_H__
