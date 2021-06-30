#ifndef _CMD_H_
#define _CMD_H_

/*
 * @brief Interface declaration of cmd module.
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

#include <stdint.h>

// Number of clients supported.
#define CMD_MAX_CLIENTS  10

// Function signature for a command handler function.
typedef int32_t (*cmd_func)(int32_t argc, const char** argv);

// Information about a single command, provided by the client.
struct cmd_cmd_info {
    const char* const name; // Name of command
    const cmd_func func;    // Command function
    const char* const help; // Command help string
};

// Information provided by the client.
// - Command base name
// - Command set info.
// - Pointer to log level variable (optional).
// - Performance measurement info (optional).

struct cmd_client_info {
    const char* const name;          // Client name (first command line token)
    const int32_t num_cmds;          // Number of commands.
    const struct cmd_cmd_info* const cmds; // Pointer to array of command info
    int32_t* const log_level_ptr;    // Pointer to log level variable (or NULL)
    const int32_t num_u16_pms;       // Number of pm values.
    uint16_t* const u16_pms;         // Pointer to array of pm values
    const char* const* const u16_pm_names; // Pointer to array of pm names
};

struct cmd_arg_val {
    char type;
    union {
        void*       p;
        uint8_t*    p8;
        uint16_t*   p16;
        uint32_t*   p32;
        int32_t     i;
        uint32_t    u;
        const char* s;
    } val;
};
    
struct cmd_cfg {
    // FUTURE
};

// Core module interface functions.
int32_t cmd_init(struct cmd_cfg* cfg);

// Other APIs.
// Note: cmd_register() keeps a copy of the client_info pointer.
int32_t cmd_register(const struct cmd_client_info* client_info);
int32_t cmd_execute(char* bfr);
int32_t cmd_parse_args(int32_t argc, const char** argv, const char* fmt,
                       struct cmd_arg_val* arg_vals);

#endif // _CMD_H_
