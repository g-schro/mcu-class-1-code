/*
 * @brief Main application file
 *
 * This file is the main application file that initializes and starts the various
 * modules and then runs the super loop.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "blinky.h"
#include "cmd.h"
#include "console.h"
#include "dio.h"
#include "gps_gtu7.h"
#include "log.h"
#include "mem.h"
#include "module.h"
#include "ttys.h"
#include "stat.h"
#include "tmr.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

enum main_u16_pms {
    CNT_INIT_ERR,
    CNT_START_ERR,
    CNT_RUN_ERR,

    NUM_U16_PMS
};

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static int32_t cmd_main_status();

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static int32_t log_level = LOG_DEFAULT;

static struct cmd_cmd_info cmds[] = {
    {
        .name = "status",
        .func = cmd_main_status,
        .help = "Get main status, usage: main status [clear]",
    },
};

static uint16_t cnts_u16[NUM_U16_PMS];

static const char* cnts_u16_names[NUM_U16_PMS] = {
    "init err",
    "start err",
    "run err",
};

static struct cmd_client_info cmd_info = {
    .name = "main",
    .num_cmds = ARRAY_SIZE(cmds),
    .cmds = cmds,
    .log_level_ptr = &log_level,
    .num_u16_pms = NUM_U16_PMS,
    .u16_pms = cnts_u16,
    .u16_pm_names = cnts_u16_names,
};


// Config info for dio module. These variables must be static since the dio
// module holds a pointer to them.

enum dout_index {
    DIN_BUTTON_1,
    DIN_GPS_PPS,

    DIN_NUM
};

static struct dio_in_info d_inputs[DIN_NUM] = {
    {
        // Button 1
        .name = "Button_1",
        .port = DIO_PORT_C,
        .pin = DIO_PIN_13,
        .pull = DIO_PULL_NO,
        .invert = 1,
    },
    {
        // GPS PPS, connected to PB2 (CN10, pin 22).
        .name = "PPS",
        .port = DIO_PORT_A,
        .pin = DIO_PIN_8,
        .pull = DIO_PULL_NO,
    }
};

enum din_index {
    DOUT_LED_2,

    DOUT_NUM
};

static struct dio_out_info d_outputs[DOUT_NUM] = {
    {
        // LED 2
        .name = "LED_2",
        .port = DIO_PORT_A,
        .pin = DIO_PIN_5,
        .pull = DIO_PULL_NO,
        .init_value = 0,
        .speed = DIO_SPEED_FREQ_LOW,
        .output_type = DIO_OUTPUT_PUSHPULL,
    }
};

static struct dio_cfg dio_cfg = {
    .num_inputs = ARRAY_SIZE(d_inputs),
    .inputs = d_inputs,
    .num_outputs = ARRAY_SIZE(d_outputs),
    .outputs = d_outputs,
};

static struct stat_dur stat_loop_dur;

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

void app_main(void)
{
    int32_t result;;
    struct console_cfg console_cfg;
    struct gps_cfg gps_cfg;
    struct ttys_cfg ttys_cfg;
    struct blinky_cfg blinky_cfg = {
        .dout_idx = DOUT_LED_2,
        .code_num_blinks = 5,
        .code_period_ms = 1000,
        .sep_num_blinks = 5,
        .sep_period_ms = 200,
    };

    //
    // Invoke the init API on modules the use it.
    //

    setvbuf(stdout, NULL, _IONBF, 0);
    // Before ttys is init-ed we have to put in \r explicitly.
    printf("\n\rInit: Init modules\n\r");
    result = ttys_get_def_cfg(TTYS_INSTANCE_UART2, &ttys_cfg);
    if (result < 0) {
        log_error("ttys_get_def_cfg error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
    } else {
        result = ttys_init(TTYS_INSTANCE_UART2, &ttys_cfg);
        if (result < 0) {
            log_error("ttys_init UART2 error %d\n", result);
            INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
        }
    }

    result = ttys_get_def_cfg(TTYS_INSTANCE_UART6, &ttys_cfg);
    if (result < 0) {
        log_error("ttys_get_def_cfg error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
    } else {
        result = ttys_init(TTYS_INSTANCE_UART6, &ttys_cfg);
        if (result < 0) {
            log_error("ttys_init UART6 error %d\n", result);
            INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
        }
    }

    result = cmd_init(NULL);
    if (result < 0) {
        log_error("cmd_init error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
    }

    result = console_get_def_cfg(&console_cfg);
    if (result < 0) {
        log_error("console_get_def_cfg error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
    } else {
        result = console_init(&console_cfg);
        if (result < 0) {
            log_error("console_init error %d\n", result);
            INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
        }
    }

    result = tmr_init(NULL);
    if (result < 0) {
        log_error("tmr_init error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
    }

    result = dio_init(&dio_cfg);
    if (result < 0) {
        log_error("dio_init error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
    }

    result = gps_get_def_cfg(&gps_cfg);
    if (result < 0) {
        log_error("gps_get_def_cfg error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
    } else {
        result = gps_init(&gps_cfg);
        if (result < 0) {
            log_error("gps_init error %d\n", result);
            INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
        }
    }
    
    result = blinky_init(&blinky_cfg);
    if (result < 0) {
        log_error("blinky_init error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_INIT_ERR]);
    }

    //
    // Invoke the start API on modules the use it.
    //

    printf("Init: Start modules\n");

    result = ttys_start(TTYS_INSTANCE_UART2);
    if (result < 0) {
        log_error("ttys_start UART2 error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_START_ERR]);
    }

    result = ttys_start(TTYS_INSTANCE_UART6);
    if (result < 0) {
        log_error("ttys_start UART6 error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_START_ERR]);
    }

    result = tmr_start();
    if (result < 0) {
        log_error("tmr_start error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_START_ERR]);
    }

    result = dio_start();
    if (result < 0) {
        log_error("dio_start error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_START_ERR]);
    }

    result = gps_start();
    if (result < 0) {
        log_error("gps_start error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_START_ERR]);
    }

    result = mem_start();
    if (result < 0) {
        log_error("mem_start error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_START_ERR]);
    }

    result = blinky_start();
    if (result < 0) {
        log_error("blinky_start error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_START_ERR]);
    }

    result = cmd_register(&cmd_info);
    if (result < 0) {
        log_error("main: cmd_register error %d\n", result);
        INC_SAT_U16(cnts_u16[CNT_START_ERR]);
    }

    stat_dur_init(&stat_loop_dur);

    //
    // In the super loop invoke the run API on modules the use it.
    //

    printf("Init: Enter super loop\n");

    while (1)
    {
        stat_dur_restart(&stat_loop_dur);

        result = console_run();
        if (result < 0)
            INC_SAT_U16(cnts_u16[CNT_RUN_ERR]);

        result = gps_run();
        if (result < 0)
            INC_SAT_U16(cnts_u16[CNT_RUN_ERR]);

        result = tmr_run();
        if (result < 0)
            INC_SAT_U16(cnts_u16[CNT_RUN_ERR]);

    }
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Console command function for "main status".
 *
 * @param[in] argc Number of arguments, including "main"
 * @param[in] argv Argument values, including "main"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: main status [clear]
 */
static int32_t cmd_main_status(int32_t argc, const char** argv)
{
    bool clear = false;
    bool bad_arg = false;

    if (argc == 3) {
        if (strcasecmp(argv[2], "clear") == 0)
            clear = true;
        else
            bad_arg = true;
    } else if (argc > 3) {
        bad_arg = true;
    }

    if (bad_arg) {
        printf("Invalid arguments\n");
        return MOD_ERR_ARG;
    }

    printf("Super loop samples=%lu min=%lu ms, max=%lu ms, avg=%lu us\n",
           stat_loop_dur.samples, stat_loop_dur.min, stat_loop_dur.max,
           stat_dur_avg_us(&stat_loop_dur));

    if (clear) {
        printf("Clearing loop stat\n");
        stat_dur_init(&stat_loop_dur);
    }
    return 0;
}
