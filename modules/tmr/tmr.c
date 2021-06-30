/*
 * @brief Implementation of tmr module.
 *
 * This module provides timing servies, consisting of:
 * - Software timers, with a resolution based on the system tick rate (e.g.  1
 *   ms). When the timer expires, a callback function is executed, in the
 *   context of this module (not in interrupt context). The callback function
 *   can request that the timer be restarted, to get a precise periodic timer.
 *   If a callback function is not provided, the user can poll the state of the
 *   timer to see when it has expired.
 * - A function to get the current ms time value, an unsigned value which
 *   periodically rolls over. With the current 32 bit variable, it rolls over
 *   every 49.7 days.
 *
 * The number of software timers is fixed at compile time (see TMR_NUM_INST).
 * Each software timer has one of the following states:
 *   TMR_UNUSED:  Not in use.
 *   TMR_STOPPED: Initialized (gotten) but not running.
 *   TMR_RUNNING: Initialized (gotten) and running.
 *   TMR_EXPIRED  Expired, and callback did not request restart.
 *
 * The following console commands are provided:
 * > tmr status
 * > tmr test
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stm32f4xx_ll_cortex.h"

#include "cmd.h"
#include "log.h"
#include "module.h"
#include "tmr.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

enum tmr_state {
    TMR_UNUSED = 0,
    TMR_STOPPED,
    TMR_RUNNING,
    TMR_EXPIRED,
};

// State information for a timer instance.
struct tmr_inst_info {
    uint32_t period_ms;
    uint32_t start_time;
    enum tmr_state state;
    tmr_cb_func cb_func;
    uint32_t cb_user_data;
};

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static int32_t cmd_tmr_status(int32_t argc, const char** argv);
static int32_t cmd_tmr_test(int32_t argc, const char** argv);
static enum tmr_cb_action test_cb_func(int32_t tmr_id, uint32_t user_data);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static uint32_t tick_ms_ctr;

static struct tmr_inst_info tmrs[TMR_NUM_INST];

static struct cmd_cmd_info cmds[] = {
    {
        .name = "status",
        .func = cmd_tmr_status,
        .help = "Get module status, usage: tmr status",
    },
    {
        .name = "test",
        .func = cmd_tmr_test,
        .help = "Run test, usage: tmr test [<op> [<arg1> [<arg2>]]] (enter no op/args for help)",
    },
};

static int32_t log_level = LOG_DEFAULT;

static struct cmd_client_info cmd_info = {
    .name = "tmr",
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
 * @brief Initialize tmr module instance.
 *
 * @param[in] cfg The tmr configuration. (FUTURE)
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function initializes the tmr singleton module. Generally, it should not
 * access other modules as they might not have been initialized yet. An
 * exception is the log module.
 */
int32_t tmr_init(struct tmr_cfg* cfg)
{
    log_debug("In tmr_init()\n");
    memset(&tmrs, 0, sizeof(tmrs));
    LL_SYSTICK_EnableIT();
    return 0;
}

/*
 * @brief Start tmr module instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts the tmr singleton module, to enter normal operation.
 */
int32_t tmr_start(void)
{
    int32_t result;

    log_debug("In tmr_start()\n");
    result = cmd_register(&cmd_info);
    if (result < 0) {
        log_error("tmr_start: cmd error %d\n", result);
        return MOD_ERR_RESOURCE;
    }
    return 0;
}

/*
 * @brief Run tmr module instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * @note This function should not block.
 *
 * This function runs the tmr singleton module, during normal operation.  It
 * checks for expired timers, and runs the callback function.
 */
int32_t tmr_run(void)
{
    static uint32_t last_ms = 0;
    int32_t idx;
    uint32_t now_ms = tmr_get_ms();

    // Fast exit if time has not changed.
    if (now_ms == last_ms)
        return 0;

    last_ms = now_ms;
    for (idx = 0; idx < TMR_NUM_INST; idx++) {
        struct tmr_inst_info* ti = &tmrs[idx];
        if (ti->state == TMR_RUNNING) {
            if (now_ms - ti->start_time >= ti->period_ms) {
                ti->state = TMR_EXPIRED;
                if (ti->cb_func != NULL) {
                    enum tmr_cb_action result =
                        ti->cb_func(idx, ti->cb_user_data);
                    now_ms = tmr_get_ms();
                    if (result == TMR_CB_RESTART) {
                        ti->state = TMR_RUNNING;
                        ti->start_time += ti->period_ms;
                    }
                }
            }
        }
    }
    return 0;
}

/*
 * @brief Get system tick counter.
 *
 * @return System tick value.
 *
 * @note This function should not block.
 *
 * This function runs the tmr singleton module, during normal operation.  It
 * checks for expired timers, and runs the callback function.
 */
uint32_t tmr_get_ms(void)
{
    return tick_ms_ctr;
}

