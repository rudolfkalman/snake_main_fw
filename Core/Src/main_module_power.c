#include "main.h"
#include "main_module_power.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "stm32g4xx_ll_adc.h"
#include <stdbool.h>
#include <stdio.h>

/* snake_pcb: U5 WDI, U8 D and CLK respectively. */
#define WDI_PIN GPIO_PIN_13
#define LATCH_D_PIN GPIO_PIN_14
#define LATCH_CLK_PIN GPIO_PIN_15
#define LED1_PIN GPIO_PIN_11
#define LED2_PIN GPIO_PIN_12
#define HALF_PERIOD_MS 550U
#define MAX_HALF_PERIOD_MS 650U
#define WARMUP_FALLS 6U
#define STS_POSITION_10_DEG 114U  /* 4096 counts per 360 degrees */
#define STS_POSITION_30_DEG 341U

extern USBD_HandleTypeDef hUsbDeviceFS;

static void Clock_Init(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI48;
  osc.HSIState = RCC_HSI_ON;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.HSI48State = RCC_HSI48_ON;
  osc.PLL.PLLState = RCC_PLL_OFF;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

static void GPIO_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  /* TB6643 IN1/IN2 stay LOW; no legacy MD PWM or SPI2 initialization. */
  gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
  HAL_GPIO_WritePin(GPIOA, gpio.Pin, GPIO_PIN_RESET);
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = WDI_PIN | LATCH_D_PIN | LATCH_CLK_PIN | LED1_PIN | LED2_PIN;
  HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_RESET);
  HAL_GPIO_Init(GPIOB, &gpio);
}

static void Latch_Write(bool on)
{
  HAL_GPIO_WritePin(GPIOB, LATCH_CLK_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, LATCH_D_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_Delay(50U); /* Wide setup interval for bench probing. */
  HAL_GPIO_WritePin(GPIOB, LATCH_CLK_PIN, GPIO_PIN_SET);
  HAL_Delay(50U); /* One 50 ms rising-edge pulse is easy to capture. */
  HAL_GPIO_WritePin(GPIOB, LATCH_CLK_PIN | LATCH_D_PIN, GPIO_PIN_RESET);
}

static void Report(uint32_t falls, bool arm_sent, bool timing_fault)
{
  static uint8_t line[128];
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) return;
  USBD_CDC_HandleTypeDef *cdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  if (cdc == NULL || cdc->TxState != 0U) return;
  int n = snprintf((char *)line, sizeof(line),
      "MAIN_POWER WDI=%s falls=%lu latch=%s Q/VSW=unverified\r\n",
      timing_fault ? "STOP_TIMING_FAULT" : "1100ms",
      (unsigned long)falls,
      timing_fault ? "OFF_SENT" : arm_sent ? "ON_SENT" : "WARMUP");
  if (n > 0 && n < (int)sizeof(line))
    (void)CDC_Transmit_FS(line, (uint16_t)n);
}

