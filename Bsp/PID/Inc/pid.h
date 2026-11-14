#ifndef __PID_H__
#define __PID_H__

typedef struct {
  float kp;
  float ki;
  float kd;

  float sum_min;
  float sum_max;

  float output_min;
  float output_max;

  float prev_error;
  float sum;
} PID_t;

float clamp(float value, float min, float max);

float update_pid(PID_t *pid_arg, float current, float target, float dt);

#endif
