/*
 * @brief Implementation of cmd module.
 *
 * This module is a utility used by clients (e.g. other modules) to provide
 * console commands. It works like this:
 * - Client register a set of console commands during startup.
 * - When the console module receives a command line string, it passes it to
 *   this (cmd) module which parses the string, and handles it directly, or
 *   calls a client command handler function.
 * - This module also provides some common commands automatically, on behalf
 *   of the clients, as described below.
 *
 * Generally, a command line string consists of a client name, then the command
 * name, and then optional command arguments.
 *
 * For example, say the tmr module has two commands, "status" and "test". The
 * tmr module would register these commands, with this (cmd) module. It would
 * specify a client name of "tmr". It would provide information for the commands
 * "status" and "test". Then, to invoke these commands, the console user would
 * enter commands like this.
 *
 * > tmr status
 * > tmr test get 0
 *
 * This module would parse the command line, and call the appropriate command
 * handler function specified by the tmr client (as part of registration).
 *
 * All command line tokens (i.e. the client name, the command name, and any
 * arguments) are passed to the command handler using the (int32_t argc, char**
 * argv) pattern. It is up to the command handler function to validate the
 * arguments.
 *
 * The cmd module provides several commands automatically on behalf of the
 * clients:
 * - A help command. For example, if the console user enters "tmr help" then a
 *   list of tmr commands is printed.
 * - A command to get and set the client's logging level, if the client provided
 *   access to its logging level variable (as part of registration). For
 *   example, the console user could enter "tmr log" to get the current log
 *   level, or "tmr log debug" or "tmr log off" to set the log level.
 * - A command to get or clear the client's performance measurements (typically
 *   counters), if the client provided access to these measurements (as part of
 *   registration). For example, the console user could enter "ttys pm" to get
 *   the current performance measurement values, or "ttys pm clear" to clear
 *   them.
 *
 * The cmd module provides a global "help" command to list the commands of all
 * clients. The token "?" can be used in place of help.
 *
 * The cmd module provides "wild card" commands which are executed for all
 * clients. In this case, the first token in the command line is "*" rather than
 * a client name. The following wild card commands are supported:
 *
 * > * log
 * > * log <new-level>
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
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "log.h"
#include "module.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

#define MAX_CMD_TOKENS 10

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static const char* log_level_str(int32_t level);
static int32_t log_level_int(const char* level_name);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

// Client info.
static const struct cmd_client_info* client_info[CMD_MAX_CLIENTS];

static int32_t log_level = LOG_DEFAULT; 

static const char* log_level_names[] = {
    LOG_LEVEL_NAMES_CSV
};

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Initialize cmd instance.
 *
 * @param[in] cfg The cmd configuration. (FUTURE)
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function initializes the cmd singleton module. Generally, it should not
 * access other modules as they might not have been initialized yet. An
 * exception is the log module.
 */
int32_t cmd_init(struct cmd_cfg* cfg)
{
    memset(client_info, 0, sizeof(client_info));
    return 0;
}

/*
 * @brief Register a client.
 *
 * @param[in] _client_info The client's configuration (eg. command metadata)
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * @note This function keeps a copy of the _client_info pointer.
 */
int32_t cmd_register(const struct cmd_client_info* _client_info)
{
    int32_t idx;

    for (idx = 0; idx < CMD_MAX_CLIENTS; idx++) {
        if (client_info[idx] == NULL ||
            strcasecmp(client_info[idx]->name, _client_info->name) == 0) {
            client_info[idx] = _client_info;
            return 0;
        }
    }
    return MOD_ERR_RESOURCE;
}

/*
 * @brief Execute a command line.
 *
 * @param[in] bfr The command line.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function parses the command line and then executes the command,
 * typically by running a command function handler for a client.
 */
