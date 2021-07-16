/*
 * @brief Implementation of ttys module.
 *
 * This module provides a simple "TTY serial" interface for MCU UARTs.
 * Main features:
 * - Buffering on output to prevent blocking (overrun is possible)
 * - Buffering on input to avoid loss of input characters (overrun is possible)
 * - Integrate into the C standard library streams I/O (to support printf and
     friends)
 * - Performance measurements.
 * - Console commands
 *
 * The following console commands are provided:
 * > ttys status
 * > ttys test
 * See code for details.
 *
 * This library makes use of the STMicroelectronics Low Level (LL) device
 * library.
 *
 * Currently, this module does not perform hardware initialization of the UART
 * and associated hardware (e.g. GPIO), except for the interrupt controller (see
 * below). It is expected that the driver library has been used for
 * initilazation (e.g. via generated IDE code). This avoids having to deal with
 * the issue of inconsistent driver libraries among MCUs.
 *
 * This module enables USART interrupts on the Nested Vector Interrupt
 * Controller (NVIC). It also overrides the (weak) USART interrupt handler
 * functions (USARTx_IRQHandler). Thus, in the IDE device configuration tool,
 * the "USARTx global interrupt" should NOT be chosen or you will get a
 * duplicate symbol at link time.
 *
 * A future feature is to perform full hardware initialization in this library,
 * and allowing at least some UART parameters to be set (e.g. buad).
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "stm32f4xx_ll_usart.h"

#include "cmd.h"
#include "log.h"
#include "module.h"
#include "tmr.h"
#include "ttys.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

// This module integrates into the C language stdio system. The ttys "device
// files" can be viewed as always "open", with the following file
// descriptors. These are then mapped to FILE streams.
//
// Note that one of the UARTs is mapped to stdout (file descriptor 1), which
// is thus the one used for printf() and friends.

#define UART1_FD 4
#define UART2_FD 1
#define UART6_FD 3

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

// Per-instance ttys state information.
struct ttys_state {
    struct ttys_cfg cfg;
    FILE* stream;
    int fd;
    USART_TypeDef* uart_reg_base;
    uint16_t rx_buf_get_idx;
    uint16_t rx_buf_put_idx;
    uint16_t tx_buf_get_idx;
    uint16_t tx_buf_put_idx;
    char tx_buf[TTYS_TX_BUF_SIZE];
    char rx_buf[TTYS_RX_BUF_SIZE];
};

// Performance measurements for ttys. Currently these are common to all
// instances.  A future enhancement would be to make them per-instance.

enum ttys_u16_pms {
    CNT_RX_UART_ORE,
    CNT_RX_UART_NE,
    CNT_RX_UART_FE,
    CNT_RX_UART_PE,
    CNT_TX_BUF_OVERRUN,
    CNT_RX_BUF_OVERRUN,

    NUM_U16_PMS
};

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

static void ttys_interrupt(enum ttys_instance_id instance_id,
                           IRQn_Type irq_type);
static int32_t cmd_ttys_status(int32_t argc, const char** argv);
static int32_t cmd_ttys_test(int32_t argc, const char** argv);

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

static struct ttys_state ttys_states[TTYS_NUM_INSTANCES];

static int32_t log_level = LOG_DEFAULT;

// Storage for performance measurements.
static uint16_t cnts_u16[NUM_U16_PMS];

// Names of performance measurements.
static const char* cnts_u16_names[NUM_U16_PMS] = {
    "uart rx overrun err",
    "uart rx noise err",
    "uart rx frame err",
    "uart rx parity err",
    "tx buf overrun err",
    "rx buf overrun err",
};

// Data structure with console command info.
static struct cmd_cmd_info cmds[] = {
    {
        .name = "status",
        .func = cmd_ttys_status,
        .help = "Get module status, usage: ttys status",
    },
    {
        .name = "test",
        .func = cmd_ttys_test,
        .help = "Run test, usage: ttys test [<op> [<arg>]] (enter no op/arg for help)",
    }
};

// Data structure passed to cmd module for console interaction.
static struct cmd_client_info cmd_info = {
    .name = "ttys",
    .num_cmds = ARRAY_SIZE(cmds),
    .cmds = cmds,
    .log_level_ptr = &log_level,
    .num_u16_pms = NUM_U16_PMS,
    .u16_pms = cnts_u16,
    .u16_pm_names = cnts_u16_names,
};

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Get default ttys configuration.
 *
 * @param[out] cfg The ttys configuration with defaults filled in.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 */
