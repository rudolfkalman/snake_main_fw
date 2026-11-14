#include "pid.h"
#include <math.h>

float clamp(float value, float min, float max){
  if(value > max){
    value = max;
  }else if(value < min){
    value = min;
  }
  return value;
}

float update_pid(PID_t *pid_arg, float current, float target, float dt){
  float error = target - current;
  float d_error = error - pid_arg->prev_error;
  pid_arg->sum += error * dt;
  pid_arg->sum = clamp(pid_arg->sum, pid_arg->sum_min, pid_arg->sum_max);

  if(fabs(error) < 1.0){
    pid_arg->sum = 0;
  }
  float output = pid_arg->kp * error + pid_arg->ki * pid_arg->sum + pid_arg->kd * d_error / dt;
  pid_arg->prev_error = error;

  output = clamp(output, pid_arg->output_min, pid_arg->output_max); 
  return output;
}