int32_t cmd_execute(char* bfr)
{
    int32_t num_tokens = 0;
    const char* tokens[MAX_CMD_TOKENS];
    char* p = bfr;
    int32_t idx;
    int32_t idx2;
    const struct cmd_client_info* ci;
    const struct cmd_cmd_info* cci;

    // Tokenize the command line in-place.
    while (1) {
        // Find start of token.
        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p == '\0') {
            // Found end of line.
            break;
        } else {
            if (num_tokens >= MAX_CMD_TOKENS) {
                printf("Too many tokens\n");
                return MOD_ERR_BAD_CMD;
            }
            // Record pointer to token and find its end.
            tokens[num_tokens++] = p;
            while (*p && !isspace((unsigned char)*p))
                p++;
            if (*p) {
                // Terminate token.
                *p++ = '\0';
            } else {
                // Found end of line.
                break;
            }
        }
    }

    // If there are no tokens, nothing to do.
    if (num_tokens == 0)
        return 0;

    // Handle wild card commands
    if (strcmp("*", tokens[0]) == 0) {
        if (num_tokens < 2) {
            printf("Wildcard missing command\n");
            return MOD_ERR_BAD_CMD;
        } 
        if (strcasecmp(tokens[1], "log") == 0) {
            int32_t log_level;
            if (num_tokens == 3) {
                log_level = log_level_int(tokens[2]);
                if (log_level < 0) {
                    printf("Invalid log level: %s\n", tokens[2]);
                    return MOD_ERR_ARG;
                }
            } else if (num_tokens > 3) {
                printf("Invalid arguments\n");
                return MOD_ERR_ARG;
            }
            for (idx = 0;
                 idx < CMD_MAX_CLIENTS && client_info[idx] != NULL;
                 idx++) {
                ci = client_info[idx];
                if (ci->log_level_ptr != NULL) {
                    if (num_tokens == 3) {
                        *ci->log_level_ptr = log_level;
                    } else {
                        printf("Log level for %s = %s\n", ci->name,
                               log_level_str(*ci->log_level_ptr));
                    }
                }
            }
        }
        return 0;
    }
    // Handle top-level help.
    if (strcasecmp("help", tokens[0]) == 0 ||
        strcasecmp("?", tokens[0]) == 0) {
        for (idx = 0;
             idx < CMD_MAX_CLIENTS && client_info[idx] != NULL;
             idx++) {
            ci = client_info[idx];
            if (ci->num_cmds == 0)
                continue;
            printf("%s (", ci->name);
            for (idx2 = 0; idx2 < ci->num_cmds; idx2++) {
                cci = &ci->cmds[idx2];
                printf("%s%s", idx2 == 0 ? "" : ", ", cci->name);
            }
            // If client provided log level, include log command.
            if (ci->log_level_ptr)
                printf("%s%s", idx2 == 0 ? "" : ", ", "log");

            // If client provided pm info, include pm command.
            if (ci->num_u16_pms > 0)
                printf("%s%s", idx2 == 0 ? "" : ", ", "pm");

            printf(")\n");
        }
        printf("\nLog levels are: %s\n", LOG_LEVEL_NAMES);
        return 0;
    }

    // Find and execute the command.
        for (idx = 0;
             idx < CMD_MAX_CLIENTS && client_info[idx] != NULL;
             idx++) {
        ci = client_info[idx];
        if (strcasecmp(tokens[0], ci->name) != 0)
            continue;

        // If there is no command, create a dummy.
        if (num_tokens == 1)
            tokens[1] = "";

        // Handle help command directly.
        if (strcasecmp(tokens[1], "help") == 0 ||
            strcasecmp(tokens[1], "?") == 0) {
            log_debug("Handle client help\n");
            for (idx2 = 0; idx2 < ci->num_cmds; idx2++) {
                cci = &ci->cmds[idx2];
                printf("%s %s: %s\n", ci->name, cci->name, cci->help);
            }

            // If client provided log level, print help for log command.
            if (ci->log_level_ptr) {
                printf("%s log: set or get log level, args: [level]\n",
                       ci->name);
            }

            // If client provided pm info, print help for pm command.
            if (ci->num_u16_pms > 0)
                printf("%s pm: get or clear performance measurements, "
                       "args: [clear]\n", ci->name);

            if (ci->log_level_ptr)
                printf("\nLog levels are: %s\n", LOG_LEVEL_NAMES);

            return 0;
        }

        // Handle log command directly.
        if (strcasecmp(tokens[1], "log") == 0) {
            log_debug("Handle command log\n");
            if (ci->log_level_ptr) {
                if (num_tokens < 3) {
                    printf("Log level for %s = %s\n", ci->name,
                           log_level_str(*ci->log_level_ptr));
                } else {
                    int32_t log_level = log_level_int(tokens[2]);
                    if (log_level < 0) {
                        printf("Invalid log level: %s\n", tokens[2]);
                        return MOD_ERR_ARG;
                    }
                    *ci->log_level_ptr = log_level;
                }
            }
            return 0;
        }

        // Handle pm command directly.
        if (strcasecmp(tokens[1], "pm") == 0) {
            bool clear = ((num_tokens >= 3) &&
                          (strcasecmp(tokens[2], "clear") == 0));
            if (ci->num_u16_pms > 0) {
                if (clear)
                    printf("Clearing performance measurements for %s\n",
                           ci->name);
                else
                    printf("%s:\n", ci->name);
                for (idx2 = 0; idx2 < ci->num_u16_pms; idx2++) {
                    if (clear)
                        ci->u16_pms[idx2] = 0;
                    else
                        printf("  %s: %d\n", ci->u16_pm_names[idx2],
                               ci->u16_pms[idx2]);
                }
            }
            return 0;
        }

        // Find the command
        for (idx2 = 0; idx2 < ci->num_cmds; idx2++) {
            if (strcasecmp(tokens[1], ci->cmds[idx2].name) == 0) {
                log_debug("Handle command\n");
                ci->cmds[idx2].func(num_tokens, tokens);
                return 0;
            }
        }
        printf("No such command (%s %s)\n", tokens[0], tokens[1]);
        return MOD_ERR_BAD_CMD;
    }
    printf("No such command (%s)\n", tokens[0]);
    return MOD_ERR_BAD_CMD;
}