int32_t ttys_get_def_cfg(enum ttys_instance_id instance_id, struct ttys_cfg* cfg)
{
    if (cfg == NULL)
        return MOD_ERR_ARG;

    memset(cfg, 0, sizeof(*cfg));
    cfg->create_stream = true;
    cfg->send_cr_after_nl = true;
    return 0;
}

/*
 * @brief Initialize ttys module instance.
 *
 * @param[in] instance_id Identifies the ttys instance.
 * @param[in] cfg The ttys module configuration (FUTURE)
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function initializes a ttys module instance. Generally, it should not
 * access other modules as they might not have been initialized yet.
 */
int32_t ttys_init(enum ttys_instance_id instance_id, struct ttys_cfg* cfg)
{
    struct ttys_state* st;

    if (instance_id >= TTYS_NUM_INSTANCES)
        return MOD_ERR_BAD_INSTANCE;

    if (cfg == NULL)
        return MOD_ERR_ARG;

    // We selectively initialize the state structure, as we want to preserve the
    // transmit queue in case there is output in it.  However, if the transmit
    // queue appears corrupted, we initialize the whole thing.

    st = &ttys_states[instance_id];
    if (st->tx_buf_get_idx >= TTYS_TX_BUF_SIZE ||
        st->tx_buf_put_idx >= TTYS_TX_BUF_SIZE) {
        memset(st, 0, sizeof(*st));
    } else {
        st->rx_buf_get_idx = 0;
        st->rx_buf_put_idx = 0;
    }
    st->cfg = *cfg;

    switch (instance_id) {
        case TTYS_INSTANCE_UART1:
            st->uart_reg_base = USART1;
            st->fd = UART1_FD;
            break;
        case TTYS_INSTANCE_UART2:
            st->uart_reg_base = USART2;
            st->fd = UART2_FD;
            break;
        case TTYS_INSTANCE_UART6:
            st->uart_reg_base = USART6;
            st->fd = UART6_FD;
            break;
        default:
            return MOD_ERR_BAD_INSTANCE;
    }
    if (st->cfg.create_stream) {
        st->stream = fdopen(st->fd, "r+");
        if (st->stream != NULL)
            setvbuf(st->stream, NULL, _IONBF, 0);    
    } else {
        st->stream = NULL;
    }
    return 0;
}

/*
 * @brief Start ttys module instance.
 *
 * @param[in] instance_id Identifies the ttys instance.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * This function starts a ttys module instance, to enter normal operation. This
 * includes enabling UART interrupts.
 */
