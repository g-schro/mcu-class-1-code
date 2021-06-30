#ifndef _MODDEFS_H_
#define _MODDEFS_H_

/*
 * @brief Common definitions for modules.
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

#include <limits.h>

// Error codes.
#define MOD_ERR_ARG          -1
#define MOD_ERR_RESOURCE     -2
#define MOD_ERR_STATE        -3
#define MOD_ERR_BAD_CMD      -4
#define MOD_ERR_BUF_OVERRUN  -5
#define MOD_ERR_BAD_INSTANCE -6

// Get size of an array.
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// Increment a uint16_t, saturating at the maximum value.
#define INC_SAT_U16(a) do { (a) += ((a) == UINT16_MAX ? 0 : 1); } while (0)

// Clamp a numeric value between a lower and upper limit, inclusive.
#define CLAMP(a, low, high) ((a) <= (low) ? (low) : ((a) > (high) ? (high) : (a)))

#endif // _MODDEFS_H_
