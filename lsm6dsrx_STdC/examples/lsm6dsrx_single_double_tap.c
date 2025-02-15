/*
 ******************************************************************************
 * @file    lsm6dsrx_single_double_tap.c
 * @author  Sensors Software Solution Team
 * @brief   This file show the simplest way to detect single and double tap
 *          from sensor.
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/*
 * This example was developed using the following STMicroelectronics
 * evaluation boards:
 *
 * - STEVAL_MKI109V3 + STEVAL-MKI195V1
 * - NUCLEO_F411RE + STEVAL-MKI195V1
 * - DISCOVERY_SPC584B + STEVAL-MKI195V1
 *
 * Used interfaces:
 *
 * STEVAL_MKI109V3    - Host side:   USB (Virtual COM)
 *                    - Sensor side: SPI(Default) / I2C(supported)
 *
 * NUCLEO_STM32F411RE - Host side: UART(COM) to USB bridge
 *                    - Sensor side: I2C(Default) / SPI(supported)
 *
 * DISCOVERY_SPC584B  - Host side: UART(COM) to USB bridge
 *                    - Sensor side: I2C(Default) / SPI(supported)
 *
 * If you need to run this example on a different hardware platform a
 * modification of the functions: `platform_write`, `platform_read`,
 * `tx_com` and 'platform_init' is required.
 *
 */

/* STMicroelectronics evaluation boards definition
 *
 * Please uncomment ONLY the evaluation boards in use.
 * If a different hardware is used please comment all
 * following target board and redefine yours.
 */

//#define STEVAL_MKI109V3  /* little endian */
//#define NUCLEO_F411RE    /* little endian */
//#define SPC584B_DIS      /* big endian */

/* ATTENTION: By default the driver is little endian. If you need switch
 *            to big endian please see "Endianness definitions" in the
 *            header file of the driver (_reg.h).
 */

#if defined(STEVAL_MKI109V3)
/* MKI109V3: Define communication interface */
#define SENSOR_BUS hspi2
/* MKI109V3: Vdd and Vddio power supply values */
#define PWM_3V3 915

#elif defined(NUCLEO_F411RE)
/* NUCLEO_F411RE: Define communication interface */
#define SENSOR_BUS hi2c1

#elif defined(SPC584B_DIS)
/* DISCOVERY_SPC584B: Define communication interface */
#define SENSOR_BUS I2CD1

#endif

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <lsm6dsrx_reg.h>

#if defined(NUCLEO_F411RE)
#include "stm32f4xx_hal.h"
#include "usart.h"
#include "gpio.h"
#include "i2c.h"

#elif defined(STEVAL_MKI109V3)
#include "stm32f4xx_hal.h"
#include "usbd_cdc_if.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"

#elif defined(SPC584B_DIS)
#include "components.h"
#endif

/* Private macro -------------------------------------------------------------*/
#define    BOOT_TIME            10 //ms

/* Private variables ---------------------------------------------------------*/
static uint8_t whoamI, rst;
static uint8_t tx_buffer[1000];

