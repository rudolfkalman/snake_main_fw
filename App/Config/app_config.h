#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define BOARD_TYPE_PWM   1
#define BOARD_TYPE_LIMIT 2
#define BOARD_TYPE_ABS   3

#define DUTY_ON  8499
#define DUTY_OFF 0


#ifndef BOARD_TYPE
#define BOARD_TYPE 0
#endif

#ifndef CAN_DEVICE_ID
#define CAN_DEVICE_ID 1
#endif

#endif
