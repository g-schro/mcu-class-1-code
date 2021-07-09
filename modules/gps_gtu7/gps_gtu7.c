/*
 * @brief Implementation of gps_gtu7 module.
 *
 * This module provides a very basic interface for playing with a GPS GTU7
 * hardware module. This hardware module contains a GPS receiver with an ASCII
 * serial interface, and a pulse-per-second (PPS) discrete output.
 *
 * This module provides:
 * - Parsing of the GGPS GTU7 output messages that contain satellite positions.
 * - Map satellite positions to a 2-D grid, and plot it to the console, using
 *   ANSI escape sequences so the satellite map stays at a fixed position.
 *
 * The following console commands are provided:
 * > gps status
 * > gps map
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
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "cmd.h"
#include "gps_gtu7.h"
#include "log.h"
#include "module.h"
#include "tmr.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

#define GPS_IN_BFR_SIZE 80
#define MAX_SATS 32
#define CLEANUP_TMR_MS 5000

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

struct sat_data {
    uint32_t last_update_ms;  // Used to detect disappering satellites.
    uint16_t azimuth;   // 0-359 degress
    uint8_t present;    // 1 if satellite being reported.
    uint8_t elevation;  // 0-90 maximum
    uint8_t snr;        // 0-99 dB
};

struct gps_state {
    enum ttys_instance_id ttys_instance_id;
    char in_bfr[GPS_IN_BFR_SIZE];
    uint16_t in_bfr_chars;
    struct sat_data sat_data[MAX_SATS];
    bool disp_map_on;
    bool disp_map_clear_screen;
    bool disp_map_update;
    bool disp_map_clear_history;
    int32_t cleanup_tmr_id;
};

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static int32_t cmd_gps_status(int32_t argc, const char** argv);
static int32_t cmd_gps_map(int32_t argc, const char** argv);

static void process_msg(char* msg);
static char sat_idx_to_char(int32_t sat_idx);
static char* csv_get_token(char* start, char** next_start);
static void display_map(void);
static enum tmr_cb_action cleanup_tmr_cb(int32_t tmr_id, uint32_t user_data);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static struct gps_state gps_state;

static struct cmd_cmd_info cmds[] = {
    {
        .name = "status",
        .func = cmd_gps_status,
        .help = "Get module status, usage: gps status",
    },
    {
        .name = "map",
        .func = cmd_gps_map,
        .help = "Map display on/off/clear, usage: gps map {on|off|clear}",
    }
};

static int32_t log_level = LOG_DEFAULT;

static struct cmd_client_info cmd_info = {
    .name = "gps",
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
 * @brief Get default gps configuration.
 *
 * @param[out] cfg The gps configuration with defaults filled in.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t gps_get_def_cfg(struct gps_cfg* cfg)
{
    if (cfg == NULL)
        return MOD_ERR_ARG;

    memset(cfg, 0, sizeof(*cfg));
    cfg->ttys_instance_id = TTYS_INSTANCE_UART3;
    return 0;
}

/*
 * @brief Initialize gps module instance.
 *
 * @param[in] cfg The gps module configuration.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function initializes a gps singleton module. Generally, it should not
 * access other modules as they might not have been initialized yet.
 */
int32_t gps_init(struct gps_cfg* cfg)
{
    if (cfg == NULL) {
        return MOD_ERR_ARG;
    }
    memset(&gps_state, 0, sizeof(gps_state));
    gps_state.ttys_instance_id = cfg->ttys_instance_id;
    gps_state.disp_map_clear_history = true;
    return 0;
}

/*
 * @brief Start gps module instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts a gps module instance, to enter normal operation.
 */
int32_t gps_start(void)
{
    int32_t result;

    cmd_register(&cmd_info);
    result = cmd_register(&cmd_info);
    if (result < 0) {
        log_error("gps_start: cmd error %d\n", result);
        return MOD_ERR_RESOURCE;
    }
    gps_state.cleanup_tmr_id = tmr_inst_get_cb(CLEANUP_TMR_MS,
                                               cleanup_tmr_cb,
                                               0);
    if (gps_state.cleanup_tmr_id < 0) {
        log_error("gps_start: tmr error %d\n", gps_state.cleanup_tmr_id);
        return MOD_ERR_RESOURCE;
    }

    return 0;
}

/*
 * @brief Run gps module instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * @note This function should not block.
 */
