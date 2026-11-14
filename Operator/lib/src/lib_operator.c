#include "lib_operator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <math.h>

int op_init(const char *device_path) {
    int fd = open(device_path, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("op_init: open");
        return -1;
    }

    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) {
        perror("op_init: tcgetattr");
        close(fd);
        return -1;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~CRTSCTS;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        perror("op_init: tcsetattr");
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);
    return fd;
}

void op_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int op_send_raw(int fd, uint32_t can_id, int32_t value) {
    uint8_t packet[8];
    packet[0] = (uint8_t)(can_id >> 24);
    packet[1] = (uint8_t)(can_id >> 16);
    packet[2] = (uint8_t)(can_id >> 8);
    packet[3] = (uint8_t)can_id;
    packet[4] = (uint8_t)((uint32_t)value >> 24);
    packet[5] = (uint8_t)((uint32_t)value >> 16);
    packet[6] = (uint8_t)((uint32_t)value >> 8);
    packet[7] = (uint8_t)value;

    size_t written = 0;
    while (written < sizeof(packet)) {
        ssize_t ret = write(fd, packet + written, sizeof(packet) - written);
        if (ret <= 0) {
            perror("op_send_raw: write");
            return -1;
        }
        written += (size_t)ret;
    }
    return 0;
}

int op_send_duty(int fd, uint32_t can_id, float duty) {
    if (duty > 100.0f) duty = 100.0f;
    if (duty < -100.0f) duty = -100.0f;
    
    // PWM board expects int32, -100 to 100
    return op_send_raw(fd, can_id, (int32_t)duty);
}

int op_send_direction(int fd, uint32_t can_id, int8_t direction) {
    if (direction > 1) direction = 1;
    if (direction < -1) direction = -1;
    
    // Limit board expects int32, -1, 0, or 1
    return op_send_raw(fd, can_id, (int32_t)direction);
}

int op_send_target_angle(int fd, uint32_t can_id, float angle_rad) {
    // ABS board expects target_angle * 1000 in main.c:
    // target_angle = (float)(received_data) / 1000;
    int32_t value = (int32_t)(angle_rad * 1000.0f);
    return op_send_raw(fd, can_id, value);
}