void MainModule_PowerOnMain(void)
{
  HAL_Init();
  Clock_Init();
  GPIO_Init();
  Latch_Write(false);
  MX_USB_Device_Init();

  bool wdi_high = false;
  bool arm_sent = false;
  bool timing_fault = false;
  uint32_t falls = 0U;
  uint32_t last_edge = HAL_GetTick();
  uint32_t last_report = last_edge;

  for (;;)
  {
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = (uint32_t)(now - last_edge);
    if (!timing_fault && elapsed > MAX_HALF_PERIOD_MS)
    {
      /* Never catch up with short WDI pulses or automatically re-arm after a stall. */
      timing_fault = true;
      Latch_Write(false);
      HAL_GPIO_WritePin(GPIOB, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);
      /* Freeze WDI at its last level; the external watchdog also times out. */
    }
    if (!timing_fault && elapsed >= HALF_PERIOD_MS)
    {
      wdi_high = !wdi_high;
      HAL_GPIO_WritePin(GPIOB, WDI_PIN | LED1_PIN,
                       wdi_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      last_edge = now;
      if (!wdi_high)
      {
        ++falls;
        if (!arm_sent && falls >= WARMUP_FALLS)
        {
          Latch_Write(true);
          arm_sent = true;
          /* This LED means the request was issued, not that VSW was measured. */
          HAL_GPIO_WritePin(GPIOB, LED2_PIN, GPIO_PIN_SET);
        }
      }
    }
    if ((uint32_t)(now - last_report) >= 1000U)
    {
      Report(falls, arm_sent, timing_fault);
      last_report = now;
    }
    HAL_Delay(1U);
  }
}

void MainModule_DPinTestMain(void)
{
  HAL_Init();
  Clock_Init();
  GPIO_Init();
  /* Start both D and CLK low; clock each new D value below. */
  HAL_GPIO_WritePin(GPIOB, LATCH_CLK_PIN | LATCH_D_PIN | WDI_PIN, GPIO_PIN_RESET);
  MX_USB_Device_Init();

  bool d_high = false;
  bool wdi_high = false;
  bool timing_fault = false;
  uint32_t falls = 0U;
  bool clk_pending = false;
  bool clk_high = false;
  uint32_t now = HAL_GetTick();
  uint32_t next_d_toggle = now + 500U;
  uint32_t clk_rise_due = 0U;
  uint32_t clk_fall_due = 0U;
  uint32_t last_wdi_edge = now;
  uint32_t last_report = now;

  for (;;)
  {
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = (uint32_t)(now - last_wdi_edge);
    if (!timing_fault && elapsed > MAX_HALF_PERIOD_MS)
    {
      timing_fault = true;
      HAL_GPIO_WritePin(GPIOB, WDI_PIN, GPIO_PIN_RESET);
    }
    if (!timing_fault && elapsed >= HALF_PERIOD_MS)
    {
      wdi_high = !wdi_high;
      HAL_GPIO_WritePin(GPIOB, WDI_PIN,
                       wdi_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      last_wdi_edge = now;
      if (!wdi_high) ++falls;
    }
    if (!clk_pending && (int32_t)(now - next_d_toggle) >= 0)
    {
      d_high = !d_high;
      HAL_GPIO_WritePin(GPIOB, LATCH_D_PIN,
                       d_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      clk_rise_due = now + 10U; /* D setup time before the rising edge. */
      clk_pending = true;
      next_d_toggle += 500U;
    }
    if (clk_pending && !clk_high &&
        (int32_t)(now - clk_rise_due) >= 0)
    {
      HAL_GPIO_WritePin(GPIOB, LATCH_CLK_PIN, GPIO_PIN_SET);
      clk_high = true;
      clk_fall_due = now + 10U;
    }
    if (clk_high && (int32_t)(now - clk_fall_due) >= 0)
    {
      HAL_GPIO_WritePin(GPIOB, LATCH_CLK_PIN, GPIO_PIN_RESET);
      clk_high = false;
      clk_pending = false;
    }
    if ((uint32_t)(now - last_report) >= 1000U)
    {
      Report(falls, false, timing_fault);
      last_report = now;
    }
    HAL_Delay(1U);
  }
}

void MainModule_LED1BlinkMain(void)
{
  HAL_Init();
  Clock_Init(); /* HSI16 internal clock; no external crystal is used. */

  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = LED1_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_WritePin(GPIOB, LED1_PIN, GPIO_PIN_RESET);
  HAL_GPIO_Init(GPIOB, &gpio);

  for (;;)
  {
    HAL_GPIO_TogglePin(GPIOB, LED1_PIN);
    HAL_Delay(500U);
  }
}

/* Adapted from g431_md_fw, branch half_duplex:
 * Bsp/STS3215/Src/sts3215_bridge.c: spin_test_write().
 * Same broadcast transport; USART2/PB3 becomes USART3/PB10 here.
 * Position mode replaces that test's persistent velocity-mode setting. */
static UART_HandleTypeDef hsts;
volatile uint32_t sts_command_count = 0U;
volatile uint16_t sts_target = STS_POSITION_10_DEG;
volatile uint8_t sts_init_step = 0U;
volatile HAL_StatusTypeDef sts_tx_status = HAL_OK;

static void STS_UART_Init(void)
{
  RCC_PeriphCLKInitTypeDef clock = {0};
  clock.PeriphClockSelection = RCC_PERIPHCLK_USART3;
  clock.Usart3ClockSelection = RCC_USART3CLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK) Error_Handler();
  __HAL_RCC_USART3_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = GPIO_PIN_10;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOB, &gpio);

  hsts.Instance = USART3;
  hsts.Init.BaudRate = 1000000U;
  hsts.Init.WordLength = UART_WORDLENGTH_8B;
  hsts.Init.StopBits = UART_STOPBITS_1;
  hsts.Init.Parity = UART_PARITY_NONE;
  hsts.Init.Mode = UART_MODE_TX_RX;
  hsts.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hsts.Init.OverSampling = UART_OVERSAMPLING_16;
  hsts.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hsts.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hsts.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_HalfDuplex_Init(&hsts) != HAL_OK) Error_Handler();
  if (HAL_UARTEx_SetTxFifoThreshold(&hsts, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    Error_Handler();
  if (HAL_UARTEx_SetRxFifoThreshold(&hsts, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    Error_Handler();
  if (HAL_UARTEx_DisableFifoMode(&hsts) != HAL_OK) Error_Handler();
  if (HAL_HalfDuplex_EnableTransmitter(&hsts) != HAL_OK) Error_Handler();
}

static HAL_StatusTypeDef STS_WriteBroadcast(uint8_t address,
                                           const uint8_t *data, uint8_t data_len)
{
  /* Expanded from the old test's 9 bytes to fit a 7-byte position payload. */
  uint8_t packet[16];
  if (data_len > sizeof(packet) - 7U) return HAL_ERROR;
  uint8_t packet_len = (uint8_t)(data_len + 7U);
  uint8_t checksum = 0U;
  packet[0] = 0xFFU;
  packet[1] = 0xFFU;
  packet[2] = 0xFEU;
  packet[3] = (uint8_t)(data_len + 3U);
  packet[4] = 0x03U;
  packet[5] = address;
  for (uint8_t i = 0U; i < data_len; ++i) packet[6U + i] = data[i];
  for (uint8_t i = 2U; i < packet_len - 1U; ++i)
    checksum = (uint8_t)(checksum + packet[i]);
  packet[packet_len - 1U] = (uint8_t)~checksum;
  /* Broadcast WRITE has no reply, just as in the known-working spin test. */
  return HAL_UART_Transmit(&hsts, packet, packet_len, 10U);
}

static HAL_StatusTypeDef STS_WriteByte(uint8_t address, uint8_t value)
{
  return STS_WriteBroadcast(address, &value, 1U);
}

static HAL_StatusTypeDef STS_Position(uint16_t target)
{
  /* ACC=50, absolute goal, goal time=0, speed=2500 counts/s. */
  const uint8_t data[7] = {
    50U, (uint8_t)target, (uint8_t)(target >> 8), 0U, 0U,
    (uint8_t)2500U, (uint8_t)(2500U >> 8)
  };
  return STS_WriteBroadcast(41U, data, sizeof(data));
}

void MainModule_STSTestMain(void)
{
  HAL_Init();
  Clock_Init();
  GPIO_Init();
  Latch_Write(false);
  MX_USB_Device_Init();

  bool wdi_high = false, armed = false, uart_ready = false, fault = false;
  uint32_t falls = 0U;
  uint32_t last_edge = HAL_GetTick();
  uint32_t next_action = last_edge;
  uint32_t last_report = last_edge;
  for (;;)
  {
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - last_edge;
    if (!fault && elapsed > MAX_HALF_PERIOD_MS)
    {
      fault = true;
      Latch_Write(false);
      HAL_GPIO_WritePin(GPIOB, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);
    }
    if (!fault && elapsed >= HALF_PERIOD_MS)
    {
      wdi_high = !wdi_high;
      HAL_GPIO_WritePin(GPIOB, WDI_PIN | LED1_PIN,
                       wdi_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      last_edge = now;
      if (!wdi_high && ++falls >= WARMUP_FALLS && !armed)
      {
        Latch_Write(true);
        armed = true;
        HAL_GPIO_WritePin(GPIOB, LED2_PIN, GPIO_PIN_SET);
        next_action = HAL_GetTick() + 1000U;
      }
    }

    if (armed && !fault && (int32_t)(now - next_action) >= 0)
    {
      if (!uart_ready) { STS_UART_Init(); uart_ready = true; }
      /* Old test's 20 ms gaps, scheduled without blocking the watchdog. */
      uint32_t delay_ms = 20U;
      switch (sts_init_step)
      {
        case 0: sts_tx_status = STS_WriteByte(40U, 0U); break; /* torque OFF */
        case 1: sts_tx_status = STS_WriteByte(55U, 0U); break; /* unlock */
        case 2: sts_tx_status = STS_WriteByte(33U, 0U); break; /* POSITION mode */
        case 3: sts_tx_status = STS_WriteByte(55U, 1U); delay_ms = 100U; break;
        case 4:
          sts_target = STS_POSITION_10_DEG;
          sts_tx_status = STS_Position(sts_target);
          if (sts_tx_status == HAL_OK) ++sts_command_count;
          break;
        case 5:
          sts_tx_status = STS_WriteByte(40U, 1U); /* torque ON */
          delay_ms = 1000U;
          break;
        default:
          sts_target = sts_target == STS_POSITION_10_DEG ?
                       STS_POSITION_30_DEG : STS_POSITION_10_DEG;
          sts_tx_status = STS_Position(sts_target);
          if (sts_tx_status == HAL_OK) ++sts_command_count;
          delay_ms = 1000U;
          break;
      }
      if (sts_tx_status != HAL_OK)
      {
        fault = true;
        Latch_Write(false);
        HAL_GPIO_WritePin(GPIOB, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);
      }
      else if (sts_init_step < 6U) ++sts_init_step;
      next_action = HAL_GetTick() + delay_ms;
    }

    if ((uint32_t)(now - last_report) >= 1000U)
    {
      static uint8_t line[192];
      if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED)
      {
        USBD_CDC_HandleTypeDef *cdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
        if (cdc != NULL && cdc->TxState == 0U)
        {
          int n = snprintf((char *)line, sizeof(line),
              "STS_POS_V6 state=%s step=%u id=FE mode=0_SENT target_deg=%u target=%u seq=%lu tx=%u feedback=UNVERIFIED\r\n",
              fault ? "FAULT_OFF" : !armed ? "WARMUP" : sts_init_step < 6U ? "INIT" : "RUN",
              (unsigned)sts_init_step, sts_target == STS_POSITION_10_DEG ? 10U : 30U,
              (unsigned)sts_target, (unsigned long)sts_command_count, (unsigned)sts_tx_status);
          if (n > 0 && n < (int)sizeof(line)) (void)CDC_Transmit_FS(line, (uint16_t)n);
        }
      }
      last_report = now;
    }
    HAL_Delay(1U);
  }
}

/* Standalone CAN receive test, using internal clocks and USB CDC. */
static FDCAN_HandleTypeDef hfdcan_monitor;
static void CAN_Monitor_Init(void)
{
  FDCAN_FilterTypeDef filter = {0};

  hfdcan_monitor.Instance = FDCAN1;
  hfdcan_monitor.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan_monitor.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan_monitor.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan_monitor.Init.AutoRetransmission = ENABLE;
  hfdcan_monitor.Init.TransmitPause = DISABLE;
  hfdcan_monitor.Init.ProtocolException = DISABLE;

  /* The board's FDCAN clock is 16 MHz: 16 MHz / (2 * 16 TQ) = 500 kbit/s. */
  hfdcan_monitor.Init.NominalPrescaler = 2U;
  hfdcan_monitor.Init.NominalSyncJumpWidth = 1U;
  hfdcan_monitor.Init.NominalTimeSeg1 = 13U;
  hfdcan_monitor.Init.NominalTimeSeg2 = 2U;
  hfdcan_monitor.Init.DataPrescaler = 2U;
  hfdcan_monitor.Init.DataSyncJumpWidth = 1U;
  hfdcan_monitor.Init.DataTimeSeg1 = 13U;
  hfdcan_monitor.Init.DataTimeSeg2 = 2U;
  hfdcan_monitor.Init.StdFiltersNbr = 1U;
  hfdcan_monitor.Init.ExtFiltersNbr = 0U;
  hfdcan_monitor.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

  if (HAL_FDCAN_Init(&hfdcan_monitor) != HAL_OK)
  {
    Error_Handler();
  }

  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterIndex = 0U;
  filter.FilterType = FDCAN_FILTER_RANGE;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = 0U;
  filter.FilterID2 = 0x7FFU;
  if (HAL_FDCAN_ConfigFilter(&hfdcan_monitor, &filter) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan_monitor,
                                   FDCAN_REJECT,
                                   FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK ||
      HAL_FDCAN_Start(&hfdcan_monitor) != HAL_OK)
  {
    Error_Handler();
  }
}

static uint8_t CAN_DlcToBytes(uint32_t dlc)
{
  switch (dlc)
  {
    case FDCAN_DLC_BYTES_0: return 0U;
    case FDCAN_DLC_BYTES_1: return 1U;
    case FDCAN_DLC_BYTES_2: return 2U;
    case FDCAN_DLC_BYTES_3: return 3U;
    case FDCAN_DLC_BYTES_4: return 4U;
    case FDCAN_DLC_BYTES_5: return 5U;
    case FDCAN_DLC_BYTES_6: return 6U;
    case FDCAN_DLC_BYTES_7: return 7U;
    case FDCAN_DLC_BYTES_8: return 8U;
    default: return 0U;
  }
}

/* Queue copies outlive USB DMA/interrupt transfers. Only overwrite usb_line
 * after TxState goes idle; receive polling never waits for the USB host. */
#define CAN_LOG_DEPTH 16U
#define CAN_LOG_LENGTH 160U
static char can_log[CAN_LOG_DEPTH][CAN_LOG_LENGTH];
static uint16_t can_log_lengths[CAN_LOG_DEPTH];
static uint32_t can_log_write, can_log_read;
static uint32_t can_rx_count, can_log_dropped, can_fifo_losses;

static void CAN_Log(const char *line, int length)
{
  if (length <= 0 || length >= CAN_LOG_LENGTH) return;
  if (can_log_write - can_log_read >= CAN_LOG_DEPTH)
  {
    ++can_log_dropped;
    return;
  }
  uint32_t slot = can_log_write % CAN_LOG_DEPTH;
  for (int i = 0; i < length; ++i) can_log[slot][i] = line[i];
  can_log_lengths[slot] = (uint16_t)length;
  ++can_log_write;
}

static void CAN_RxStatus(bool armed, bool fault)
{
  char line[CAN_LOG_LENGTH];
  FDCAN_ProtocolStatusTypeDef status = {0};
  FDCAN_ErrorCountersTypeDef errors = {0};
  HAL_FDCAN_GetProtocolStatus(&hfdcan_monitor, &status);
  HAL_FDCAN_GetErrorCounters(&hfdcan_monitor, &errors);
  int n = snprintf(line, sizeof(line),
      "MAIN_CAN_RX 500kbps RX=%lu LOG_DROP=%lu FIFO_LOST=%lu REC=%lu BO=%lu LEC=%lu power=%s\r\n",
      (unsigned long)can_rx_count, (unsigned long)can_log_dropped,
      (unsigned long)can_fifo_losses, (unsigned long)errors.RxErrorCnt,
      (unsigned long)status.BusOff, (unsigned long)status.LastErrorCode,
      fault ? "FAULT_OFF" : armed ? "ON_SENT" : "WARMUP");
  CAN_Log(line, n);
  if (status.BusOff) CLEAR_BIT(hfdcan_monitor.Instance->CCCR, FDCAN_CCCR_INIT);
}

static void CAN_RxPoll(void)
{
  static uint8_t usb_line[CAN_LOG_LENGTH];
  if (__HAL_FDCAN_GET_FLAG(&hfdcan_monitor, FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST))
  {
    ++can_fifo_losses;
    __HAL_FDCAN_CLEAR_FLAG(&hfdcan_monitor, FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST);
  }
  /* Bounded work preserves watchdog timing even on a busy CAN bus. */
  for (unsigned int frame = 0; frame < 3U; ++frame)
  {
    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan_monitor, FDCAN_RX_FIFO0) == 0U) break;
    FDCAN_RxHeaderTypeDef header;
    uint8_t data[64];
    if (HAL_FDCAN_GetRxMessage(&hfdcan_monitor, FDCAN_RX_FIFO0, &header, data) != HAL_OK) break;
    ++can_rx_count;
    HAL_GPIO_TogglePin(GPIOB, LED2_PIN);
    char line[CAN_LOG_LENGTH];
    uint8_t size = CAN_DlcToBytes(header.DataLength);
    int n = snprintf(line, sizeof(line), "CAN_RX #%lu ID=%03lX DLC=%u DATA=",
                     (unsigned long)can_rx_count, (unsigned long)header.Identifier, size);
    for (uint8_t i = 0; i < size; ++i)
      n += snprintf(line + n, sizeof(line) - (size_t)n, "%s%02X", i ? " " : "", data[i]);
    if (header.Identifier == 0x124U && size == 8U)
    {
      uint32_t sequence = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                          ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
      n += snprintf(line + n, sizeof(line) - (size_t)n, " SEQ=%lu", (unsigned long)sequence);
    }
    n += snprintf(line + n, sizeof(line) - (size_t)n, "\r\n");
    CAN_Log(line, n);
  }
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) return;
  USBD_CDC_HandleTypeDef *cdc = hUsbDeviceFS.pClassData;
  if (cdc == NULL || cdc->TxState != 0U || can_log_read == can_log_write) return;
  uint32_t slot = can_log_read % CAN_LOG_DEPTH;
  uint16_t length = can_log_lengths[slot];
  for (uint16_t i = 0; i < length; ++i) usb_line[i] = (uint8_t)can_log[slot][i];
  if (CDC_Transmit_FS(usb_line, length) == USBD_OK) ++can_log_read;
}

void MainModule_CANRxMain(void)
{
  HAL_Init();
  Clock_Init();
  GPIO_Init();
  Latch_Write(false);
  MX_USB_Device_Init();
  CAN_Monitor_Init();

  bool wdi_high = false;
  bool arm_sent = false;
  bool timing_fault = false;
  uint32_t falls = 0U;
  uint32_t last_edge = HAL_GetTick();
  uint32_t last_report = last_edge;

  for (;;)
  {
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = (uint32_t)(now - last_edge);
    if (!timing_fault && elapsed > MAX_HALF_PERIOD_MS)
    {
      /* Never catch up with short WDI pulses or automatically re-arm after a stall. */
      timing_fault = true;
      Latch_Write(false);
      HAL_GPIO_WritePin(GPIOB, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);
      /* Freeze WDI at its last level; the external watchdog also times out. */
    }
    if (!timing_fault && elapsed >= HALF_PERIOD_MS)
    {
      wdi_high = !wdi_high;
      HAL_GPIO_WritePin(GPIOB, WDI_PIN | LED1_PIN,
                       wdi_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      last_edge = now;
      if (!wdi_high)
      {
        ++falls;
        if (!arm_sent && falls >= WARMUP_FALLS)
        {
          Latch_Write(true);
          arm_sent = true;
          /* This LED means the request was issued, not that VSW was measured. */
          HAL_GPIO_WritePin(GPIOB, LED2_PIN, GPIO_PIN_SET);
        }
      }
    }
    if ((uint32_t)(now - last_report) >= 1000U)
    {
      CAN_RxStatus(arm_sent, timing_fault);
      last_report = now;
    }
    CAN_RxPoll();
    HAL_Delay(1U);
  }
}


/* Reverse-direction link test: main_module -> can2ethernet. */
void MainModule_CANTxMain(void)
{
  HAL_Init();
  Clock_Init();
  GPIO_Init();
  Latch_Write(false);
  MX_USB_Device_Init();
  CAN_Monitor_Init();

  bool wdi_high = false;
  bool arm_sent = false;
  bool timing_fault = false;
  uint32_t falls = 0U;
  uint32_t last_edge = HAL_GetTick();
  uint32_t last_report = last_edge;
  uint32_t last_tx = last_edge;
  uint32_t sequence = 0U;
  uint32_t tx_buffer = 0U;
  uint32_t tx_started = 0U;
  bool tx_pending = false;
  bool last_tx_ok = false;
  uint32_t tx_ok_count = 0U;
  uint32_t tx_fail_count = 0U;
  uint32_t tx_result_sequence = 0U;

  for (;;)
  {
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = (uint32_t)(now - last_edge);
    if (!timing_fault && elapsed > MAX_HALF_PERIOD_MS)
    {
      timing_fault = true;
      Latch_Write(false);
      HAL_GPIO_WritePin(GPIOB, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);
    }
    if (!timing_fault && elapsed >= HALF_PERIOD_MS)
    {
      wdi_high = !wdi_high;
      HAL_GPIO_WritePin(GPIOB, WDI_PIN | LED1_PIN,
                       wdi_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      last_edge = now;
      if (!wdi_high)
      {
        ++falls;
        if (!arm_sent && falls >= WARMUP_FALLS)
        {
          Latch_Write(true);
          arm_sent = true;
          HAL_GPIO_WritePin(GPIOB, LED2_PIN, GPIO_PIN_SET);
        }
      }
    }

    if (tx_pending)
    {
      if (HAL_FDCAN_IsTxBufferMessagePending(&hfdcan_monitor, tx_buffer) == 0U)
      {
        last_tx_ok = (READ_BIT(hfdcan_monitor.Instance->TXBTO, tx_buffer) != 0U);
        tx_result_sequence = sequence - 1U;
        if (last_tx_ok) ++tx_ok_count; else ++tx_fail_count;
        tx_pending = false;
        HAL_GPIO_WritePin(GPIOB, LED2_PIN, last_tx_ok ? GPIO_PIN_SET : GPIO_PIN_RESET);
      }
      else if ((uint32_t)(now - tx_started) >= 100U)
      {
        (void)HAL_FDCAN_AbortTxRequest(&hfdcan_monitor, tx_buffer);
        tx_result_sequence = sequence - 1U;
        last_tx_ok = false;
        ++tx_fail_count;
        tx_pending = false;
      }
    }

    if (arm_sent && !timing_fault && !tx_pending &&
        (uint32_t)(now - last_tx) >= 500U &&
        HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan_monitor) != 0U)
    {
      FDCAN_TxHeaderTypeDef header = {0};
      uint8_t data[8] = {
        (uint8_t)sequence, (uint8_t)(sequence >> 8),
        (uint8_t)(sequence >> 16), (uint8_t)(sequence >> 24),
        0x5AU, 0xA5U, 0x3CU, 0xC3U
      };
      header.Identifier = 0x124U;
      header.IdType = FDCAN_STANDARD_ID;
      header.TxFrameType = FDCAN_DATA_FRAME;
      header.DataLength = FDCAN_DLC_BYTES_8;
      header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
      header.BitRateSwitch = FDCAN_BRS_OFF;
      header.FDFormat = FDCAN_CLASSIC_CAN;
      header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
      header.MessageMarker = 0U;
      if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan_monitor, &header, data) == HAL_OK)
      {
        tx_buffer = HAL_FDCAN_GetLatestTxFifoQRequestBuffer(&hfdcan_monitor);
        tx_started = now;
        tx_pending = true;
        ++sequence;
        last_tx = now;
      }
    }

    if ((uint32_t)(now - last_report) >= 1000U)
    {
      char line[CAN_LOG_LENGTH];
      FDCAN_ProtocolStatusTypeDef status = {0};
      FDCAN_ErrorCountersTypeDef errors = {0};
      HAL_FDCAN_GetProtocolStatus(&hfdcan_monitor, &status);
      HAL_FDCAN_GetErrorCounters(&hfdcan_monitor, &errors);
      int n = snprintf(line, sizeof(line),
          "MAIN_CAN_TX 500kbps ID=124 SEQ=%lu %s OK=%lu FAIL=%lu TEC=%lu REC=%lu BO=%lu LEC=%lu power=%s\r\n",
          (unsigned long)tx_result_sequence, last_tx_ok ? "ACK" : tx_fail_count ? "NO_ACK" : "WAIT",
          (unsigned long)tx_ok_count, (unsigned long)tx_fail_count,
          (unsigned long)errors.TxErrorCnt, (unsigned long)errors.RxErrorCnt,
          (unsigned long)status.BusOff, (unsigned long)status.LastErrorCode,
          timing_fault ? "FAULT_OFF" : arm_sent ? "ON_SENT" : "WARMUP");
      CAN_Log(line, n);
      if (status.BusOff) CLEAR_BIT(hfdcan_monitor.Instance->CCCR, FDCAN_CCCR_INIT);
      last_report = now;
    }
    CAN_RxPoll(); /* Flush the shared USB log queue; TX test does not need RX. */
    HAL_Delay(1U);
  }
}

