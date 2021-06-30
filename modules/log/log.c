/*
 * @brief Implementation of log module.
 *
 * This module provides a simple logging utility. Most of the API is implemented
 * via macros found in the header file. Output is written to a ttys instance
 * specified during module creation.
 *
 * A client that uses the log API macros must have a private/static variable
 * called "log_level" to control the amount of logging. Typically, the client
 * provides a pointer to this variable to the cmd module. The cmd module then
 * allows the console user to display and set the client's logging level.
 *
 * There is also a global variable, log_active, that can be used to inhibit
 * log output. The console module toggles this variable on/off based on a
 * input key (ctrl-L).
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

#include <stdarg.h>
#include <stdio.h>

#include "tmr.h"
#include "log.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

bool _log_active = true;

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Toggle state of "log active".
 */
void log_toggle_active(void)
{
    _log_active = _log_active ? false : true;
}

/*
 * @brief Get state of "log active".
 *
 * @return Value of log active.
 *
 * Each output line starts with a ms resolution timestamp (relative time).
 */
bool log_is_active(void)
{
    return _log_active;
}

/*
 * @brief Base "printf" style function for logging.
 *
 * @param[in] fmt Format string
 *
 * Each output line starts with a ms resolution timestamp (relative time).
 */
void log_printf(const char* fmt, ...)
{
    va_list args;
    uint32_t ms = tmr_get_ms();

    printf("%lu.%03lu ", ms / 1000U, ms % 1000U);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////