int32_t ttys_start(enum ttys_instance_id instance_id)
{
    struct ttys_state* st;
    IRQn_Type irq_type;
    int32_t result;

    if (instance_id >= TTYS_NUM_INSTANCES ||
        ttys_states[instance_id].uart_reg_base == NULL)
        return MOD_ERR_BAD_INSTANCE;

    result = cmd_register(&cmd_info);
    if (result < 0) {
        log_error("ttys_start: cmd error %d\n", result);
        return MOD_ERR_RESOURCE;
    }

    st = &ttys_states[instance_id];
    LL_USART_EnableIT_RXNE(st->uart_reg_base);
    LL_USART_EnableIT_TXE(st->uart_reg_base);

    switch (instance_id) {
        case TTYS_INSTANCE_UART1:
            irq_type = USART1_IRQn;
            break;
        case TTYS_INSTANCE_UART2:
            irq_type = USART2_IRQn;
            break;
        case TTYS_INSTANCE_UART6:
            irq_type = USART6_IRQn;
            break;
        default:
            return MOD_ERR_BAD_INSTANCE;
    }
    NVIC_SetPriority(irq_type,
                     NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
    NVIC_EnableIRQ(irq_type);

    return 0;
}

/*
 * @brief Put a character for transmission.
 *
 * @param[in] instance_id Identifies the ttys instance.
 * @param[in] c Character to transmit.
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * @note Before this module is started, the UART is not known, but the user can
 *       still put chars in the TX buffer that will be transmitted if and when
 *       the module is started.
 */
int32_t ttys_putc(enum ttys_instance_id instance_id, char c)
{
    struct ttys_state* st;
    uint16_t next_put_idx;

    if (instance_id >= TTYS_NUM_INSTANCES)
        return MOD_ERR_BAD_INSTANCE;
    st = &ttys_states[instance_id];

    // Calculate the new TX buffer put index
    next_put_idx = st->tx_buf_put_idx + 1;
    if (next_put_idx >= TTYS_TX_BUF_SIZE)
        next_put_idx = 0;

    // If buffer is full, then return error.
    while (next_put_idx == st->tx_buf_get_idx) {
        INC_SAT_U16(cnts_u16[CNT_TX_BUF_OVERRUN]);
        return MOD_ERR_BUF_OVERRUN;
    }

    // Put the char in the TX buffer.
    st->tx_buf[st->tx_buf_put_idx] = c;
    st->tx_buf_put_idx = next_put_idx;

    // Ensure the TX interrupt is enabled.
    if (ttys_states[instance_id].uart_reg_base != NULL) {
        __disable_irq();
        LL_USART_EnableIT_TXE(st->uart_reg_base);
        __enable_irq();
    }
    return 0;
}

/*
 * @brief Get a received character.
 *
 * @param[in] instance_id Identifies the ttys instance.
 * @param[out] c Received character.
 *
 * @return Number of characters returned (0 or 1)
 */
int32_t ttys_getc(enum ttys_instance_id instance_id, char* c)
{
    struct ttys_state* st;
    int32_t next_get_idx;

    if (instance_id >= TTYS_NUM_INSTANCES)
        return MOD_ERR_BAD_INSTANCE;

    st = &ttys_states[instance_id];

    // Check if buffer is empty.
    if (st->rx_buf_get_idx == st->rx_buf_put_idx)
        return 0;

    // Get a character and advance get index.
    next_get_idx = st->rx_buf_get_idx + 1;
    if (next_get_idx >= TTYS_RX_BUF_SIZE)
        next_get_idx = 0;
    *c = st->rx_buf[st->rx_buf_get_idx];
    st->rx_buf_get_idx = next_get_idx;
    return 1;
}

/*
 * @brief Get file descriptor for a ttys instance.
 *
 * @param[in] instance_id Identifies the ttys instance.
 *
 * @return File descriptor (>= 0) for success, else a "MOD_ERR" value (<0). See
 *          code for details.
 */
int ttys_get_fd(enum ttys_instance_id instance_id)
{
    if (instance_id >= TTYS_NUM_INSTANCES)
        return MOD_ERR_ARG;
    if (ttys_states[instance_id].fd >= 0)
        return ttys_states[instance_id].fd;
    return MOD_ERR_RESOURCE;
}

/*
 * @brief Get FILE stream for a ttys instance.
 *
 * @param[in] instance_id Identifies the ttys instance.
 *
 * @return FILE stream pointer, or NULL if error.
 */
FILE* ttys_get_stream(enum ttys_instance_id instance_id)
{
    if (instance_id >= TTYS_NUM_INSTANCES)
        return NULL;

    return ttys_states[instance_id].stream;
}

// The following interrupt handler functions override the default handlers,
// which are "weak" symobols.

void USART1_IRQHandler(void)
{
    ttys_interrupt(TTYS_INSTANCE_UART1, USART1_IRQn);
}

void USART2_IRQHandler(void)
{
    ttys_interrupt(TTYS_INSTANCE_UART2, USART2_IRQn);
}

void USART6_IRQHandler(void)
{
    ttys_interrupt(TTYS_INSTANCE_UART6, USART6_IRQn);
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief UART interrupt handler
 *
 * @param[in] instance_id Identifies the ttys instance.
 * @param[in] irq_type Identifies the type of interrupt.
 *
 * @note This function should not write output using stdio API, to ensure we do
 *       not corrupt the put index on the tx buffer.
 */
static void ttys_interrupt(enum ttys_instance_id instance_id,
                           IRQn_Type irq_type)
{
    struct ttys_state* st;
    uint8_t sr;

    if (instance_id >= TTYS_NUM_INSTANCES)
        return;

    st = &ttys_states[instance_id];

    // If instance is not open, we should not get an interrupt, but for safety
    // just disable it.
    if (st->uart_reg_base == NULL) {
        NVIC_DisableIRQ(irq_type);
        return;
    }

    sr = st->uart_reg_base->SR;

    if (sr & LL_USART_SR_RXNE) {
        // Got an incoming character.
        uint16_t next_rx_put_idx = st->rx_buf_put_idx + 1;
        char rx_data = st->uart_reg_base->DR;
        if (next_rx_put_idx >= TTYS_RX_BUF_SIZE)
            next_rx_put_idx = 0;
        if (next_rx_put_idx == st->rx_buf_get_idx) {
            INC_SAT_U16(cnts_u16[CNT_RX_BUF_OVERRUN]);
        } else {
            st->rx_buf[st->rx_buf_put_idx] = rx_data;
            st->rx_buf_put_idx = next_rx_put_idx;
        }
    }
    if (sr & LL_USART_SR_TXE) {
        // Can send a character.
        if (st->tx_buf_get_idx == st->tx_buf_put_idx) {
            // No characters to send, disable the interrrupt.
            LL_USART_DisableIT_TXE(st->uart_reg_base);
        } else {
            st->uart_reg_base->DR = st->tx_buf[st->tx_buf_get_idx];
            if (st->tx_buf_get_idx < TTYS_TX_BUF_SIZE-1)
                st->tx_buf_get_idx++;
            else
                st->tx_buf_get_idx = 0;
        }
    }
    if (sr & (LL_USART_SR_ORE | LL_USART_SR_NE | LL_USART_SR_FE |
              LL_USART_SR_PE)) {

        // Error conditions. To clear the bit, we need to read the data
        // register, but we don't use it.

        (void)st->uart_reg_base->DR;
        if (sr & LL_USART_SR_ORE)
            INC_SAT_U16(cnts_u16[CNT_RX_UART_ORE]);
        if (sr & LL_USART_SR_NE)
            INC_SAT_U16(cnts_u16[CNT_RX_UART_NE]);
        if (sr & LL_USART_SR_FE)
            INC_SAT_U16(cnts_u16[CNT_RX_UART_FE]);
        if (sr & LL_USART_SR_PE)
            INC_SAT_U16(cnts_u16[CNT_RX_UART_PE]);
    }
}

/*
 * @brief Console command function for "ttys status".
 *
 * @param[in] argc Number of arguments, including "ttys"
 * @param[in] argv Argument values, including "ttys"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: ttys status
 */
static int32_t cmd_ttys_status(int32_t argc, const char** argv)
{
    enum ttys_instance_id instance_id;

    for (instance_id = 0; instance_id < TTYS_NUM_INSTANCES; instance_id++) {
        struct ttys_state* st = &ttys_states[instance_id];
        printf("Instance %d:\n", instance_id);
        if (st->uart_reg_base == NULL) {
            printf("  NULL\n");
        } else {
            printf("  TX buffer: get_idx=%u put_idx=%u\n",
                   st->tx_buf_get_idx, st->tx_buf_put_idx);
            printf("  RX buffer: get_idx=%u put_idx=%d\n",
                   st->rx_buf_get_idx, st->rx_buf_put_idx);
        }
    }
    return 0;
}

/*
 * @brief Console command function for "ttys test".
 *
 * @param[in] argc Number of arguments, including "ttys"
 * @param[in] argv Argument values, including "ttys"
 *
 * @return 0 for success, else a "MOD_ERR" value. See code for details.
 *
 * Command usage: ttys test [<op> [<arg>]]
 */
static int32_t cmd_ttys_test(int32_t argc, const char** argv)
{
    struct cmd_arg_val arg_vals[1];
    uint32_t param;
    FILE* f = NULL;
    int fd = 0;
    int rc;
    uint32_t start_ms;
    const char* test_msg = "Test\n";
    const int test_msg_len = strlen(test_msg);

    // Handle help case.
    if (argc == 2) {
        printf("Test operations and param(s) are as follows:\n"
               "  Write test msg using fprintf, usage: ttys test fprintf <instance-id>\n"
               "  Write test msg using write, usage: ttys test write <instance-id>\n"
               "  Read chars for 5 seconds using fgetc, usage: ttys test fgetc <instance-id>\n"
               "  Read chars for 5 seconds using read, usage: ttys test read <instance-id>\n"
               "\nWARNING! Read tests block!\n"
            );
        return 0;
    }

    // Initial argument checking.
    if (cmd_parse_args(argc-3, argv+3, "u", arg_vals) != 1)
            return MOD_ERR_BAD_CMD;

    param = arg_vals[0].val.u;

    if (strcasecmp(argv[2], "fprintf") == 0 ||
        strcasecmp(argv[2], "fgetc") == 0) {
        f = ttys_get_stream((enum ttys_instance_id)param);
        if (f == NULL) {
            printf("Can't get FILE stream for instance.\n");
            return MOD_ERR_RESOURCE;
        }
    }
    else if (strcasecmp(argv[2], "write") == 0 ||
        strcasecmp(argv[2], "read") == 0) {
        fd = ttys_get_fd((enum ttys_instance_id)param);
        if (fd < 0) {
            printf("Can't get fd for instance result=%d.\n", fd);
            return MOD_ERR_RESOURCE;
        }
    } else {
        printf("Invalid operation '%s'\n", argv[2]);
        return MOD_ERR_BAD_CMD;
    }

    if (strcasecmp(argv[2], "fprintf") == 0) {
        // command: ttys test fprintf <instance-id>
        rc = fprintf(f, test_msg);
        fflush(f);
        printf("fprintf to FILE %p returns %d errno=%d\n", f, rc, errno);
    } else if (strcasecmp(argv[2], "write") == 0) {
        // command: ttys test write <instance-id>
        rc = write(fd, test_msg, test_msg_len);
        printf("write to fd %d returns %d errno=%d\n", fd, rc, errno);
    } else if (strcasecmp(argv[2], "fgetc") == 0) {
        // command: ttys test fgetc <instance-id>
        start_ms = tmr_get_ms();
        while (tmr_get_ms() - start_ms < 5000) {
            rc = fgetc(f);
            if (rc > 0)
                printf("Got char 0x%02x\n", rc);
            else if (rc != -1 || errno != EAGAIN) {
                printf("Unexpected result rc=%d errno=%d\n", rc, errno);
                break;
            }
        }
    } else if (strcasecmp(argv[2], "read") == 0) {
        // command: ttys test read <instance-id>
        start_ms = tmr_get_ms();
        while (tmr_get_ms() - start_ms < 5000) {
            char c;
            rc = read(fd, &c, 1);
            if (rc == 1)
                printf("Got char 0x%02x\n", c);
            else if (rc != -1 || errno != EWOULDBLOCK) {
                printf("Unexpected result rc=%d errno=%d\n", rc, errno);
                break;
            }
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// The following functions are used to integrate this module into the C
// language stdio system. This is largely based on overriding of the default
// "system call functions" _write and _read. The default functions use "weak"
// symbols.
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Map file descriptor to ttys instance.
 *
 * @param[in] fd File descriptor.
 *
 * @return Instance ID corresponding to file instance, or TTYS_NUM_INSTANCES if
 *         no match.
 */
static enum ttys_instance_id fd_to_instance(int fd)
{
    enum ttys_instance_id instance_id = TTYS_NUM_INSTANCES;

    switch (fd) {
        case UART6_FD:
            instance_id = TTYS_INSTANCE_UART6;
            break;
        case UART1_FD:
            instance_id = TTYS_INSTANCE_UART1;
            break;
        case UART2_FD:
            instance_id = TTYS_INSTANCE_UART2;
            break;
    }
    return instance_id;
}

/*
 * @brief System call function for write().
 *
 * @param[in] file File descriptor.
 * @param[in] ptr Data to be written.
 * @param[in] len Length of data.
 *
 * @return Number of characters written, or -1 for error.
 *
 * @note Characters might be dropped due to buffer overrun, and this is not
 *       reflected the return value. An alternative would be to return -1 and
 *       errno=EWOULDBLOCK.
 */
int _write(int file, char* ptr, int len)
{
    int idx;
    enum ttys_instance_id instance_id = fd_to_instance(file);

    if (instance_id >= TTYS_NUM_INSTANCES) {
        errno = EBADF;
        return -1;
    }

    for (idx = 0; idx < len; idx++) {
        char c = *ptr++;
        ttys_putc(instance_id, c);
        if (c == '\n' && ttys_states[instance_id].cfg.send_cr_after_nl) {
            ttys_putc(instance_id, '\r');
        }
    }
    return len;
}

/*
 * @brief System call function for read().
 *
 * @param[in] file File descriptor.
 * @param[in] ptr Location of buffer to place characters.
 * @param[in] len Length of buffer.
 *
 * @return Number of characters written, or -1 for error.
 *
 * @note Assumes non-blocking operation.
 */
int _read(int file, char* ptr, int len)
{
    int rc = 0;
    char c;
    enum ttys_instance_id instance_id = fd_to_instance(file);

    if (instance_id >= TTYS_NUM_INSTANCES) {
        errno = EBADF;
        return -1;
    }

    if (ttys_states[instance_id].rx_buf_get_idx ==
        ttys_states[instance_id].rx_buf_put_idx) {
        errno = EAGAIN;
        rc = -1;
    } else {
        while (rc < len && ttys_getc(instance_id, &c)) {
            *ptr++ = c;
            rc++;
        }
    }
    return rc;
}