/* TB6643KQ test on the main board. PA8/TIM1_CH1 = IN1, PA9/TIM1_CH2 = IN2. */
void MainModule_MDTestMain(void)
{
  static TIM_HandleTypeDef htim_md;
  HAL_Init();
  Clock_Init();
  GPIO_Init();
  Latch_Write(false);
  MX_USB_Device_Init();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF6_TIM1;
  HAL_GPIO_Init(GPIOA, &gpio);

  htim_md.Instance = TIM1;
  htim_md.Init.Prescaler = 0U;
  htim_md.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
  htim_md.Init.Period = 15999U; /* HSI16 / (2 * 16000) = 500 Hz */
  htim_md.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim_md.Init.RepetitionCounter = 0U;
  htim_md.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim_md) != HAL_OK) Error_Handler();

  TIM_OC_InitTypeDef channel = {0};
  channel.OCMode = TIM_OCMODE_PWM1;
  channel.Pulse = 0U;
  channel.OCPolarity = TIM_OCPOLARITY_HIGH;
  channel.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  channel.OCFastMode = TIM_OCFAST_DISABLE;
  channel.OCIdleState = TIM_OCIDLESTATE_RESET;
  channel.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim_md, &channel, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIM_PWM_ConfigChannel(&htim_md, &channel, TIM_CHANNEL_2) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim_md, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIM_PWM_Start(&htim_md, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  bool wdi_high = false;
  bool arm_sent = false;
  bool timing_fault = false;
  bool motor_started = false;
  bool motor_running = false;
  bool motor_forward = true;
  uint32_t falls = 0U;
  uint32_t last_edge = HAL_GetTick();
  uint32_t motor_deadline = 0U;
  uint32_t last_report = last_edge;
  static uint8_t line[96];

  for (;;)
  {
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = (uint32_t)(now - last_edge);
    if (!timing_fault && elapsed > MAX_HALF_PERIOD_MS)
    {
      timing_fault = true;
      Latch_Write(false);
      __HAL_TIM_SET_COMPARE(&htim_md, TIM_CHANNEL_1, 0U);
      __HAL_TIM_SET_COMPARE(&htim_md, TIM_CHANNEL_2, 0U);
      HAL_GPIO_WritePin(GPIOB, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);
    }
    if (!timing_fault && elapsed >= HALF_PERIOD_MS)
    {
      wdi_high = !wdi_high;
      HAL_GPIO_WritePin(GPIOB, WDI_PIN | LED1_PIN,
                       wdi_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      last_edge = now;
      if (!wdi_high)
      {
        ++falls;
        if (!arm_sent && falls >= WARMUP_FALLS)
        {
          Latch_Write(true);
          arm_sent = true;
          HAL_GPIO_WritePin(GPIOB, LED2_PIN, GPIO_PIN_SET);
        }
      }
    }

    if (arm_sent && !timing_fault && !motor_started)
    {
      motor_started = true;
      motor_deadline = HAL_GetTick() + 1000U; /* Allow 1 s after the ON command. */
    }
    now = HAL_GetTick();
    if (motor_started && !timing_fault &&
        (int32_t)(now - motor_deadline) >= 0)
    {
      /* Coast for 300 ms between 3-second runs; keep servicing WDI. */
      __HAL_TIM_SET_COMPARE(&htim_md, TIM_CHANNEL_1, 0U);
      __HAL_TIM_SET_COMPARE(&htim_md, TIM_CHANNEL_2, 0U);
      if (motor_running)
      {
        motor_running = false;
        motor_forward = !motor_forward;
        motor_deadline = now + 300U;
      }
      else
      {
        __HAL_TIM_SET_COMPARE(&htim_md,
                              motor_forward ? TIM_CHANNEL_1 : TIM_CHANNEL_2,
                              ((htim_md.Init.Period + 1U) * 80U) / 100U);
        motor_running = true;
        motor_deadline = now + 3000U;
      }
    }

    if ((uint32_t)(now - last_report) >= 1000U)
    {
      if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED)
      {
        USBD_CDC_HandleTypeDef *cdc = hUsbDeviceFS.pClassData;
        if (cdc != NULL && cdc->TxState == 0U)
        {
          int n = snprintf((char *)line, sizeof(line),
              "MAIN_MD falls=%lu latch=%s state=%s DIR=%s PWM=%u%%\r\n",
              (unsigned long)falls,
              timing_fault ? "FAULT_OFF" : arm_sent ? "ON_SENT" : "WARMUP",
              timing_fault ? "FAULT" : motor_running ? "RUN" : "WAIT",
              (motor_running && !timing_fault) ?
                  (motor_forward ? "FWD_IN1" : "REV_IN2") : "STOP",
              (motor_running && !timing_fault) ? 80U : 0U);
          if (n > 0 && n < (int)sizeof(line))
            (void)CDC_Transmit_FS(line, (uint16_t)n);
        }
      }
      last_report = now;
    }
    HAL_Delay(1U);
  }
}

