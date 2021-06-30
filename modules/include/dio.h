#ifndef _DIO_H_
#define _DIO_H_

/*
 * @brief Interface declaration of dio module.
 *
 * See implementation file for information about this module.
 *
 * MIT License
 * 
 * Copyright (c) 2021 Eugene R Schroed3er
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>

#include "stm32f4xx_ll_gpio.h"

/*
 * Guide to defining dio inputs and outputs.
 * - An array of dio_in_info and dig_out_info structures is created for inputs
 *   and outputs.
 * - Pointers to these arrays are passed to the dio module, and the dio module
 *   stores the pointers, and accesses the arrays during normal operation.
 * 
 * Fields common for inputs and outputs:
 *   - name : A readable name for the input/output.
 *   - port : One of DIO_PORT_A, DIO_PORT_B, ...
 *   - pin : One of:
 *     + DIO_PIN_0
 *     + DIO_PIN_1
 *          :
 *     + DIO_PIN_15
 *   - pull : One of:
 *     + DIO_PULL_NO
 *     + DIO_PULL_UP
 *     + DIO_PULL_DOWN
 *   - invert : True to invert the signal value.
 *
 * Fields for outputs only:
 *   - init_value : 0 or 1
 *   - speed : One of:
 *     + DIO_SPEED_FREQ_LOW
 *     + DIO_SPEED_FREQ_MEDIUM
 *     + DIO_SPEED_FREQ_HIGH
 *     + DIO_SPEED_FREQ_VERY_HIGH
 *   - output_type : One of:
 *     + DIO_OUTPUT_PUSHPULL
 *     + DIO_OUTPUT_OPENDRAIN
 */

//
// Mappings for STM32F401.
//
#define DIO_PORT_A (GPIOA)
#define DIO_PORT_B (GPIOB)
#define DIO_PORT_C (GPIOC)
#define DIO_PORT_D (GPIOD)
#define DIO_PORT_E (GPIOE)
#define DIO_PORT_F (GPIOF)
#define DIO_PORT_G (GPIOG)
#define DIO_PORT_H (GPIOH)

#define DIO_PIN_0 (LL_GPIO_PIN_0)
#define DIO_PIN_1 (LL_GPIO_PIN_1)
#define DIO_PIN_2 (LL_GPIO_PIN_2)
#define DIO_PIN_3 (LL_GPIO_PIN_3)
#define DIO_PIN_4 (LL_GPIO_PIN_4)
#define DIO_PIN_5 (LL_GPIO_PIN_5)
#define DIO_PIN_6 (LL_GPIO_PIN_6)
#define DIO_PIN_7 (LL_GPIO_PIN_7)
#define DIO_PIN_8 (LL_GPIO_PIN_8)
#define DIO_PIN_9 (LL_GPIO_PIN_9)
#define DIO_PIN_10 (LL_GPIO_PIN_10)
#define DIO_PIN_11 (LL_GPIO_PIN_11)
#define DIO_PIN_12 (LL_GPIO_PIN_12)
#define DIO_PIN_13 (LL_GPIO_PIN_13)
#define DIO_PIN_14 (LL_GPIO_PIN_14)
#define DIO_PIN_15 (LL_GPIO_PIN_15)

#define DIO_PULL_NO (LL_GPIO_PULL_NO)
#define DIO_PULL_UP (LL_GPIO_PULL_UP)
#define DIO_PULL_DOWN (LL_GPIO_PULL_DOWN)

#define DIO_SPEED_FREQ_LOW (LL_GPIO_SPEED_FREQ_LOW)
#define DIO_SPEED_FREQ_MEDIUM (LL_GPIO_SPEED_FREQ_MEDIUM)
#define DIO_SPEED_FREQ_HIGH (LL_GPIO_SPEED_FREQ_HIGH)
#define DIO_SPEED_FREQ_VERY_HIGH (LL_GPIO_SPEED_FREQ_VERY_HIGH)

#define DIO_OUTPUT_PUSHPULL (LL_GPIO_OUTPUT_PUSHPULL)
#define DIO_OUTPUT_OPENDRAIN (LL_GPIO_OUTPUT_OPENDRAIN)

typedef GPIO_TypeDef dio_port;

struct dio_in_info {
    const char* const name;
    dio_port* const port;
    const uint32_t pin;
    const uint32_t pull;
    const uint8_t invert;
};

struct dio_out_info {
    const char* const name;
    dio_port* const port;
    const uint32_t pin;
    const uint32_t pull;
    const uint8_t invert;
    const uint8_t init_value;
    const uint32_t speed;
    const uint32_t output_type;
};

struct dio_cfg
{
    const uint32_t num_inputs;
    const struct dio_in_info* const inputs;
    const uint32_t num_outputs;
    const struct dio_out_info* const outputs;
};

// Core module interface functions.
//  Note: dio_init() keeps a copy of the cfg pointer.
int32_t dio_init(struct dio_cfg* cfg);
int32_t dio_start(void);

// Other APIs.
int32_t dio_get(uint32_t din_idx);
int32_t dio_get_out(uint32_t dout_idx);
int32_t dio_set(uint32_t dout_idx, uint32_t value);
int32_t dio_get_num_in(void);
int32_t dio_get_num_out(void);

#endif // _DIO_H_