/*
 * @brief Get a timer instance without a callback function.
 *
 * @param[in] ms Timeout value in ms (or 0 to get a stopped timer).
 *
 * @return Timer instance ID (>= 0), else a "MOD_ERR" value (< 0). See code
 *         for details.       
 *
 * If a positive timeout value is provided, the timer is automatically started
 * and put in the TMR_RUNNING state. Otherwise, it is put in the TMR_STOPPED
 * state.
 *
 * Since no callback function is provided, the user should poll the state of
 * the timer instance, to check if it has expired.
 */
int32_t tmr_inst_get(uint32_t ms)
{
    uint32_t idx;

    for (idx = 0; idx < TMR_NUM_INST; idx++) {
        struct tmr_inst_info* ti = &tmrs[idx];
        if (ti->state == TMR_UNUSED) {
            ti->period_ms = ms;
            if (ms == 0) {
                ti->state = TMR_STOPPED;
            } else {
                ti->start_time = tmr_get_ms();
                ti->state = TMR_RUNNING;
            }
            ti->cb_func = NULL;
            ti->cb_user_data = 0;
            return idx;
        }
    }
    // Out of timers.
    log_error("Out of timers\n");
    return MOD_ERR_RESOURCE;
}

/*
 * @brief Get a timer instance with a callback function.
 *
 * @param[in] ms Timeout value in ms (or 0 to get a stopped timer).
 * @param[in] cb_func Function to call when timer expires.
 * @param[in] cb_user_data Data to pass to callback function.
 *
 * @return Timer instance ID (>= 0), else a "MOD_ERR" value (< 0). See code
 *         for details.       
 *
 * If a positive timeout value is provided, the timer is automatically started
 * and put in the TMR_RUNNING state. Otherwise, it is put in the TMR_STOPPED
 * state.
 */
int32_t tmr_inst_get_cb(uint32_t ms, tmr_cb_func cb_func, uint32_t cb_user_data)
{
    int32_t tmr_id;

    tmr_id = tmr_inst_get(ms);
    if (tmr_id >= 0) {
        struct tmr_inst_info* ti = &tmrs[tmr_id];
        ti->cb_func = cb_func;
        ti->cb_user_data = cb_user_data;
    }
    return tmr_id;
}

/*
 * @brief Start, or restart, or stop, a timer instance.
 *
 * @param[in] tmr_id Timer ID (returned by tmr_inst_get/tmr_inst_get_cb).
 * @param[in] ms Timeout value in ms (or 0 to stop the timer).
 * @param[in] cb_user_data Data to pass to callback function.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * If the timer is already running, it will be restarted.
 */
int32_t tmr_inst_start(int32_t tmr_id, uint32_t ms)
{
    int32_t rc;

    if (tmr_id >= 0 && tmr_id < TMR_NUM_INST) {
        struct tmr_inst_info* ti = &tmrs[tmr_id];
        if (ti->state != TMR_UNUSED) {
            ti->period_ms = ms;
            if (ms == 0) {
                ti->state = TMR_STOPPED;
            } else {
                ti->start_time = tmr_get_ms();
                ti->state = TMR_RUNNING;
            }
            rc = 0;
        } else {
            rc = MOD_ERR_STATE;
        }
    } else {
        rc = MOD_ERR_ARG;
    }
    return rc;
}

/*
 * @brief Release a timer instance.
 *
 * @param[in] tmr_id Timer ID (returned by tmr_inst_get/tmr_inst_get_cb).
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t tmr_inst_release(int32_t tmr_id)
{
    if (tmr_id >= 0 && tmr_id < TMR_NUM_INST) {
        tmrs[tmr_id].state = TMR_UNUSED;
        return 0;
    }
    return MOD_ERR_ARG;
}

/*
 * @brief Check if timer instance is expired.
 *
 * @param[in] tmr_id Timer ID (returned by tmr_inst_get/tmr_inst_get_cb).
 *
 * @return Expiration state (0/1), else a "MOD_ERR" value (< 0). See code for
 *         details.
 */
int32_t tmr_inst_is_expired(int32_t tmr_id)
{
    if (tmr_id >= 0 && tmr_id < TMR_NUM_INST)
        return tmrs[tmr_id].state == TMR_EXPIRED;
    return MOD_ERR_ARG;
}

/*
 * @brief System tick interrupt handler.
 *
 * This function is called from the SysTick_Handler(). It must be a public
 * function so it can be called externally, but is not really a part of the API.
 */
