/**
 ******************************************************************************
 * @file    onewire.h
 * @author  Stevan Simic
 * @brief   OneWire communication driver for STM32 microcontrollers
 *
 * @details
 *          This driver provides basic functions for OneWire protocol
 *          communication, including device reset, bit and byte-level
 *          read/write operations, and device presence detection.
 *
 *          It is designed to be portable across STM32 families and can be
 *          integrated into STM32CubeMX-generated projects or custom setups.
 *
 * @note    This driver assumes a GPIO-based implementation of the OneWire
 *          protocol and may require timing adjustments depending on MCU speed.
 *
 * @version 1.0
 * @date    2025-10-14
 *
 * @license MIT License
 *          Permission is hereby granted, free of charge, to any person obtaining
 *          a copy of this software and associated documentation files (the "Software"),
 *          to deal in the Software without restriction, including without limitation
 *          the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *          and/or sell copies of the Software, and to permit persons to whom the
 *          Software is furnished to do so, subject to the following conditions:
 *
 *          The above copyright notice and this permission notice shall be included
 *          in all copies or substantial portions of the Software.
 *
 *          THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *          IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *          FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *          AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *          LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *          OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *          THE SOFTWARE.
 *
 * @Reference oneWire_protocol according to article: https://www.analog.com/en/resources/technical-articles/1wire-communication-through-software.html
 *
 * oneWire Operations:
 * ┌────────────┬───────────────────────────────────────┬───────────────────────────────────────────────────────────┐
 * │Operation	│				Description				│			Implementation                                  │
 * ├────────────┼───────────────────────────────────────┼───────────────────────────────────────────────────────────┤
 * │Write 1 bit	│ Send a '1' bit to the 1-Wire slaves 	│ Drive bus low, delay A                                    │
 * │			│ 	(Write 1 time slot)					│ Release bus, delay B                                      │
 * ├────────────┼───────────────────────────────────────┼───────────────────────────────────────────────────────────┤
 * |Write 0 bit	│ Send a '0' bit to the 1-Wire slaves 	│ Drive bus low, delay C                                    │
 * │			│ 	(Write 0 time slot)					│ Release bus, delay D                                      │
 * ├────────────┼───────────────────────────────────────┼───────────────────────────────────────────────────────────┤
 * │Read bit	│ Read a bit from the 1-Wire slaves 	│ Drive bus low, delay A                                    │
 * │			│ 	(Read time slot)					│ Release bus, delay E                                      │
 * │			│										│ Sample bus to read bit from slave                         │
 * │			│										│ Delay F                                                   │
 * ├────────────┼───────────────────────────────────────┼───────────────────────────────────────────────────────────┤
 * │Reset		│ Reset the 1-Wire bus slave devices 	│ Delay G                                                   │
 * │			│ 	and ready them for a command		│ Drive bus low, delay H                                    │
 * │			│										│ Release bus, delay I                                      │
 * │			│										│ Sample bus, 0 = device(s) present, 1 = no device present  │
 * │			│										│ Delay J                                                   │
 * └────────────┴───────────────────────────────────────┴───────────────────────────────────────────────────────────┘
 *
 *
 * oneWire Protocol Timing Diagrams
 *
 * Write '1':________|         |___________________________________|_________|_
 *                   \_________/                                   |         |
 *                   |<---A--->|<------------------B------------------------>|
 *                   |         |                                   |         |
 * Write '0':________|         |                                   |_________|_
 *                   \_________|___________________________________/         |
 *                   |<-------------------C----------------------->|<---D--->|
 *                   |         |                                   |         |
 * Read Slot:________|         |_______|___________________________|_________|_
 *                   \_________/‗‗‗‗‗‗‗|‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗‗/         |    Master pulls low, releases, then samples
 *                   |         |<--E-->|<----------------F------------------>|
 *                   |         |       |                           |         |
 *
 * Reset + Presence Pulse:
 *          _________|_____|                           |___  |      __________|_  Master     Bus released    Slave pulls low
 *                   |     \___________________________/   \‗|‗‗‗‗‗/          |   pulls low  (wait)          to signal presence
 *                   |<-G->|<------------H------------>|<-I->|<-------J------>|
 *                   |     |                           |     |                |
 *
 *
 *   ┌───────────────────────────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
 *   │Parameter                  │  A  │  B  │  C  │  D  │  E  │  F  │  G  │  H  │  I  │  J  │
 *   ├────────────────┬──────────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┤
 *   │Recommended     │Standard  │  6  │  64 │  60 │  10 │  9  │  55 │  0  │ 480 │  70 │ 410 │
 *   │   Speed        ├──────────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┤
 *   │   (us)         │Overdrive │ 1.0 │ 7.5 │ 7.5 │ 2.5 │ 1.0 │  7  │ 2.5 │  70 │ 8.5 │  40 │
 *   └────────────────┴──────────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
 *
 *
 ******************************************************************************
 */


#ifndef __oneWire_H
#define __oneWire_H
#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "stm32f3xx_hal.h"

 // Select speed mode
 #define ONEWIRE_STANDARD_SPEED   1
 #define ONEWIRE_OVERDRIVE_SPEED  0

 // Set current speed mode here
