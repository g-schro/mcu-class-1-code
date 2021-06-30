/*
 * @brief Implementation of blinky module.
 *
 * This module blinks an LED a given number of times, at a given rate. It then
 * blinks a "separator" which some number of blinks at a different
 * rate. Normally the separator rate is much faster than the blink rate. This
 * process then repeats.
 *
 * The following console commands are provided:
 * > blinky status
 * > blinky blinks num-blinks period-ms
 * > blinky sep num-blinks period-ms
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
#include "dio.h"
#include "log.h"
#include "module.h"
#include "tmr.h"

#include "blinky.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

#define PRE_BLINK_DELAY_MS 2000
#define POST_BLINK_DELAY_MS 2000

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

enum states {
    STATE_OFF,
    STATE_SEPARATOR_ON,
    STATE_SEPARATOR_OFF,
    STATE_PRE_BLINK_DELAY,
    STATE_BLINK_ON,
    STATE_BLINK_OFF,
    STATE_POST_BLINK_DELAY,
};

struct blinky_state
{
    struct blinky_cfg cfg;
    enum states state;
    bool valid_dio;
    uint32_t blink_counter;
    int32_t tmr_id;
};
////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static int32_t cmd_blinky_status(int32_t argc, const char** argv);
static int32_t cmd_blinky_blinks(int32_t argc, const char** argv);
static void start();
static enum tmr_cb_action tmr_cb(int32_t tmr_id, uint32_t user_data);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

struct blinky_state state;

static struct cmd_cmd_info cmds[] = {
    {
        .name = "status",
        .func = cmd_blinky_status,
        .help = "Get module status, usage: blinky status",
    },
    {
        .name = "blinks",
        .func = cmd_blinky_blinks,
        .help = "Set blinks/period, usage: blinky blinks num-blinks [period-ms]",
    },
    {
        .name = "sep",
        .func = cmd_blinky_blinks,
        .help = "Set separator blinks/period, usage: blinky sep num-blinks [period-ms]",
    },
};

static int32_t log_level = LOG_DEFAULT;

static struct cmd_client_info cmd_info = {
    .name = "blinky",
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
 * @brief Get default blinky configuration.
 *
 * @param[out] cfg The blinky configuration with defaults filled in.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t blinky_get_def_cfg(struct blinky_cfg* cfg)
{
    if (cfg == NULL)
        return MOD_ERR_ARG;

    memset(cfg, 0, sizeof(*cfg));
    cfg->code_num_blinks = 1;
    cfg->code_period_ms = 1000;
    cfg->sep_num_blinks = 5;
    cfg->sep_period_ms = 200;

    return 0;
}

/*
 * @brief Initialize blinky module instance.
 *
 * @param[in] cfg The blinky configuration. (FUTURE)
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function initializes the blinky singleton module. Generally, it should
 * not access other modules as they might not have been initialized yet. An
 * exception is the log module.
 */
int32_t blinky_init(struct blinky_cfg* cfg)
{
    log_debug("In blinky_init()\n");

    memset(&state, 0, sizeof(state));
    state.tmr_id = -1;

    state.cfg = *cfg;
    return 0;
}

/*
 * @brief Start blinky module instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts the blinky singleton module, to enter normal operation.
 */
int32_t blinky_start(void)
{
    int32_t result;

    log_debug("In blinky_start()\n");
    result = dio_get_num_out();
    if (result < 0) {
        log_error("blinky_start: dio error %d\n", result);
        return MOD_ERR_RESOURCE;
    }
    else if (state.cfg.dout_idx >= result) {
        log_error("blinky_start: bad dout_idx %u (>=%d)\n",
                  state.cfg.dout_idx, result);
        return MOD_ERR_ARG;
    }
    state.valid_dio = true;
    result = cmd_register(&cmd_info);
    if (result < 0) {
        log_error("blinky_start: cmd error %d\n", result);
        return MOD_ERR_RESOURCE;
    }
    state.tmr_id = tmr_inst_get_cb(0, tmr_cb, 0);
    if (state.tmr_id < 0) {
        log_error("blinky_start: tmr error %d\n", state.tmr_id);
        return MOD_ERR_RESOURCE;
    }
    start();
    return 0;
}

/*
 * @brief Set number of code blinks.
 *
 * @param[in] num_blinks The number of blinks (set to 0 for no blinks).
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t blinky_set_code_blinks(uint32_t num_blinks)
{
    state.cfg.code_num_blinks = num_blinks;
    start();
    return 0;
}

/*
 * @brief Set number of separator blinks.
 *
 * @param[in] num_blinks The number of separator blinks (set to 0 for no
 *                       separator blinks).
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t blinky_set_sep_blinks(uint32_t num_blinks)
{
    state.cfg.sep_num_blinks = num_blinks;
    start();
    return 0;
}

/*
 * @brief Set code blink period.
 *
 * @param[in] period_ms Period in ms (if set to 0, there will be no code blinks).
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t blinky_set_code_period(uint32_t period_ms)
{
    state.cfg.code_period_ms = period_ms;
    start();
    return 0;
}

/*
 * @brief Set separator blink period.
 *
 * @param[in] period_ms Period in ms (if set to 0, there will be no separator
 *                      blinks).
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t blinky_set_sep_period(uint32_t period_ms)
{
    state.cfg.sep_period_ms = period_ms;
    start();
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Console command function for "blinky status".
 *
 * @param[in] argc Number of arguments, including "blinky"
 * @param[in] argv Argument values, including "blinky"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: blinky status
 */
