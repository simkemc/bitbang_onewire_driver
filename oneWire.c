/**
 ******************************************************************************
 * @file    onewire.c
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

#include "oneWire.h"
#include "task.h"


GPIO_PinState sampled_bus_bit = GPIO_PIN_SET;


/* Private function prototypes -----------------------------------------------*/
static void pull_low(OneWireDriver* onewire);
static void pull_high(OneWireDriver* onewire);
static GPIO_PinState read_pin(OneWireDriver* onewire);
static int is_time_expired(OneWireDriver* onewire, TickType_t expatration_time);
static void set_state(OneWireDriver* onewire, OneWireState newState);
static void pin_output_mode(OneWireDriver* onewire);
static void set_flag(OneWireDriver* onewire, OneWireFlags flagBit);
static void reset_flag(OneWireDriver* onewire, OneWireFlags flagBit);
static uint8_t get_flag(OneWireDriver* onewire, OneWireFlags flagBit);
static void store_read_bit(OneWireDriver* onewire, uint8_t value);
static void set_write_init_state(OneWireDriver* onewire,uint8_t bit);
static void handle_write_bit_done_state(OneWireDriver* onewire);



static void pull_low(OneWireDriver* onewire) {
	HAL_GPIO_WritePin(onewire->Port, onewire->Pin, GPIO_PIN_RESET);
}

static void pull_high(OneWireDriver* onewire) {
	HAL_GPIO_WritePin(onewire->Port, onewire->Pin, GPIO_PIN_SET);
}

static GPIO_PinState read_pin(OneWireDriver* onewire) {
	return HAL_GPIO_ReadPin(onewire->Port, onewire->Pin);
}

static int is_time_expired(OneWireDriver *onewire, TickType_t expatration_time) {
	return onewire->timestamp + pdMS_TO_TICKS(expatration_time) <= xTaskGetTickCount();
}

static void set_state(OneWireDriver *onewire, OneWireState new_state) {
	onewire->state = new_state;
	onewire->timestamp = xTaskGetTickCount();
}

static void pin_output_mode(OneWireDriver* onewire) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin = onewire->Pin;

	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(onewire->Port, &GPIO_InitStruct);
}

static void set_flag(OneWireDriver* onewire, OneWireFlags flag_bit) {
	if(flag_bit < 8) {
		onewire->flag_reg |= (1 << flag_bit);
	}
}

static void reset_flag(OneWireDriver* onewire, OneWireFlags flag_bit) {
	if(flag_bit < 8) {
		onewire->flag_reg &= ~(1 << flag_bit);
	}
}

static uint8_t get_flag(OneWireDriver* onewire, OneWireFlags flag_bit) {
	if(flag_bit < 8) {
		return (onewire->flag_reg >> flag_bit) & 1;
	}
	return 0;
}


static void store_read_bit(OneWireDriver* onewire, uint8_t value) {
    if (value) {
        onewire->rx_byte |= (1 << onewire->bit_index);   // Set bit
    } else {
        onewire->rx_byte &= ~(1 << onewire->bit_index);  // Clear bit
    }
}

static void set_write_init_state(OneWireDriver* onewire,uint8_t bit) {
	onewire->timestamp = xTaskGetTickCount();
	if(bit) {
		onewire->state = ONEWIRE_STATE_WRITE_HIGH_INIT;
	}
	else {
		onewire->state = ONEWIRE_STATE_WRITE_LOW_INIT;
	}
}
static void handle_write_bit_done_state(OneWireDriver* onewire){
	onewire->bit_index++;
	// set int state
	if (onewire->bit_index >= 8) {
		set_state(onewire, ONEWIRE_STATE_IDLE);
		onewire->bit_index = 0;
		onewire->rx_byte = 0;
		set_flag(onewire, FLAG_BYTE_SEND);
	}
	// set state to write 1 or 0 depending of bit that is on bit_index place in tx_byte
	else {
		set_write_init_state(onewire, (onewire->tx_byte >> onewire->bit_index)& 0x01);
	}
}


void onewire_init(OneWireDriver* onewire, GPIO_TypeDef* port, uint32_t pin) {

	onewire->Pin = pin;
	onewire->Port = port;
	pin_output_mode(onewire);
	onewire->state = ONEWIRE_STATE_IDLE;
	onewire->rx_byte = 0x00;
	onewire->tx_byte = 0x00;
	onewire->bit_index = 0;
	onewire->timestamp = 0;
	onewire->flag_reg = 0; //reset all flags
}

