#ifndef _STAT_H_
#define _STAT_H_

/*
 * @brief Interface declaration of stat utility.
 *
 * See implementation file for information about this utility.
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

struct stat_dur {
    uint64_t accum_ms;
    uint32_t start_ms;
    uint32_t min;
    uint32_t max;
    uint32_t samples;
    bool started;
};

void stat_dur_init(struct stat_dur* stat);
void stat_dur_start(struct stat_dur* stat);
void stat_dur_restart(struct stat_dur* stat);
void stat_dur_end(struct stat_dur* stat);
uint32_t stat_dur_avg_us(struct stat_dur* stat);

#endif // _STAT_H_