#define SERVO_TRAVEL_MS 3000U
#define SERVO_MIN_US 500U   /* DS51150 270-degree version: 0 degrees. */
#define SERVO_MAX_US 2500U  /* 270 degrees; neutral is 1500 us / 135 degrees. */

static uint32_t ServoSmoothPulse(uint32_t from_us, uint32_t to_us, uint32_t elapsed_ms)
{
  if (elapsed_ms >= SERVO_TRAVEL_MS) return to_us;
  float t = (float)elapsed_ms / (float)SERVO_TRAVEL_MS;
  /* Quintic easing: zero velocity and acceleration at both ends. */
  float blend = t * t * t * (10.0f + t * (-15.0f + 6.0f * t));
  float pulse = (float)from_us + ((float)to_us - (float)from_us) * blend;
  return (uint32_t)(pulse + 0.5f);
}

/* DS51150: J6 PA5/TIM2_CH1 and J7 PB3/TIM2_CH2, shared +VSW. */
void MainModule_PWMServoTestMain(void)
{
  static TIM_HandleTypeDef timer;
  HAL_Init();
  Clock_Init();
  GPIO_Init(); /* MD IN1/IN2 remain LOW. */
  Latch_Write(false);
  MX_USB_Device_Init();

  __HAL_RCC_TIM2_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF1_TIM2;
  gpio.Pin = GPIO_PIN_5;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = GPIO_PIN_3;
  HAL_GPIO_Init(GPIOB, &gpio);

  timer.Instance = TIM2;
  timer.Init.Prescaler = 15U; /* HSI16 -> 1 MHz, one count per microsecond. */
  timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  timer.Init.Period = 19999U; /* 20 ms / 50 Hz. */
  timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&timer) != HAL_OK) Error_Handler();
  TIM_OC_InitTypeDef channel = {0};
  channel.OCMode = TIM_OCMODE_PWM1;
  channel.OCPolarity = TIM_OCPOLARITY_HIGH;
  channel.OCFastMode = TIM_OCFAST_DISABLE;
  channel.Pulse = 0U; /* No pulses before switched power is enabled. */
  if (HAL_TIM_PWM_ConfigChannel(&timer, &channel, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIM_PWM_ConfigChannel(&timer, &channel, TIM_CHANNEL_2) != HAL_OK ||
      HAL_TIM_PWM_Start(&timer, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIM_PWM_Start(&timer, TIM_CHANNEL_2) != HAL_OK)
    Error_Handler();

  bool wdi_high = false, armed = false, fault = false;
  uint32_t falls = 0U, pulse_us = 0U;
  uint32_t last_edge = HAL_GetTick(), last_report = last_edge;
  uint32_t move_deadline = 0U;
  uint32_t travel_started = 0U, from_us = 1500U, to_us = SERVO_MIN_US;
  static uint8_t line[128];
  for (;;)
  {
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = (uint32_t)(now - last_edge);
    if (!fault && elapsed > MAX_HALF_PERIOD_MS)
    {
      fault = true;
      pulse_us = 0U;
      __HAL_TIM_SET_COMPARE(&timer, TIM_CHANNEL_1, 0U);
      __HAL_TIM_SET_COMPARE(&timer, TIM_CHANNEL_2, 0U);
      Latch_Write(false);
      HAL_GPIO_WritePin(GPIOB, LED1_PIN | LED2_PIN, GPIO_PIN_RESET);
    }
    if (!fault && elapsed >= HALF_PERIOD_MS)
    {
      wdi_high = !wdi_high;
      HAL_GPIO_WritePin(GPIOB, WDI_PIN | LED1_PIN,
                       wdi_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      last_edge = now;
      if (!wdi_high && ++falls >= WARMUP_FALLS && !armed)
      {
        Latch_Write(true);
        armed = true;
        move_deadline = HAL_GetTick() + 1000U;
        HAL_GPIO_WritePin(GPIOB, LED2_PIN, GPIO_PIN_SET);
      }
    }
    now = HAL_GetTick();
    if (armed && !fault && (int32_t)(now - move_deadline) >= 0)
    {
      if (pulse_us == 0U) travel_started = now;
      uint32_t travel_elapsed = (uint32_t)(now - travel_started);
      if (travel_elapsed >= SERVO_TRAVEL_MS)
      {
        from_us = to_us;
        to_us = to_us == SERVO_MIN_US ? SERVO_MAX_US : SERVO_MIN_US;
        travel_started = now;
        travel_elapsed = 0U;
      }
      pulse_us = ServoSmoothPulse(from_us, to_us, travel_elapsed);
      /* Keep the two preload writes in the same timer update cycle. */
      SET_BIT(timer.Instance->CR1, TIM_CR1_UDIS);
      __HAL_TIM_SET_COMPARE(&timer, TIM_CHANNEL_1, pulse_us);
      __HAL_TIM_SET_COMPARE(&timer, TIM_CHANNEL_2, pulse_us);
      CLEAR_BIT(timer.Instance->CR1, TIM_CR1_UDIS);
      move_deadline = now + 20U; /* Update the shared target every PWM frame. */
    }
    if ((uint32_t)(now - last_report) >= 1000U)
    {
      USBD_CDC_HandleTypeDef *cdc = hUsbDeviceFS.pClassData;
      if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED &&
          cdc != NULL && cdc->TxState == 0U)
      {
        int n = snprintf((char *)line, sizeof(line),
            "PWM_SERVO DS51150 state=%s J6=%luus J7=%luus FREQ=50Hz falls=%lu\r\n",
            fault ? "FAULT_OFF" : pulse_us ? "RUN" : armed ? "SETTLE" : "WARMUP",
            (unsigned long)pulse_us, (unsigned long)pulse_us, (unsigned long)falls);
        if (n > 0 && n < (int)sizeof(line))
          (void)CDC_Transmit_FS(line, (uint16_t)n);
      }
      last_report = now;
    }
    HAL_Delay(1U);
  }
}

/* 3S balance connector on snake_pcb: GND, CELL1, CELL2, +BATT.
 * PA0/ADC_C1 divider is 10k:22k; PA1/ADC_C2 is 39k:22k;
 * PA2/ADC_C3 has 33k+33k over 22k. */
#define BATTERY_MON_WDI_PIN GPIO_PIN_13
#define BATTERY_MON_D_PIN GPIO_PIN_14
#define BATTERY_MON_CLK_PIN GPIO_PIN_15

static ADC_HandleTypeDef battery_adc;
extern USBD_HandleTypeDef hUsbDeviceFS;

static uint16_t BatteryAdcRead(uint32_t channel)
{
  ADC_ChannelConfTypeDef config = {0};
  config.Channel = channel;
  config.Rank = ADC_REGULAR_RANK_1;
  config.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  config.SingleDiff = ADC_SINGLE_ENDED;
  config.OffsetNumber = ADC_OFFSET_NONE;
  config.Offset = 0;
  if (HAL_ADC_ConfigChannel(&battery_adc, &config) != HAL_OK) Error_Handler();
  /* Flush the ADC sample capacitor after switching from another input, then
   * average several conversions so a stale first sample cannot look like a
   * real battery tap voltage. */
  uint32_t sum = 0U;
  for (uint32_t sample = 0U; sample < 9U; ++sample)
  {
    if (HAL_ADC_Start(&battery_adc) != HAL_OK) Error_Handler();
    if (HAL_ADC_PollForConversion(&battery_adc, 100U) != HAL_OK) Error_Handler();
    uint16_t value = (uint16_t)HAL_ADC_GetValue(&battery_adc);
    if (HAL_ADC_Stop(&battery_adc) != HAL_OK) Error_Handler();
    if (sample != 0U) sum += value;
  }
  return (uint16_t)((sum + 4U) / 8U);
}

static void BatteryMonitorAdcInit(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  battery_adc.Instance = ADC1;
  battery_adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  battery_adc.Init.Resolution = ADC_RESOLUTION_12B;
  battery_adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  battery_adc.Init.GainCompensation = 0;
  battery_adc.Init.ScanConvMode = ADC_SCAN_DISABLE;
  battery_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  battery_adc.Init.LowPowerAutoWait = DISABLE;
  battery_adc.Init.ContinuousConvMode = DISABLE;
  battery_adc.Init.NbrOfConversion = 1;
  battery_adc.Init.DiscontinuousConvMode = DISABLE;
  battery_adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  battery_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  battery_adc.Init.DMAContinuousRequests = DISABLE;
  battery_adc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  battery_adc.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&battery_adc) != HAL_OK) Error_Handler();
  if (HAL_ADCEx_Calibration_Start(&battery_adc, ADC_SINGLE_ENDED) != HAL_OK)
    Error_Handler();

  HAL_Delay(2U);
}

