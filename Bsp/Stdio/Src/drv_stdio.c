#include "drv_stdio.h"

int _write(int file, char *ptr, int len) {
  uint8_t result = CDC_Transmit_FS((uint8_t *)ptr, len);

  if (result == USBD_BUSY) {
    return 0;
  }

  return len;
}