/*
 * @brief Parse and validate command arguments
 *
 * @param[in]  argc The number of arguments to be parsed.
 * @param[in]  argv Pointer string array of arguments to be be parsed.
 * @param[in]  fmt String indicating expected argument types
 * @param[out] arg_vals Pointer to array of parsed argument values
 *
 * @return Number of parsed arguments, negative value if error.
 *
 * @note In case of error, an error message is always printed to the
 * console.
 *
 * This function provides a common method to parse and validate command
 * arguments.  In particular, it does string to numeric conversions, with error
 * checking.
 *
 * A format string is used to guide the parsing. The format string contains a
 * letter for each expected argument. The supported letters are:
 * - i Integer value, in either decimal, octal, or hex formats. Octal values
 *     must start with 0, and hex values must start with 0x.
 * - u Unsigned value, in either decimal, octal, or hex formats. Octal values
 *     must start with 0, and hex values must start with 0x.
 * - p Pointer, in hex format. No leading 0x is necessary (but allowed).
 * - s String
 *
 * In addition:
 * - [ indicates that remaining arguments are optional. However,
 *   if one optional argument is present, then subsequent arguments
 *   are required.
 * - ] is ignored; it can be put into the fmt string for readability,
 *   to match brackets.
 *
 * Examples:
 *   "up" - Requires exactly one unsigned and one pointer argument.
 *   "ii" - Requires exactly two integer arguments.
 *   "i[i" - Requires either one or two integer arguments.
 *   "i[i]" - Same as above (matched brackets).
 *   "i[i[i" - Requires either one, two, or three integer arguments.
 *   "i[i[i]]" - Same as above (matched brackets).
 *   "i[ii" - Requires either one or three integer arguments.
 *   "i[ii]" - Same as above (matched brackets).
 *
 * @return On success, the number of arguments present (>=0), a "MOD_ERR" value
 *         (<0). See code for details.
 */
int32_t cmd_parse_args(int32_t argc, const char** argv, const char* fmt,
                       struct cmd_arg_val* arg_vals)
{
    int32_t arg_cnt = 0;
    char* endptr;
    bool opt_args = false;

    while (*fmt) {
        if (*fmt == '[') {
            opt_args = true;
            fmt++;
            continue;
        }
        if (*fmt == ']') {
            fmt++;
            continue;
        }

        if (arg_cnt >= argc) {
            if (opt_args) {
                return arg_cnt;
            }
            printf("Insufficient arguments\n");
            return MOD_ERR_BAD_CMD;
        }

        // These error conditions should not occur, but we check them for
        // safety.
        if (*argv == NULL || **argv == '\0') {
            printf("Invalid empty arguments\n");
            return MOD_ERR_BAD_CMD;
        }

        switch (*fmt) {
            case 'i':
                arg_vals->val.i = strtol(*argv, &endptr, 0);
                if (*endptr) {
                    printf("Argument '%s' not a valid integer\n", *argv);
                    return MOD_ERR_ARG;
                }
                break;
            case 'u':
                arg_vals->val.u = strtoul(*argv, &endptr, 0);
                if (*endptr) {
                    printf("Argument '%s' not a valid unsigned integer\n", *argv);
                    return MOD_ERR_ARG;
                }
                break;
            case 'p':
                arg_vals->val.p = (void*)strtoul(*argv, &endptr, 16);
                if (*endptr) {
                    printf("Argument '%s' not a valid pointer\n", *argv);
                    return MOD_ERR_ARG;
                }
                break;
            case 's':
                arg_vals->val.s = *argv;
                break;
            default:
                printf("Bad argument format '%c'\n", *fmt);
                return MOD_ERR_ARG;
        }
        arg_vals->type = *fmt;
        arg_vals++;
        arg_cnt++;
        argv++;
        fmt++;
        opt_args = false;
    }
    if (arg_cnt < argc) {
        printf("Too many arguments\n");
        return MOD_ERR_BAD_CMD;
    }
    return arg_cnt;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Convert integer log level to a string.
 *
 * @param[in] level The log level as an integer.
 *
 * @return Log level as a string (or "INVALID" on error)
 */
static const char* log_level_str(int32_t level)
{
    if (level < ARRAY_SIZE(log_level_names))
        return log_level_names[level];
    return "INVALID";
}


/*
 * @brief Convert log level string to an int.
 *
 * @param[in] level_name The log level as a string.
 *
 * @return Log level as an int, or -1 on error.
 */
static int32_t log_level_int(const char* level_name)
{
    int32_t level;
    int32_t rc = -1;

    for (level = 0; level < ARRAY_SIZE(log_level_names); level++) {
        if (strcasecmp(level_name, log_level_names[level]) == 0) {
            rc = level;
            break;
        }
    }
    return rc;
}
