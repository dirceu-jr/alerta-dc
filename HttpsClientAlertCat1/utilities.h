/**
 * @file      utilities.h
 * @author    Dirceu Jr. (dirceuu@gmail.com)
 * @license   MIT
 * @copyright Copyright (c) 2025 Igapó Tech
 * @date      2025-08-13
 *
 */

#pragma once

// TODO:
// - Remove unused/commented variables;

#define MODEM_BAUDRATE                      (115200)
#define MODEM_DTR_PIN                       (25)
#define MODEM_TX_PIN                        (27)
#define MODEM_RX_PIN                        (26)

// The modem boot pin needs to follow the startup sequence.
#define BOARD_PWRKEY_PIN                    (4)
// The modem power switch must be set to HIGH for the modem to supply power.
#define BOARD_POWERON_PIN                   (12)
#define MODEM_RING_PIN                      (33)
#define MODEM_RESET_PIN                     (5)
// #define BOARD_MISO_PIN                      (2)
// #define BOARD_MOSI_PIN                      (15)
// #define BOARD_SCK_PIN                       (14)
// #define BOARD_SD_CS_PIN                     (13)
// #define BOARD_BAT_ADC_PIN                   (35)
#define MODEM_RESET_LEVEL                   HIGH
#define SerialAT                            Serial1

// #define MODEM_GPS_ENABLE_GPIO               (-1)
// #define MODEM_GPS_ENABLE_LEVEL              (-1)

// #define TINY_GSM_MODEM_A7670 // HTTP
#define TINY_GSM_MODEM_A76XXSSL // HTTPS

// It is only available in V1.4 version. In other versions, IO36 is not connected.
// #define BOARD_SOLAR_ADC_PIN                 (36)

#ifndef MODEM_START_WAIT_MS
    #define MODEM_START_WAIT_MS             3000
#endif

// #define MODEM_REG_SMS_ONLY

// Used on power on/off sequence
#define MODEM_POWERON_PULSE_WIDTH_MS      (100)
#define MODEM_POWEROFF_PULSE_WIDTH_MS     (3000)
