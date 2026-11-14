#ifndef __DRV_STDIO_H__
#define __DRV_STDIO_H__

#include "stm32g4xx.h"
#include "usbd_cdc_if.h"
#include <stdio.h>

int _write(int file, char *ptr, int len);

#endif // __DRV_STDIO_H__