static void BatteryMonitorReport(bool power_armed, uint16_t raw1, uint16_t raw2,
                                 uint16_t raw3)
{
  static uint8_t line[192];
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) return;
  USBD_CDC_HandleTypeDef *cdc =
      (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  if (cdc == NULL || cdc->TxState != 0U) return;

  /* Use nominal 3V3 during this ADC-path diagnostic; VREFINT is deliberately
   * not sampled so it cannot contaminate the external-channel test. */
  const uint32_t vdda_mv = 3300U;

  /* Convert ADC values to cumulative balance-tap voltage using the actual
   * divider ratios; cells are differences between consecutive taps. */
  uint32_t adc1_mv = ((uint32_t)raw1 * vdda_mv + 2047U) / 4095U;
  uint32_t adc2_mv = ((uint32_t)raw2 * vdda_mv + 2047U) / 4095U;
  uint32_t adc3_mv = ((uint32_t)raw3 * vdda_mv + 2047U) / 4095U;
  uint32_t tap1_mv = (adc1_mv * 32U + 11U) / 22U;
  uint32_t tap2_mv = (adc2_mv * 61U + 11U) / 22U;
  uint32_t tap3_mv = adc3_mv * 4U;
  int32_t cell1_mv = (int32_t)tap1_mv;
  int32_t cell2_mv = (int32_t)tap2_mv - (int32_t)tap1_mv;
  int32_t cell3_mv = (int32_t)tap3_mv - (int32_t)tap2_mv;

  int n = snprintf((char *)line, sizeof(line),
      "CELL_MON power=%s vdda_nom=%lumV raw=%u,%u,%u "
      "taps=%lu,%lu,%lu mV cells=%ld,%ld,%ld mV\r\n",
      power_armed ? "ON_SENT" : "WAIT",
      (unsigned long)vdda_mv, (unsigned int)raw1, (unsigned int)raw2,
      (unsigned int)raw3,
      (unsigned long)tap1_mv, (unsigned long)tap2_mv, (unsigned long)tap3_mv,
      (long)cell1_mv, (long)cell2_mv, (long)cell3_mv);
  if (n > 0 && n < (int)sizeof(line))
    (void)CDC_Transmit_FS(line, (uint16_t)n);
}

