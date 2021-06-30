#ifndef _LOG_H_
#define _LOG_H_

/*
 * @brief Interface declaration of log module.
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

// The log toggle char at the console is ctrl-L which is form feed, or 0x0c.
#define LOG_TOGGLE_CHAR '\x0c'

enum log_level {
    LOG_OFF = 0,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE,
    LOG_DEFAULT = LOG_INFO
};

#define LOG_LEVEL_NAMES "off, error, warning, info, debug, trace"
#define LOG_LEVEL_NAMES_CSV "off", "error", "warning", "info", "debug", "trace"

// Core module interface functions.

// Other APIs.
void log_toggle_active(void);
bool log_is_active(void);
void log_printf(const char* fmt, ...);

#define log_error(fmt, ...) do { if (_log_active && log_level >= LOG_ERROR) \
            log_printf("ERR  " fmt, ##__VA_ARGS__); } while (0)
#define log_warning(fmt, ...) do { if (_log_active && log_level >= LOG_WARNING) \
            log_printf("WARN " fmt, ##__VA_ARGS__); } while (0)
#define log_info(fmt, ...) do { if (_log_active && log_level >= LOG_INFO) \
            log_printf("INFO " fmt, ##__VA_ARGS__); } while (0)
#define log_debug(fmt, ...) do { if (_log_active && log_level >= LOG_DEBUG) \
            log_printf("DBG  " fmt, ##__VA_ARGS__); } while (0)
#define log_trace(fmt, ...) do { if (_log_active && log_level >= LOG_TRACE) \
            log_printf("TRC  " fmt, ##__VA_ARGS__); } while (0)

// Following variable is global to allow efficient access by macros,
// but is considered private.

extern bool _log_active;

#endif // _LOG_H_
