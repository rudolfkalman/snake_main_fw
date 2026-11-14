#ifndef LIB_OPERATOR_H
#define LIB_OPERATOR_H

#include <stdint.h>

/**
 * @brief Initialize the serial connection for robot control.
 * @param device_path Path to the serial device (e.g., "/dev/ttyACM0").
 * @return File descriptor on success, -1 on failure.
 */
int op_init(const char *device_path);

/**
 * @brief Close the serial connection.
 * @param fd File descriptor returned by op_init.
 */
void op_close(int fd);

/**
 * @brief Send raw CAN message via USB bridge.
 * @param fd Serial file descriptor.
 * @param can_id CAN ID of the target board.
 * @param value 32-bit integer payload.
 * @return 0 on success, -1 on failure.
 */
int op_send_raw(int fd, uint32_t can_id, int32_t value);

/**
 * @brief Send motor duty cycle (PWM control).
 * @param fd Serial file descriptor.
 * @param can_id CAN ID of the PWM board.
 * @param duty Duty cycle percentage (-100 to 100).
 * @return 0 on success, -1 on failure.
 */
int op_send_duty(int fd, uint32_t can_id, float duty);

/**
 * @brief Send movement direction (Limit board control).
 * @param fd Serial file descriptor.
 * @param can_id CAN ID of the limit board.
 * @param direction -1 (reverse), 0 (stop), 1 (forward).
 * @return 0 on success, -1 on failure.
 */
int op_send_direction(int fd, uint32_t can_id, int8_t direction);

/**
 * @brief Send target angle for position control (ABS board control).
 * @param fd Serial file descriptor.
 * @param can_id CAN ID of the ABS board.
 * @param angle_rad Target angle in radians (-PI to PI or 0 to 2PI).
 * @return 0 on success, -1 on failure.
 */
int op_send_target_angle(int fd, uint32_t can_id, float angle_rad);

#endif // LIB_OPERATOR_H
