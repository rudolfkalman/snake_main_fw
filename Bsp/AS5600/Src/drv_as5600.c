#include "drv_as5600.h"
#include <math.h>

bool init_as5600(I2C_HandleTypeDef *hi2cx){
  uint8_t current_as5600_conf;
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2cx, (AS5600_ADDR << 1), AS5600_CONF_ADDR, I2C_MEMADD_SIZE_16BIT, &current_as5600_conf, 2, 100);
  if(status == HAL_OK){
    current_as5600_conf |= (0x03 << 2);
    HAL_I2C_Mem_Write(hi2cx, (AS5600_ADDR << 1), AS5600_CONF_ADDR , I2C_MEMADD_SIZE_16BIT, &current_as5600_conf, 2, 100);
    return false;
  }else{
    return true;
  }
}

bool get_raw_angle(I2C_HandleTypeDef *hi2cx, float *angle){
  uint8_t buf[2];
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2cx, (AS5600_ADDR << 1), RAW_ANGLE_ADDR, I2C_MEMADD_SIZE_8BIT, buf, 2, 100);
  if(status == HAL_OK){
    uint16_t raw_angle = (uint16_t)((buf[0] << 8) | buf[1]);
    *angle = ((float)raw_angle * 2 * M_PI) / 4096.0f;
    //*angle = ((float)raw_angle * 360.0f) / 4096.0f;
    return false;
  }else{
    return true;
  }
}