//  #define ONEWIRE_SPEED_MODE       ONEWIRE_OVERDRIVE_SPEED
 #define ONEWIRE_SPEED_MODE       ONEWIRE_STANDARD_SPEED

 #if (ONEWIRE_SPEED_MODE == ONEWIRE_STANDARD_SPEED)

 // Standard Speed Delays (in microseconds)
 #define WRITE_1_LOW_DELAY        	6     // A
 #define WRITE_1_RELEASE_BUS_DELAY  64    // B
 #define WRITE_0_LOW_DELAY        	60    // C
 #define WRITE_0_RELEASE_BUS_DELAY  10    // D
 #define READ_RELEASE_BUS_DELAY     9     // E
 #define READ_SAMPLE_DELAY          55    // F
 #define RESET_INIT_DELAY         	0     // G
 #define RESET_DRIVE_BUS_LOW_DELAY 	480   // H
 #define RESET_RELEASE_BUS_DELAY  	70    // I
 #define RESET_SAMPLE_BUS_DELAY   	410   // J

 #else // Overdrive Speed

 // Overdrive Speed Delays (in microseconds)
 #define WRITE_1_LOW_DELAY        	1     // A
 #define WRITE_1_RELEASE_BUS_DELAY  7.5   // B
 #define WRITE_0_LOW_DELAY        	7.5   // C
 #define WRITE_0_RELEASE_BUS_DELAY  2.5   // D
 #define READ_RELEASE_BUS_DELAY     1     // E
 #define READ_SAMPLE_DELAY          7     // F
 #define RESET_INIT_DELAY         	2.5   // G
 #define RESET_DRIVE_BUS_LOW_DELAY 	70    // H
 #define RESET_RELEASE_BUS_DELAY  	8.5   // I
 #define RESET_SAMPLE_BUS_DELAY   	40    // J

 #endif


#define SEARCH_ROM 0xf0
#define READ_ROM 0x33
#define MATCH_ROM 0x55
#define SKIP_ROM 0xcc
#define ALARM_SEARCH 0xec



typedef enum
{
  ONEWIRE_NOT_OK = 0,
  ONEWIRE_OK
}OneWire_OK;

typedef enum
{
	ONEWIRE_IP_RESPONCE_START,
	ONEWIRE_IP_RESPONCE_READ,
	ONEWIRE_IP_RESPONCE_WAIT_END
}OneWireIPResponceState;


typedef enum {
	// General states
	ONEWIRE_STATE_IDLE,
	ONEWIRE_STATE_ERROR,
	// Init Pulse/ Reset
    ONEWIRE_STATE_RESET_INIT,
	ONEWIRE_STATE_RESET_DRIVE_BUS_LOW,
    ONEWIRE_STATE_RESET_RELEASE_BUS,
    ONEWIRE_STATE_RESET_SAMPLE_BUS,
    ONEWIRE_STATE_RESET_DONE,
	// Write High
    ONEWIRE_STATE_WRITE_HIGH_INIT,
    ONEWIRE_STATE_WRITE_HIGH_DRIVE_BUS_LOW,
    ONEWIRE_STATE_WRITE_HIGH_RELEASE_BUS,
    ONEWIRE_STATE_WRITE_HIGH_DONE,
	// Write Low
    ONEWIRE_STATE_WRITE_LOW_INIT,
    ONEWIRE_STATE_WRITE_LOW_DRIVE_BUS_LOW,
    ONEWIRE_STATE_WRITE_LOW_RELEASE_BUS,
    ONEWIRE_STATE_WRITE_LOW_DONE,
	// Master Read
    ONEWIRE_STATE_MASTER_READ_INIT,
    ONEWIRE_STATE_MASTER_READ_DRIVE_BUS_LOW,
    ONEWIRE_STATE_MASTER_READ_RELEASE_BUS,
    ONEWIRE_STATE_MASTER_READ_SAMPLE_BUS,
    ONEWIRE_STATE_MASTER_READ_DONE,
    // Slave Read
    ONEWIRE_STATE_SLAVE_READ_INIT,
    ONEWIRE_STATE_SLAVE_READ_MONITOR_BUS,
    ONEWIRE_STATE_SLAVE_READ_RELEASE_BUS,
    ONEWIRE_STATE_SLAVE_READ_SAMPLE_BUS,
    ONEWIRE_STATE_SLAVE_READ_DONE,

} OneWireState;

typedef enum {
    FLAG_ERROR,                 // set if there is error during onewire communication
    FLAG_PRESENCE_DETECTED,     // set when slave pull down line during reset state
    FLAG_BYTE_RECEIVED,         // set high when all 8 bit-s from rx_byte are send over bus
    FLAG_BYTE_SEND,             // set high when all 8 bit-s from tx_byte are send over bus
    FLAG_IS_SLAVE,              // is driver set to act as onewire slave
} OneWireFlags;

typedef enum {
    OPERATING_MODE_MASTER,
    OPERATING_MODE_SLAVE
}OneWireOperatingMode;


typedef struct {
    uint32_t Pin;                   // GPIO pin used for OneWire communication
    GPIO_TypeDef* Port;             // GPIO port used for OneWire communication 
    OneWireState state;             // Current state
    uint8_t tx_byte;                // Byte to transmit
    uint8_t rx_byte;                // Byte received
    uint8_t bit_index;              // Bit position (0–7)
    TickType_t timestamp;           // For non-blocking delays
    uint8_t flag_reg;               // error flags defined in OneWireFlags
} OneWireDriver;


void onewire_init(OneWireDriver* onewire, GPIO_TypeDef* port, uint32_t pin, OneWireOperatingMode mode);
void onewire_process(OneWireDriver *onewire);
void onewire_write_byte(OneWireDriver* onewire, uint8_t data);
uint8_t onewire_data_available(OneWireDriver* onewire);
uint8_t onewire_get_byte(OneWireDriver* onewire);

#ifdef __cplusplus
}
#endif
#endif