static int32_t cmd_blinky_status(int32_t argc, const char** argv)
{
    printf("state=%d blink_counter=%lu tmr_id=%ld\n",
           state.state, state.blink_counter, state.tmr_id);
    printf("code-num-blinks=%lu code-period-ms=%lu sep-num-blinks=%lu sep-perioid-ms=%lu\n",
           state.cfg.code_num_blinks, state.cfg.code_period_ms,
           state.cfg.sep_num_blinks, state.cfg.sep_period_ms);
    return 0;
}

/*
 * @brief Console command function for "blinky blinks" and "blinky sep"
 *
 * @param[in] argc Number of arguments, including "blinky"
 * @param[in] argv Argument values, including "blinky"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage:
 *    blinky blinks num-blinks [period-ms]
 *    blinky sep num-blinks [period-ms]
 */
static int32_t cmd_blinky_blinks(int32_t argc, const char** argv)
{
    struct cmd_arg_val arg_vals[2];
    int32_t num_args;

    num_args = cmd_parse_args(argc-2, argv+2, "u[u]", arg_vals);
    if (num_args < 0)
        return MOD_ERR_BAD_CMD;

    if (strcasecmp(argv[1], "blinks") == 0) {
        blinky_set_code_blinks(arg_vals[0].val.u);
        if (num_args >= 2)
            blinky_set_code_period(arg_vals[1].val.u);
    } else {
        blinky_set_sep_blinks(arg_vals[0].val.u);
        if (num_args >= 2)
            blinky_set_sep_period(arg_vals[1].val.u);
    }
    start();
    return 0;
}

/*
 * @brief Start or restart blinky.
 *
 * This is called during the initial module start, and after any change of
 * parameters.
 */
void start(void)
{
    if (!state.valid_dio || state.tmr_id < 0)
        return;

    dio_set(state.cfg.dout_idx, false);
    state.state = STATE_PRE_BLINK_DELAY;
    tmr_inst_start(state.tmr_id, PRE_BLINK_DELAY_MS);
}

/*
 * @brief Blinky timer callback.
 *
 * @param[in] tmr_id Timer ID.
 * @param[in] user_data User callback data.
 *
 * @return TMR_CB_NONE
 */
enum tmr_cb_action tmr_cb(int32_t tmr_id, uint32_t user_data)
{
    enum states next_state;
    bool led_on;
    uint32_t next_timer_value;

    switch (state.state) {
        case STATE_OFF:
            log_error("blinky unexpected tmr_cb in off state\n");
            return TMR_CB_NONE;

        case STATE_SEPARATOR_ON:
            if (++state.blink_counter < state.cfg.sep_num_blinks) {
                next_state = STATE_SEPARATOR_OFF;
                led_on = false;
                next_timer_value = (state.cfg.sep_period_ms+1)/2;
            } else {
                next_state = STATE_PRE_BLINK_DELAY;
                led_on = false;
                next_timer_value = PRE_BLINK_DELAY_MS;
            }
            break;

        case STATE_SEPARATOR_OFF:
            next_state = STATE_SEPARATOR_ON;
            led_on = true;
            next_timer_value = (state.cfg.sep_period_ms+1)/2;
            break;

        case STATE_PRE_BLINK_DELAY:
            if (state.cfg.code_period_ms > 0 && state.cfg.code_num_blinks > 0) {
                next_state = STATE_BLINK_ON;
                led_on = true;
                next_timer_value = (state.cfg.code_period_ms+1)/2;
                state.blink_counter = 0;
            } else {
                next_state = STATE_POST_BLINK_DELAY;
                led_on = false;
                next_timer_value = POST_BLINK_DELAY_MS;
            }
            break;

        case STATE_BLINK_ON:
            if (++state.blink_counter < state.cfg.code_num_blinks) {
                next_state = STATE_BLINK_OFF;
                led_on = false;
                next_timer_value = (state.cfg.code_period_ms+1)/2;
            } else {
                next_state = STATE_POST_BLINK_DELAY;
                led_on = false;
                next_timer_value = POST_BLINK_DELAY_MS;
            }
            break;

        case STATE_BLINK_OFF:
            next_state = STATE_BLINK_ON;
            led_on = true;
            next_timer_value = (state.cfg.code_period_ms+1)/2;
            break;

        case STATE_POST_BLINK_DELAY:
            if (state.cfg.sep_period_ms > 0 && state.cfg.sep_num_blinks > 0) {
                next_state = STATE_SEPARATOR_ON;
                led_on = true;
                next_timer_value = (state.cfg.sep_period_ms+1)/2;
                state.blink_counter = 0;
            } else {
                next_state = STATE_PRE_BLINK_DELAY;
                led_on = false;
                next_timer_value = PRE_BLINK_DELAY_MS;
            }
            break;

        default:
            log_error("blinky unexpected state %d\n", state.state);
            next_state = STATE_OFF;
            return TMR_CB_NONE;
    }

    state.state = next_state;
    dio_set(state.cfg.dout_idx, led_on);
    tmr_inst_start(state.tmr_id, next_timer_value);
    return TMR_CB_NONE;
}