/* Extern variables ----------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/*
 *   WARNING:
 *   Functions declare in this section are defined at the end of this file
 *   and are strictly related to the hardware platform used.
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len);
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len);
static void tx_com( uint8_t *tx_buffer, uint16_t len );
static void platform_delay(uint32_t ms);
static void platform_init(void);

/* Main Example --------------------------------------------------------------*/
void lsm6dsrx_double_tap(void)
{
  stmdev_ctx_t dev_ctx;
  /* Uncomment to configure INT 1. */
  //lsm6dsrx_pin_int1_route_t int1_route;
  /* Uncomment to configure INT 2. */
  lsm6dsrx_pin_int2_route_t int2_route;
  /* Initialize mems driver interface. */
  dev_ctx.write_reg = platform_write;
  dev_ctx.read_reg = platform_read;
  dev_ctx.handle = &SENSOR_BUS;
  /* Init test platform */
  platform_init();
  /* Wait sensor boot time */
  platform_delay(BOOT_TIME);
  /* Check device ID. */
  lsm6dsrx_device_id_get(&dev_ctx, &whoamI);

  if (whoamI != LSM6DSRX_ID)
    while (1);

  /* Restore default configuration. */
  lsm6dsrx_reset_set(&dev_ctx, PROPERTY_ENABLE);

  do {
    lsm6dsrx_reset_get(&dev_ctx, &rst);
  } while (rst);

  /* Disable I3C interface. */
  lsm6dsrx_i3c_disable_set(&dev_ctx, LSM6DSRX_I3C_DISABLE);
  /* Set XL Output Data Rate to 417 Hz. */
  lsm6dsrx_xl_data_rate_set(&dev_ctx, LSM6DSRX_XL_ODR_417Hz);
  /* Set 2g full XL scale */
  lsm6dsrx_xl_full_scale_set(&dev_ctx, LSM6DSRX_2g);
  /* Enable Tap detection on X, Y, Z
   */
  lsm6dsrx_tap_detection_on_z_set(&dev_ctx, PROPERTY_ENABLE);
  lsm6dsrx_tap_detection_on_y_set(&dev_ctx, PROPERTY_ENABLE);
  lsm6dsrx_tap_detection_on_x_set(&dev_ctx, PROPERTY_ENABLE);
  /* Set Tap threshold to 01000b, therefore the tap threshold
   * is 500 mg (= 12 * FS_XL / 32 )
   */
  lsm6dsrx_tap_threshold_x_set(&dev_ctx, 0x08);
  lsm6dsrx_tap_threshold_y_set(&dev_ctx, 0x08);
  lsm6dsrx_tap_threshold_z_set(&dev_ctx, 0x08);
  /* Configure Single and Double Tap parameter
   *
   * For the maximum time between two consecutive detected taps, the DUR
   * field of the INT_DUR2 register is set to 0111b, therefore the Duration
   * time is 538.5 ms (= 7 * 32 * ODR_XL)
   *
   * The SHOCK field of the INT_DUR2 register is set to 11b, therefore
   * the Shock time is 57.36 ms (= 3 * 8 * ODR_XL)
   *
   * The QUIET field of the INT_DUR2 register is set to 11b, therefore
   * the Quiet time is 28.68 ms (= 3 * 4 * ODR_XL)
   */
  lsm6dsrx_tap_dur_set(&dev_ctx, 0x07);
  lsm6dsrx_tap_quiet_set(&dev_ctx, 0x03);
  lsm6dsrx_tap_shock_set(&dev_ctx, 0x03);
  /* Enable Single and Double Tap detection. */
  lsm6dsrx_tap_mode_set(&dev_ctx, LSM6DSRX_BOTH_SINGLE_DOUBLE);
  /* For single tap only uncomments next function */
  //lsm6dsrx_tap_mode_set(&dev_ctx, LSM6DSRX_ONLY_SINGLE);
  /* Enable interrupt generation on Single and Double Tap INT1 pin */
  //lsm6dsrx_pin_int1_route_get(&dev_ctx, &int1_route);
  /* For single tap only comment next function */
  //int1_route.md1_cfg.int1_double_tap = PROPERTY_ENABLE;
  //int1_route.md1_cfg.int1_single_tap = PROPERTY_ENABLE;
  //lsm6dsrx_pin_int1_route_set(&dev_ctx, &int1_route);
  /* Uncomment if interrupt generation on Single and Double Tap INT2 pin */
  lsm6dsrx_pin_int2_route_get(&dev_ctx, &int2_route);
  /* For single tap only comment next function */
  int2_route.md2_cfg.int2_double_tap = PROPERTY_ENABLE;
  int2_route.md2_cfg.int2_single_tap = PROPERTY_ENABLE;
  lsm6dsrx_pin_int2_route_set(&dev_ctx, &int2_route);

  /* Wait Events */
  while (1) {
    lsm6dsrx_all_sources_t all_source;
    /* Check if Tap events */
    lsm6dsrx_all_sources_get(&dev_ctx, &all_source);

    if (all_source.tap_src.double_tap) {
      sprintf((char *)tx_buffer, "D-Tap: ");

      if (all_source.tap_src.x_tap) {
        strcat((char *)tx_buffer, "x-axis");
      }

      else if (all_source.tap_src.y_tap) {
        strcat((char *)tx_buffer, "y-axis");
      }

      else {
        strcat((char *)tx_buffer, "z-axis");
      }

      if (all_source.tap_src.tap_sign) {
        strcat((char *)tx_buffer, " negative");
      }

      else {
        strcat((char *)tx_buffer, " positive");
      }

      strcat((char *)tx_buffer, " sign\r\n");
      tx_com(tx_buffer, strlen((char const *)tx_buffer));
    }

    if (all_source.tap_src.single_tap) {
      sprintf((char *)tx_buffer, "S-Tap: ");

      if (all_source.tap_src.x_tap) {
        strcat((char *)tx_buffer, "x-axis");
      }

      else if (all_source.tap_src.y_tap) {
        strcat((char *)tx_buffer, "y-axis");
      }

      else {
        strcat((char *)tx_buffer, "z-axis");
      }

      if (all_source.tap_src.tap_sign) {
        strcat((char *)tx_buffer, " negative");
      }

      else {
        strcat((char *)tx_buffer, " positive");
      }

      strcat((char *)tx_buffer, " sign\r\n");
      tx_com(tx_buffer, strlen((char const *)tx_buffer));
    }
  }
}