void MainModule_BatteryMonitorMain(void)
{
  HAL_Init();
  Clock_Init();
  GPIO_Init();
  Latch_Write(false);
  MX_USB_Device_Init();
  BatteryMonitorAdcInit();

  bool wdi_high = false;
  bool power_armed = false;
  uint32_t falls = 0U;
  uint32_t last_edge = HAL_GetTick();
  uint32_t last_report = last_edge;
  for (;;)
  {
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_edge) >= 500U)
    {
      wdi_high = !wdi_high;
      HAL_GPIO_WritePin(GPIOB, BATTERY_MON_WDI_PIN | GPIO_PIN_11,
                       wdi_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      last_edge = now;
      if (!wdi_high)
      {
        ++falls;
        if (!power_armed && falls >= 6U)
        {
          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);
          Latch_Write(true);
          power_armed = true;
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
        }
      }
    }

    if ((uint32_t)(now - last_report) >= 500U)
    {
      uint16_t raw1 = BatteryAdcRead(ADC_CHANNEL_1);
      uint16_t raw2 = BatteryAdcRead(ADC_CHANNEL_2);
      uint16_t raw3 = BatteryAdcRead(ADC_CHANNEL_3);
      BatteryMonitorReport(power_armed, raw1, raw2, raw3);
      last_report = HAL_GetTick();
    }
    HAL_Delay(1U);
  }
}