void tmr_SysTick_Handler(void)
{
    tick_ms_ctr++;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Convert timer instance state enum value to a string.
 *
 * @param[in] state Timer instance state enum.
 *
 * @return String corresponding to enum value.
 */
static const char* tmr_state_str(enum tmr_state state)
{
    const char* rc;

    switch (state) {
        case TMR_UNUSED:
            rc = "unused";
            break;
        case TMR_STOPPED:
            rc = "stopped";
            break;
        case TMR_RUNNING:
            rc = "running";
            break;
        case TMR_EXPIRED:
            rc = "expired";
            break;
        default:
            rc = "invalid";
            break;
    }
    return rc;
}

/*
 * @brief Console command function for "tmr status".
 *
 * @param[in] argc Number of arguments, including "tmr"
 * @param[in] argv Argument values, including "tmr"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: tmr status
 */
static int32_t cmd_tmr_status(int32_t argc, const char** argv)
{
    uint32_t idx;
    uint32_t now_ms = tmr_get_ms();

    printf("SysTick->CTRL=0x%08lx\n", SysTick->CTRL); // TODO REMOVE
    printf("Current millisecond tmr=%lu\n\n", now_ms);

    printf("ID   Period   Start time Time left  CB User data  State\n");
    printf("-- ---------- ---------- ---------- -- ---------- ------\n");
    for (idx = 0; idx < TMR_NUM_INST; idx++) {
        struct tmr_inst_info* ti = &tmrs[idx];
        if (ti->state == TMR_UNUSED)
            continue;
        printf("%2lu %10lu %10lu %10lu %2s %10lu %s\n", idx, ti->period_ms,
               ti->start_time,
               ti->state == TMR_RUNNING ? 
               ti->period_ms - (now_ms - ti->start_time) : 0,
               ti->cb_func == NULL ? "N" : "Y",
               ti->cb_user_data,
               tmr_state_str(ti->state));
    }
    return 0;
}

/*
 * @brief Console command function for "tmr test".
 *
 * @param[in] argc Number of arguments, including "tmr"
 * @param[in] argv Argument values, including "tmr"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: tmr test [<op> [<arg1> [<arg2>]]]
 */
static int32_t cmd_tmr_test(int32_t argc, const char** argv)
{
    struct cmd_arg_val arg_vals[2];
    uint32_t param1;
    uint32_t param2;
    int32_t rc;

    // Handle help case.
    if (argc == 2) {
        printf("Test operations and param(s) are as follows:\n"
               "  Get a non-callback tmr, usage: tmr test get <ms>\n"
               "  Get a callback tmr, usage: tmr test get_cb <ms> <cb-user-data>\n"
               "  Start a tmr, usage: tmr test start <tmr-id> <ms>\n"
               "  Release a tmr, usage: tmr test release <tmr-id>\n"
               "  Check if expired, usage: tmr test is_expired <tmr-id>\n");
        return 0;
    }

    if (argc < 4) {
        printf("Insufficent arguments\n");
        return MOD_ERR_BAD_CMD;
    }

    // Initial argument checking.
    if (strcasecmp(argv[2], "get_cb") == 0 ||
        strcasecmp(argv[2], "start") == 0) {
        if (cmd_parse_args(argc-3, argv+3, "uu", arg_vals) != 2)
            return MOD_ERR_BAD_CMD;
        param2 = arg_vals[1].val.u;
    } else {
        if (cmd_parse_args(argc-3, argv+3, "u", arg_vals) != 1)
            return MOD_ERR_BAD_CMD;
    }
    param1 = arg_vals[0].val.u;

    if (strcasecmp(argv[2], "get") == 0) {
        // command: tmr test get <ms>
        rc = tmr_inst_get(param1);
    } else if (strcasecmp(argv[2], "get_cb") == 0) {
        // command: tmr test get_cb <ms> <cb_user_data>
        rc = tmr_inst_get_cb(param1, test_cb_func, param2);
    } else if (strcasecmp(argv[2], "start") == 0) {
        // command: tmr test start <id> <ms>
        rc = tmr_inst_start(param1, param2);
    } else if (strcasecmp(argv[2], "release") == 0) {
        // command: tmr test release <id>
        rc = tmr_inst_release(param1);
    } else if (strcasecmp(argv[2], "is_expired") == 0) {
        // command: tmr test is_expired <id>
        rc = tmr_inst_is_expired(param1);
    } else {
        printf("Invalid operation '%s'\n", argv[2]);
        return MOD_ERR_BAD_CMD;
    }
    printf("Operation returns %ld\n", rc);
    return 0;
}

/*
 * @brief Timer callback function for "tmr test" command.
 *
 * @param[in] tmr_id Timer ID.
 * @param[in] user_data User callback data.
 *
 * @return TMR_CB_RESTART or TMR_CB_NONE based on test logic.
 */
static enum tmr_cb_action test_cb_func(int32_t tmr_id, uint32_t user_data)
{
    log_debug("test_cb_func(tmr_id=%d user_data=%lu\n",
              tmr_id, user_data);
    return user_data == 0 ? TMR_CB_RESTART : TMR_CB_NONE;
}