/*
 * @brief  Write generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to write
 * @param  bufp      pointer to data to write in register reg
 * @param  len       number of consecutive register to write
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len)
{
#if defined(NUCLEO_F411RE)
  HAL_I2C_Mem_Write(handle, LSM6DSRX_I2C_ADD_L, reg,
                    I2C_MEMADD_SIZE_8BIT, (uint8_t*) bufp, len, 1000);
#elif defined(STEVAL_MKI109V3)
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(handle, &reg, 1, 1000);
  HAL_SPI_Transmit(handle, (uint8_t*) bufp, len, 1000);
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_SET);
#elif defined(SPC584B_DIS)
  i2c_lld_write(handle,  LSM6DSRX_I2C_ADD_L & 0xFE, reg, (uint8_t*) bufp, len);
#endif
  return 0;
}

/*
 * @brief  Read generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to read
 * @param  bufp      pointer to buffer that store the data read
 * @param  len       number of consecutive register to read
 *
 */
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len)
{
#if defined(NUCLEO_F411RE)
  HAL_I2C_Mem_Read(handle, LSM6DSRX_I2C_ADD_L, reg,
                   I2C_MEMADD_SIZE_8BIT, bufp, len, 1000);
#elif defined(STEVAL_MKI109V3)
  reg |= 0x80;
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(handle, &reg, 1, 1000);
  HAL_SPI_Receive(handle, bufp, len, 1000);
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_SET);
#elif defined(SPC584B_DIS)
  i2c_lld_read(handle, LSM6DSRX_I2C_ADD_L & 0xFE, reg, bufp, len);
#endif
  return 0;
}

/*
 * @brief  Write generic device register (platform dependent)
 *
 * @param  tx_buffer     buffer to transmit
 * @param  len           number of byte to send
 *
 */
static void tx_com(uint8_t *tx_buffer, uint16_t len)
{
#if defined(NUCLEO_F411RE)
  HAL_UART_Transmit(&huart2, tx_buffer, len, 1000);
#elif defined(STEVAL_MKI109V3)
  CDC_Transmit_FS(tx_buffer, len);
#elif defined(SPC584B_DIS)
  sd_lld_write(&SD2, tx_buffer, len);
#endif
}

/*
 * @brief  platform specific delay (platform dependent)
 *
 * @param  ms        delay in ms
 *
 */
static void platform_delay(uint32_t ms)
{
#if defined(NUCLEO_F411RE) | defined(STEVAL_MKI109V3)
  HAL_Delay(ms);
#elif defined(SPC584B_DIS)
  osalThreadDelayMilliseconds(ms);
#endif
}

/*
 * @brief  platform specific initialization (platform dependent)
 */
static void platform_init(void)
{
#if defined(STEVAL_MKI109V3)
  TIM3->CCR1 = PWM_3V3;
  TIM3->CCR2 = PWM_3V3;
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_Delay(1000);
#endif
}
