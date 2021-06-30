/*
 * @brief Implementation of mem module.
 *
 * This module simply provides console commands to read and write memory, for
 * debugging.
 *
 * The following console commands are provided:
 * > mem r
 * > mem w
 * See code for details.
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

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "log.h"
#include "module.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static int32_t cmd_mem_read(int32_t argc, const char** argv);
static int32_t cmd_mem_write(int32_t argc, const char** argv);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static struct cmd_cmd_info cmds[] = {
    {
        .name = "r",
        .func = cmd_mem_read,
        .help = "Read memory, usage: mem r addr [count [data-unit-size]]",
    },
    {
        .name = "w",
        .func = cmd_mem_write,
        .help = "Write memory, usage: mem w addr <data-unit-size> value ...",
    },
};

static int32_t log_level = LOG_DEFAULT;

static struct cmd_client_info cmd_info = {
    .name = "mem",
    .num_cmds = ARRAY_SIZE(cmds),
    .cmds = cmds,
    .log_level_ptr = &log_level,
};

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Start mem instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts the mem singleton module, to enter normal operation.
 */
int32_t mem_start(void)
{
    int32_t result;

    log_debug("In mem_start()\n");
    result = cmd_register(&cmd_info);
    if (result < 0) {
        log_error("mem_start: cmd error %d\n", result);
        return MOD_ERR_RESOURCE;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Console command function for "mem r".
 *
 * @param[in] argc Number of arguments, including "mem"
 * @param[in] argv Argument values, including "mem"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: mem r addr [count [data-unit-size]]
 */
static int32_t cmd_mem_read(int32_t argc, const char** argv)
{
    int32_t num_args;
    struct cmd_arg_val arg_vals[3];
    uint32_t count = 1;
    uint32_t unit_size = 4;
    int32_t items_per_line;
    int32_t line_item_ctr;
    
    num_args = cmd_parse_args(argc-2, argv+2, "p[u[u]]", arg_vals);
    if (num_args >= 2) {
        count = arg_vals[1].val.u;
    }
    if (num_args >= 3) {
        unit_size = arg_vals[2].val.u;
    }
    if (num_args < 1 || num_args > 3)
        return num_args;
            
    switch (unit_size) {
        case 1:
            items_per_line = 16;
            break;
        case 2:
            items_per_line = 8;
            break;
        case 4:
            items_per_line = 4;
            break;
        default:
            printf("Invalid data unit size %lu\n", unit_size);
            return MOD_ERR_ARG;
    }
    line_item_ctr = 0;
    while (count-- > 0) {
        if (line_item_ctr == 0)
            printf("%08x:", (unsigned)arg_vals[0].val.p);
        switch (unit_size) {
            case 1:
                printf(" %02x", *arg_vals[0].val.p8);
                break;
            case 2:
                printf(" %04x", *arg_vals[0].val.p16);
                break;
            case 4:
                printf(" %08lx", *arg_vals[0].val.p32);
                break;
        }
        arg_vals[0].val.p8 += unit_size;
        if (++line_item_ctr == items_per_line)
        {
            printf("\n");
            line_item_ctr = 0;
        }
    }
    if (line_item_ctr != 0)
        printf("\n");
    return 0;
}


/*
 * @brief Console command function for "mem r".
 *
 * @param[in] argc Number of arguments, including "mem"
 * @param[in] argv Argument values, including "mem"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: mem w addr <data-unit-size> value ...
 */
static int32_t cmd_mem_write(int32_t argc, const char** argv)
{
    int32_t num_args;
    struct cmd_arg_val arg_vals[6];
    uint32_t unit_size;
    int32_t val_arg_idx;

    num_args = cmd_parse_args(argc-2, argv+2, "puu[u[u[u]]]", arg_vals);
    if (num_args < 3)
        return num_args;
    unit_size = arg_vals[1].val.u;
    if (unit_size != 1 && unit_size != 2 && unit_size != 4) {
        printf("Invalid data unit_size %lu\n", unit_size);
        return MOD_ERR_ARG;
    }
    val_arg_idx = 2;
    while (val_arg_idx < num_args) {
        switch (unit_size) {
            case 1:
                *arg_vals[0].val.p8++ = (uint8_t)arg_vals[val_arg_idx].val.u;
                break;
            case 2:
                *arg_vals[0].val.p16++ = (uint16_t)arg_vals[val_arg_idx].val.u;
                break;
            case 4:
                *arg_vals[0].val.p32++ = (uint32_t)arg_vals[val_arg_idx].val.u;
                break;
        }
        val_arg_idx++;
    }
    return 0;
}
