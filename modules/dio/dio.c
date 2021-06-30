/*
 * @brief Implementation of dio module.
 *
 * This module provides access to discrete inputs and outputs, sometimes
 * referred to as digital inputs and outputs.
 *
 * During configuration, the user must specify the set of inputs and outputs and
 * their characteristics.
 *
 * The following console commands are provided:
 * > dio status
 * > dio get
 * > dio set
 * See code for details.
 *
 * Currently, defintions from the STMicroelectronics Low Level (LL) device
 * library are used for some configuration paramters. A future enhancment would
 * be to define all configuration parameters in this module, so that user code
 * is more portable.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "dio.h"
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

static int32_t cmd_dio_status(int32_t argc, const char** argv);
static int32_t cmd_dio_get(int32_t argc, const char** argv);
static int32_t cmd_dio_set(int32_t argc, const char** argv);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static struct dio_cfg* cfg;

static struct cmd_cmd_info cmds[] = {
    {
        .name = "status",
        .func = cmd_dio_status,
        .help = "Get module status, usage: dio status",
    },
    {
        .name = "get",
        .func = cmd_dio_get,
        .help = "Get input value, usage: dio get <input-name>",
    },
    {
        .name = "set",
        .func = cmd_dio_set,
        .help = "Set output value, usage: dio set <output-name> {0|1}",
    },
};

static int32_t log_level = LOG_DEFAULT;

static struct cmd_client_info cmd_info = {
    .name = "dio",
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
 * @brief Initialize dio module instance.
 *
 * @param[in] cfg The dio configuration.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function initializes the dio singleton module. Generally, it should not
 * access other modules as they might not have been initialized yet. An
 * exception is the log module.
 */
int32_t dio_init(struct dio_cfg* _cfg)
{
    uint32_t idx;
    const struct dio_in_info* dii;
    const struct dio_out_info* doi;

    cfg = _cfg;

    for (idx = 0; idx < cfg->num_inputs; idx++) {
        dii = &cfg->inputs[idx];
        LL_GPIO_SetPinPull(dii->port, dii->pin, dii->pull);
        LL_GPIO_SetPinMode(dii->port, dii->pin, LL_GPIO_MODE_INPUT);
    }
    for (idx = 0; idx < cfg->num_outputs; idx++) {
        doi = &cfg->outputs[idx];
        LL_GPIO_SetPinSpeed(doi->port, doi->pin, doi->speed);
        LL_GPIO_SetPinOutputType(doi->port, doi->pin,  doi->output_type);
        LL_GPIO_SetPinPull(doi->port, doi->pin, doi->pull);
        LL_GPIO_SetPinMode(doi->port, doi->pin, LL_GPIO_MODE_OUTPUT);
    }
    return 0;
}

/*
 * @brief Start dio module instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts the dio singleton module, to enter normal operation.
 */
int32_t dio_start(void)
{
    int32_t result;

    result = cmd_register(&cmd_info);
    if (result < 0) {
        log_error("dio_start: cmd error %d\n", result);
        return MOD_ERR_RESOURCE;
    }
    return 0;
}

/*
 * @brief Get value of discrete input.
 *
 * @param[in] din_idx Discrete input index per module configuration.
 *
 * @return Input state (0/1), else a "MOD_ERR" value (< 0). See code for
 *         details.
 */
int32_t dio_get(uint32_t din_idx)
{
    if (din_idx >= cfg->num_inputs)
        return MOD_ERR_ARG;
    return LL_GPIO_IsInputPinSet(cfg->inputs[din_idx].port,
                                 cfg->inputs[din_idx].pin) ^
        cfg->inputs[din_idx].invert;
}

/*
 * @brief Get value of discrete output.
 *
 * @param[in] dout_idx Discrete output index per module configuration.
 *
 * @return Output state (0/1), else a "MOD_ERR" value (< 0). See code for
 *         details.
 */
int32_t dio_get_out(uint32_t dout_idx)
{
    if (dout_idx >= cfg->num_outputs)
        return MOD_ERR_ARG;

    return LL_GPIO_IsOutputPinSet(cfg->outputs[dout_idx].port,
                                  cfg->outputs[dout_idx].pin) ^
        cfg->outputs[dout_idx].invert;
}

