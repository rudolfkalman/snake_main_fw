#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static int setup_serial_raw(int fd)
{
    struct termios tio;

    if (tcgetattr(fd, &tio) != 0) {
        perror("tcgetattr");
        return -1;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~CRTSCTS;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        perror("tcsetattr");
        return -1;
    }

    tcflush(fd, TCIOFLUSH);
    return 0;
}

static int write_all(int fd, const uint8_t *buf, size_t len)
{
    size_t written = 0;

    while (written < len) {
        ssize_t ret = write(fd, buf + written, len - written);
        if (ret <= 0) {
            return -1;
        }
        written += (size_t)ret;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s /dev/ttyACM0\n", argv[0]);
        return -1;
    }

    int fd = open(argv[1], O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    if (setup_serial_raw(fd) != 0) {
        close(fd);
        return -1;
    }

    char line[128];

    while (1) {
        printf("Enter: can_id(hex) value(int32) (q to quit)\n> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        if (line[0] == 'q' || line[0] == 'Q') break;

        uint32_t can_id;
        int32_t value;

        if (sscanf(line, "%x %d", &can_id, &value) != 2) {
            printf("format error: expected can_id(hex) value(int32)\n");
            continue;
        }

        uint8_t packet[8];
        packet[0] = (uint8_t)(can_id >> 24);
        packet[1] = (uint8_t)(can_id >> 16);
        packet[2] = (uint8_t)(can_id >> 8);
        packet[3] = (uint8_t)can_id;
        packet[4] = (uint8_t)((uint32_t)value >> 24);
        packet[5] = (uint8_t)((uint32_t)value >> 16);
        packet[6] = (uint8_t)((uint32_t)value >> 8);
        packet[7] = (uint8_t)value;

        if (write_all(fd, packet, sizeof(packet)) != 0) {
            perror("write");
        } else {
            printf("sent: can_id=0x%X value=%d\n", can_id, value);
        }
    }

    close(fd);
    return 0;
}
