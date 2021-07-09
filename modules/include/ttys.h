#ifndef _TTYS_H_
#define _TTYS_H_

/*
 * @brief Interface declaration of ttys module.
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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// UART numbering is based on the MCU hardware definition.
enum ttys_instance_id {
    TTYS_INSTANCE_UART1,
    TTYS_INSTANCE_UART2, // File discriptor 1 (stdin/stdout).
    TTYS_INSTANCE_UART3,

    TTYS_NUM_INSTANCES
};

#define TTYS_RX_BUF_SIZE 80
#define TTYS_TX_BUF_SIZE 1024

struct ttys_cfg {
    bool create_stream;
    bool send_cr_after_nl;
};

// Core module interface functions.
int32_t ttys_get_def_cfg(enum ttys_instance_id instance_id, struct ttys_cfg* cfg);
int32_t ttys_init(enum ttys_instance_id instance_id, struct ttys_cfg* cfg);
int32_t ttys_start(enum ttys_instance_id instance_id);

// Other APIs.
int32_t ttys_putc(enum ttys_instance_id instance_id, char c);
int32_t ttys_getc(enum ttys_instance_id instance_id, char* c);
int ttys_get_fd(enum ttys_instance_id instance_id);
FILE* ttys_get_stream(enum ttys_instance_id instance_id);

#endif // _TTYS_H_