static void WdtTimeoutReport(const char *state, uint32_t elapsed_ms, uint32_t falls)
{
  static uint8_t line[112];
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) return;
  USBD_CDC_HandleTypeDef *cdc =
      (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  if (cdc == NULL || cdc->TxState != 0U) return;
  int n = snprintf((char *)line, sizeof(line),
      "WDT_TEST state=%s elapsed=%lums WDI=%s falls=%lu\r\n",
      state, (unsigned long)elapsed_ms,
      (HAL_GPIO_ReadPin(GPIOB, WDI_PIN) == GPIO_PIN_SET) ? "HIGH" : "LOW",
      (unsigned long)falls);
  if (n > 0 && n < (int)sizeof(line))
    (void)CDC_Transmit_FS(line, (uint16_t)n);
}

void MainModule_WdtTimeoutTestMain(void)
{
  HAL_Init();
  Clock_Init();
  GPIO_Init();
  Latch_Write(false);
  MX_USB_Device_Init();

  bool wdi_high = false;
  bool power_on_sent = false;
  bool wdi_stopped = false;
  uint32_t falls = 0U;
  uint32_t last_edge = HAL_GetTick();
  uint32_t power_on_tick = 0U;
  uint32_t last_report = last_edge;

  WdtTimeoutReport("WARMUP", 0U, 0U);
  for (;;)
  {
    uint32_t now = HAL_GetTick();
    if (!wdi_stopped && (uint32_t)(now - last_edge) >= HALF_PERIOD_MS)
    {
      wdi_high = !wdi_high;
      HAL_GPIO_WritePin(GPIOB, WDI_PIN | LED1_PIN,
                       wdi_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      last_edge = now;

      if (!wdi_high)
      {
        ++falls;
        if (!power_on_sent && falls >= WARMUP_FALLS)
        {
          Latch_Write(true);
          power_on_sent = true;
          power_on_tick = HAL_GetTick();
          HAL_GPIO_WritePin(GPIOB, LED2_PIN, GPIO_PIN_SET);
          WdtTimeoutReport("POWER_ON_SENT", 0U, falls);
        }
      }
    }

    /* Latch_Write() blocks, and power_on_tick is captured afterwards.
     * Refresh now so an older timestamp cannot underflow the subtraction. */
    now = HAL_GetTick();
    if (power_on_sent && !wdi_stopped &&
        (uint32_t)(now - power_on_tick) >= 5000U)
    {
      /* Freeze WDI at its current level. The external TPS3430 should time out
       * and clear the latch; no further heartbeat edges are generated. */
      wdi_stopped = true;
      WdtTimeoutReport("WDI_STOPPED", (uint32_t)(now - power_on_tick), falls);

      /* Give the USB CDC host a brief chance to receive the stop event. */
      uint32_t wait_started = HAL_GetTick();
      while ((uint32_t)(HAL_GetTick() - wait_started) < 100U)
      {
        USBD_CDC_HandleTypeDef *cdc =
            (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
        if (cdc == NULL || cdc->TxState == 0U) break;
        HAL_Delay(1U);
      }
    }

    if ((uint32_t)(now - last_report) >= 1000U)
    {
      const char *state = wdi_stopped ? "WAIT_WDT_TIMEOUT" :
                          power_on_sent ? "WDI_ACTIVE" : "WARMUP";
      uint32_t elapsed = power_on_sent ? (uint32_t)(now - power_on_tick) : 0U;
      WdtTimeoutReport(state, elapsed, falls);
      last_report = now;
    }
    HAL_Delay(1U);
  }
}