int32_t gps_run(void)
{
    char c;
    while (ttys_getc(gps_state.ttys_instance_id, &c)) {
        if (c == '\n' || c == '\r') {
            if (gps_state.in_bfr_chars > 0) {
                gps_state.in_bfr[gps_state.in_bfr_chars] = '\0';
                process_msg(gps_state.in_bfr);
                gps_state.in_bfr_chars = 0;
            }
            continue;
        }
        if (isprint(c)) {
            if (gps_state.in_bfr_chars < (GPS_IN_BFR_SIZE-1)) {
                gps_state.in_bfr[gps_state.in_bfr_chars++] = c;
            } else {
                gps_state.in_bfr[GPS_IN_BFR_SIZE-1] = '\0';
                log_debug("Truncated: %s\n", gps_state.in_bfr);
                gps_state.in_bfr_chars = 0;
            }
            continue;
        }
    }
    if (gps_state.disp_map_on && gps_state.disp_map_update) {
        display_map();
        gps_state.disp_map_update = false;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Console command function for "gps status".
 *
 * @param[in] argc Number of arguments, including "gps"
 * @param[in] argv Argument values, including "gps"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: gps status
 */
static int32_t cmd_gps_status(int32_t argc, const char** argv)
{
    int32_t idx;

    printf("Reported satellites:\n");
    for (idx = 0; idx < MAX_SATS; idx++) {
        struct sat_data* sat_data = &gps_state.sat_data[idx];
        if (sat_data->present) {
            printf("  %c: azimuth=%3d deg elevation=%2d deg snr=%2d dB "
                   "data-age=%lu ms\n",
                   sat_idx_to_char(idx),
                   sat_data->azimuth,
                   sat_data->elevation,
                   sat_data->snr,
                   tmr_get_ms() - sat_data->last_update_ms);
        }
    }
    printf("gps map: %s\n", gps_state.disp_map_on ? "on" : "off");
    return 0;
}

/*
 * @brief Console command function for "gps map".
 *
 * @param[in] argc Number of arguments, including "gps"
 * @param[in] argv Argument values, including "gps"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: gps map {on|off|clear}
 */
static int32_t cmd_gps_map(int32_t argc, const char** argv)
{
    struct cmd_arg_val arg_vals[1];
    const char* op;

    if (cmd_parse_args(argc-2, argv+2, "s", arg_vals) != 1)
        return MOD_ERR_BAD_CMD;
    op = arg_vals[0].val.s;
    if (strcasecmp(op, "on") == 0) {
        gps_state.disp_map_on = true;
        gps_state.disp_map_clear_screen = true;
    } else if (strcasecmp(op, "off") == 0) {
        gps_state.disp_map_on = false;
    } else if (strcasecmp(op, "clear") == 0) {
        gps_state.disp_map_clear_history = false;
    } else {
        printf("Invalid operation '%s'\n", op);
        return MOD_ERR_ARG;
    }
    return 0;
}

/*
 * @brief Process a message received from the GPS hardware module.
 *
 * @param[in] msg GPS hardware module message.
 */
static void process_msg(char* msg)
{
    enum parse_state {
        PARSE_STATE_START,
        PARSE_STATE_GPGSV,
        PARSE_STATE_IGNORE,
    };

    char* next = msg;
    enum parse_state parse_state = PARSE_STATE_START;
    int32_t field_num = 0;
    int32_t satellite_num = -1;
    uint16_t msg_azimuth;
    uint8_t msg_elevation;
    uint8_t msg_snr;
    struct sat_data* sat_data;

    log_trace("Msg: %s\n", msg);
    while (1) {
        if (next == NULL)
            break;
        char* token = csv_get_token(next, &next);
        field_num++;
        switch (parse_state) {
            case PARSE_STATE_START:
                parse_state = strcasecmp(token, "$GPGSV") == 0 ?
                    PARSE_STATE_GPGSV : PARSE_STATE_IGNORE;
                break;
            case PARSE_STATE_GPGSV:
                switch ((field_num - 5) % 4) {
                    case 0:
                        // Satellite PRN number
                        satellite_num = atoi(token);
                        if (satellite_num < 1 || satellite_num > 32) {
                            log_debug("Unused satellite, number=%s\n", token);
                            parse_state = PARSE_STATE_IGNORE;
                        } else {
                            // Make satellite number zero-based.
                            satellite_num--;
                        }
                        break;
                    case 1:
                        // Elevation degress (0-90)
                        msg_elevation = atoi(token);
                        break;
                    case 2:
                        // Azimuth degress (000-359)
                        msg_azimuth = atoi(token);
                        break;
                    case 3:
                        // SNR (00-99)
                        msg_snr = atoi(token);
                        sat_data = &gps_state.sat_data[satellite_num];
                        if ((!sat_data->present) ||
                            (msg_elevation != sat_data->elevation) ||
                            (msg_azimuth != sat_data->azimuth)) {
                            log_debug("Update sat %d ele=%d az=%d snr=%d\n",
                                      satellite_num+1, msg_elevation,
                                      msg_azimuth, msg_snr);
                            sat_data->present = true;
                            sat_data->elevation = msg_elevation;
                            sat_data->azimuth = msg_azimuth;
                            gps_state.disp_map_update = true;
                        }
                        sat_data->snr = msg_snr;
                        sat_data->last_update_ms = tmr_get_ms();
                        break;
                }
                break;
            case PARSE_STATE_IGNORE:
                continue;
        }
        if (next == NULL)
            break;
    }
}

/*
 * @brief Convert statellite index of the display char.
 *
 * @param[in] sat_idx Satellite index (zero-based).
 *
 * @return The satellite dislay char.
 */
static char sat_idx_to_char(int32_t sat_idx)
{
    if (sat_idx < 9)
        return '1' + sat_idx;
    return 'A' + (sat_idx - 9);
}

/*
 * @brief Tokenize a CSV string in-place, incrementally.
 *
 * @param[in] start The current point in the string, typically pointing to the
 *            next token.
 * @param[out] next_start The start of the next token. This should be passed in
 *             as "start" in the next call. If there are no more tokens, it will
 *             be set to NULL.
 *
 * @return The next token to process. The token might have zero length (e.g.
 *         in the case of consecutive commas).
 *
 * @note The char '*' is also treated as a separator (i.e. like a ',').
 */
static char* csv_get_token(char* start, char** next_start)
{
    char* p = start;
    while (*p && *p != ',' && *p != '*') p++;
    if (*p == ',' || *p == '*') {
        *p++ = '\0';
        *next_start = p;
    } else {
        *next_start = NULL;
    }
    return start;
}

/*
 * @brief Display the satellite positions as a map.
 */
static void display_map(void)
{
    #define DISP_MAX_RAD 10
    #define DISP_ROWS (1 + 2*DISP_MAX_RAD)
    #define DISP_COLS (1 + 2*DISP_MAX_RAD)
    #define DISP_X_OFFSET DISP_MAX_RAD
    #define DISP_Y_OFFSET DISP_MAX_RAD
    #define DEG_TO_RAD(d) ((double)(d) * M_PI / 180.0)

    static char map[DISP_COLS][DISP_ROWS];
    int32_t idx;
    int32_t idy;

    if (gps_state.disp_map_clear_history) {
        memset(map, '.', sizeof(map));
        gps_state.disp_map_clear_history = false;
    }
    for (idx = 0; idx < MAX_SATS; idx++) {
        struct sat_data* sat_data = &gps_state.sat_data[idx];
        if (sat_data->present && sat_data->elevation <= 90) {
            double r = cos(DEG_TO_RAD(sat_data->elevation)) * (double)(DISP_MAX_RAD);
            double theta = DEG_TO_RAD(90 - sat_data->azimuth);
            int32_t x = (int32_t)round(cos(theta) * r) + DISP_X_OFFSET;
            int32_t y = (int32_t)round(sin(theta) * r) + DISP_Y_OFFSET;
            x = CLAMP(x, 0, DISP_COLS-1);
            y = CLAMP(y, 0, DISP_ROWS-1);
            map[x][y] = sat_idx_to_char(idx);
            log_debug("%c %c az=%3d el=%3d r*1000=%5d x=%3d y=%3d\n",
                      sat_idx_to_char(idx),
                      map[x][y],
                      sat_data->azimuth,
                      sat_data->elevation,
                      (int32_t)round(r*1000.0),
                      x,
                      y);
        }
    }

    printf("\x1B[?25l");
    if (gps_state.disp_map_clear_screen) {
        gps_state.disp_map_clear_screen = false;
        printf("\x1B[2J");
    }
    printf("\x1B[1;1H");
    for (idy = DISP_ROWS-1; idy >= 0; idy--) {
        for (idx = 0; idx < DISP_COLS; idx++)
            printf("%c ", map[idx][idy]);
        printf("\n");
    }
    printf("\x1B[?25h");
}

/*
 * @brief Timer callback to remove satellites that have disappeared.
 *
 * @param[in] tmr_id The timer ID (not used).
 * @param[in] user_data User data for the timer (not used).
 *
 * If a satellite has not been reported for some amount of time, it is removed.
 */
static enum tmr_cb_action cleanup_tmr_cb(int32_t tmr_id, uint32_t user_data)
{
    uint32_t now_ms = tmr_get_ms();
    uint32_t idx;

    log_debug("In cleanup_tmr_cb()\n");
    for (idx = 0; idx < MAX_SATS; idx++) {
        struct sat_data* sat_data = &gps_state.sat_data[idx];
        if (sat_data->present &&
            ((now_ms - sat_data->last_update_ms) >
             CLEANUP_TMR_MS)) {
            log_debug("Clean up satellite %d\n", idx+1);
            sat_data->present = false;
            gps_state.disp_map_update = true;
        }
    }
    return TMR_CB_RESTART;
}
