#ifndef _TMR_H_
#define _TMR_H_

/*
 * @brief Interface declaration of tmr module.
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

#define TMR_NUM_INST 5

// Return values from a timer handler indicating whether or not to restart the
// timer.
enum tmr_cb_action {
    TMR_CB_NONE,
    TMR_CB_RESTART,
};

// Timer handler function signature.
typedef enum tmr_cb_action (*tmr_cb_func)(int32_t tmr_id, uint32_t user_data);

struct tmr_cfg
{
    // FUTURE
};

// Core module interface functions.
int32_t tmr_init(struct tmr_cfg* cfg);;
int32_t tmr_start(void);
int32_t tmr_run(void);

// Other module-level APIs:
uint32_t tmr_get_ms(void);

// Timer instance-level APIs.
int32_t tmr_inst_get(uint32_t ms);
int32_t tmr_inst_get_cb(uint32_t ms, tmr_cb_func cb_func, uint32_t cb_user_data);
int32_t tmr_inst_start(int32_t tmr_id, uint32_t ms);
int32_t tmr_inst_release(int32_t tmr_id);
int32_t tmr_inst_is_expired(int32_t tmr_id);

// Private API that must be declared publically.
void tmr_SysTick_Handler(void);

#endif // _TMR_H_
