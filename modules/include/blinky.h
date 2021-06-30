#ifndef _BLINKY_H_
#define _BLINKY_H_

/*
 * @brief Interface declaration of blinky module.
 *
 * See implementation file for information about this module.
 *
 * MIT License
 * 
 * Copyright (c) 2021 Eugene R Schroeder
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

#include <stddef.h>
#include <stdint.h>

struct blinky_cfg
{
    uint32_t dout_idx;
    uint32_t code_num_blinks; // N1.
    uint32_t code_period_ms;  // T1.
    uint32_t sep_num_blinks; // N2.
    uint32_t sep_period_ms;  // T2.

};

// Core module interface functions.
int32_t blinky_get_def_cfg(struct blinky_cfg* cfg);
int32_t blinky_init(struct blinky_cfg* cfg);
int32_t blinky_start(void;);

// Other module-level APIs:
int32_t blinky_set_code_blinks(uint32_t num_blinks);
int32_t blinky_set_code_period(uint32_t period_ms);
int32_t blinky_set_sep_blinks(uint32_t num_blinks);
int32_t blinky_set_sep_period(uint32_t period_ms);

#endif // _BLINKY_H_