void onewire_process(OneWireDriver *onewire){
	
	switch (onewire->state) {
	case ONEWIRE_STATE_RESET_INIT:
		if (is_time_expired(onewire, pdMS_TO_TICKS(RESET_INIT_DELAY))){
			set_state(onewire, ONEWIRE_STATE_RESET_DRIVE_BUS_LOW);
			pull_low(onewire);
		}
		break;
	case ONEWIRE_STATE_RESET_DRIVE_BUS_LOW:
		if (is_time_expired(onewire, pdMS_TO_TICKS(RESET_DRIVE_BUS_LOW_DELAY))){
			set_state(onewire, ONEWIRE_STATE_RESET_RELEASE_BUS);
			pull_high(onewire);
		}
		break;
	case ONEWIRE_STATE_RESET_RELEASE_BUS:
		if (is_time_expired(onewire, pdMS_TO_TICKS(RESET_RELEASE_BUS_DELAY))){
			set_state(onewire, ONEWIRE_STATE_RESET_SAMPLE_BUS);
			reset_flag(onewire, FLAG_PRESENCE_DETECTED);
		}
		break;
	case ONEWIRE_STATE_RESET_SAMPLE_BUS:
		if (!is_time_expired(onewire, pdMS_TO_TICKS(RESET_SAMPLE_BUS_DELAY))){
			if (read_pin(onewire) == GPIO_PIN_RESET){
				set_flag(onewire, FLAG_PRESENCE_DETECTED);
			}
		}
		else {
			set_state(onewire, ONEWIRE_STATE_RESET_DONE);
			if (get_flag(onewire, FLAG_PRESENCE_DETECTED) !=0){
				set_flag(onewire, FLAG_ERROR);
			}
			break;
		}
	// write high
	case ONEWIRE_STATE_WRITE_HIGH_INIT:
		set_state(onewire,ONEWIRE_STATE_WRITE_HIGH_DRIVE_BUS_LOW);
		pull_low(onewire);
		break;
	case ONEWIRE_STATE_WRITE_HIGH_DRIVE_BUS_LOW:
		if (is_time_expired(onewire, pdMS_TO_TICKS(WRITE_1_LOW_DELAY))){
			set_state(onewire, ONEWIRE_STATE_WRITE_HIGH_RELEASE_BUS);
			pull_high(onewire);
		}
		break;
	case ONEWIRE_STATE_WRITE_HIGH_RELEASE_BUS:
		if (is_time_expired(onewire, pdMS_TO_TICKS(WRITE_1_RELEASE_BUS_DELAY))){
			set_state(onewire, ONEWIRE_STATE_WRITE_HIGH_DONE);
		}
		break;
		// write low
	case ONEWIRE_STATE_WRITE_LOW_INIT:
		set_state(onewire,ONEWIRE_STATE_WRITE_LOW_DRIVE_BUS_LOW);
		pull_low(onewire);
		break;
	case ONEWIRE_STATE_WRITE_LOW_DRIVE_BUS_LOW:
		if (is_time_expired(onewire, pdMS_TO_TICKS(WRITE_0_LOW_DELAY))){
			set_state(onewire, ONEWIRE_STATE_WRITE_LOW_RELEASE_BUS);
			pull_high(onewire);
		}
		break;
	case ONEWIRE_STATE_WRITE_LOW_RELEASE_BUS:
		if (is_time_expired(onewire, pdMS_TO_TICKS(WRITE_0_RELEASE_BUS_DELAY))){
			set_state(onewire, ONEWIRE_STATE_WRITE_LOW_DONE);
		}
		break;
	case ONEWIRE_STATE_WRITE_HIGH_DONE:
	case ONEWIRE_STATE_WRITE_LOW_DONE:
		handle_write_bit_done_state(onewire);
		break;
	// read
	case ONEWIRE_STATE_READ_INIT:
		set_state(onewire,ONEWIRE_STATE_READ_DRIVE_BUS_LOW);
		pull_low(onewire);
		break;
	case ONEWIRE_STATE_READ_DRIVE_BUS_LOW:
		if (is_time_expired(onewire, pdMS_TO_TICKS(WRITE_1_LOW_DELAY))){
			set_state(onewire, ONEWIRE_STATE_READ_RELEASE_BUS);
			pull_high(onewire);
		}
		break;
	case ONEWIRE_STATE_READ_RELEASE_BUS:
	if (is_time_expired(onewire, pdMS_TO_TICKS(READ_RELEASE_BUS_DELAY))){
		set_state(onewire, ONEWIRE_STATE_READ_SAMPLE_BUS);
	}
		break;
	case ONEWIRE_STATE_READ_SAMPLE_BUS:
		if (!is_time_expired(onewire, pdMS_TO_TICKS(READ_SAMPLE_DELAY))){
			if (read_pin(onewire) == GPIO_PIN_RESET){
				sampled_bus_bit = GPIO_PIN_RESET; //set temp bit to 0
			}
		}
		else {
			store_read_bit(onewire, sampled_bus_bit); // shift value from bus to left by index
			set_state(onewire, ONEWIRE_STATE_READ_DONE);
		}
		break;
	case ONEWIRE_STATE_READ_DONE:
		onewire->bit_index++; // move index 
		sampled_bus_bit = GPIO_PIN_SET;// set bit to start value	
		if (onewire->bit_index >= 8){
			set_flag(onewire, FLAG_BYTE_RECEIVED); // we received whole byte of data
			// prepair for new byte
			onewire->bit_index = 0;
			set_state(onewire, ONEWIRE_STATE_IDLE);
		}		
		else {
			set_state(onewire, ONEWIRE_STATE_READ_INIT); // continue reading until all 8 bits are read
		}
		break;
	default:
		set_state(onewire, ONEWIRE_STATE_ERROR); // state not defined
		set_flag(onewire, FLAG_ERROR);
		
		
	}
}

void onewire_write_byte(OneWireDriver* onewire, uint8_t data) {
	onewire->tx_byte = data;// set data to tx_buffer
	onewire->bit_index = 0;
	set_write_init_state(onewire, data & 0x01);// set state to write 0 or 1 depending of first(0) bite
}
