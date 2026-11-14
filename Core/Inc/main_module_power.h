#ifndef MAIN_MODULE_POWER_H
#define MAIN_MODULE_POWER_H

void MainModule_PowerOnMain(void) __attribute__((noreturn));
void MainModule_DPinTestMain(void) __attribute__((noreturn));
void MainModule_LED1BlinkMain(void) __attribute__((noreturn));
void MainModule_STSTestMain(void) __attribute__((noreturn));

void MainModule_CANRxMain(void) __attribute__((noreturn));
void MainModule_CANTxMain(void) __attribute__((noreturn));
void MainModule_MDTestMain(void) __attribute__((noreturn));
void MainModule_PWMServoTestMain(void) __attribute__((noreturn));
void MainModule_BatteryMonitorMain(void) __attribute__((noreturn));
void MainModule_WdtTimeoutTestMain(void) __attribute__((noreturn));

#endif