/*
 * @brief Set value of discrete output.
 *
 * @param[in] dout_idx Discrete output index per module configuration.
 * @param[in] value Output value 0/1.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t dio_set(uint32_t dout_idx, uint32_t value)
{
    if (dout_idx >= cfg->num_outputs)
        return MOD_ERR_ARG;
    if (value ^ cfg->outputs[dout_idx].invert) {
        LL_GPIO_SetOutputPin(cfg->outputs[dout_idx].port,
                             cfg->outputs[dout_idx].pin);
    } else {
        LL_GPIO_ResetOutputPin(cfg->outputs[dout_idx].port,
                               cfg->outputs[dout_idx].pin);
    }
    return 0;
}

/*
 * @brief Get number of discrete inputs.
 *
 * @return Return number of inputs (non-negative) for success, else a "MOD_ERR"
 *         value. See code for details.
 */
int32_t dio_get_num_in(void)
{
    return cfg == NULL ? MOD_ERR_RESOURCE : cfg->num_inputs;
}

/*
 * @brief Get number of discrete output.
 *
 * @return Return number of outputs (non-negative) for success, else a "MOD_ERR"
 *         value. See code for details.
 */
int32_t dio_get_num_out(void)
{
    return cfg == NULL ? MOD_ERR_RESOURCE : cfg->num_outputs;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Console command function for "dio status".
 *
 * @param[in] argc Number of arguments, including "dio".
 * @param[in] argv Argument values, including "dio".
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: dio status
 */
static int32_t cmd_dio_status(int32_t argc, const char** argv)
{
    uint32_t idx;
    
    printf("Inputs:\n");
    for (idx = 0; idx < cfg->num_inputs; idx++)
        printf("  %2lu: %s = %ld\n", idx, cfg->inputs[idx].name, dio_get(idx));
    

    printf("Outputs:\n");
    for (idx = 0; idx < cfg->num_outputs; idx++)
        printf("  %2lu: %s = %ld\n", idx, cfg->outputs[idx].name,
               dio_get_out(idx));

    return 0;
}

/*
 * @brief Console command function for "dio get".
 *
 * @param[in] argc Number of arguments, including "dio".
 * @param[in] argv Argument values, including "dio".
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: dio get <input-name>
 */
static int32_t cmd_dio_get(int32_t argc, const char** argv)
{
    uint32_t idx;
    struct cmd_arg_val arg_vals[1];

    if (cmd_parse_args(argc-2, argv+2, "s", arg_vals) != 1)
        return MOD_ERR_BAD_CMD;

    for (idx = 0; idx < cfg->num_inputs; idx++)
        if (strcasecmp(arg_vals[0].val.s, cfg->inputs[idx].name) == 0)
            break;
    if (idx < cfg->num_inputs) {
        printf("%s = %ld\n", cfg->inputs[idx].name, dio_get(idx));
        return 0;
    }

    for (idx = 0; idx < cfg->num_outputs; idx++)
        if (strcasecmp(arg_vals[0].val.s, cfg->outputs[idx].name) == 0)
            break;
    if (idx < cfg->num_outputs) {
        printf("%s %ld\n", cfg->outputs[idx].name, dio_get_out(idx));
        return 0;
    }
    printf("Invalid dio input/output name '%s'\n", arg_vals[0].val.s);
    return MOD_ERR_ARG;
}

/*
 * @brief Console command function for "dio set".
 *
 * @param[in] argc Number of arguments, including "dio".
 * @param[in] argv Argument values, including "dio".
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: dio set <output-name> {0|1}
 */
static int32_t cmd_dio_set(int32_t argc, const char** argv)
{
    uint32_t idx;
    struct cmd_arg_val arg_vals[2];
    uint32_t value;

    if (cmd_parse_args(argc-2, argv+2, "su", arg_vals) != 2)
        return MOD_ERR_BAD_CMD;

    for (idx = 0; idx < cfg->num_outputs; idx++)
        if (strcasecmp(arg_vals[0].val.s, cfg->outputs[idx].name) == 0)
            break;
    if (idx >= cfg->num_outputs) {
        printf("Invalid dio name '%s'\n", arg_vals[0].val.s);
        return MOD_ERR_ARG;
    }

    value = arg_vals[1].val.u;
    if (value != 0 && value != 1) {
        printf("Invalid value '%s'\n", argv[3]);
        return MOD_ERR_ARG;
    }
    return dio_set(idx, value);
}
