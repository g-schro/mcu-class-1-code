/*
 * @brief Implementation of stat utility.
 *
 * This utility collects data and performs statistical calculations. Currently it
 * supports time duration measurements.
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

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "tmr.h"
#include "stat.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private (static) function declarations
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) variables and externs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Initializize time duration statistic.
 *
 * @param[in] stat Time duration statistic.
 */
void stat_dur_init(struct stat_dur* stat)
{
    memset(stat, 0, sizeof(*stat));
    stat->min = UINT32_MAX;
    stat->max = 0;
}

/*
 * @brief Indicate start of duration interval.
 *
 * @param[in] stat Time duration statistic.
 */
void stat_dur_start(struct stat_dur* stat)
{
    stat->start_ms = tmr_get_ms();
    stat->started = true;
}

/*
 * @brief Indicate end of duration interval.
 *
 * @param[in] stat Time duration statistic.
 */
void stat_dur_end(struct stat_dur* stat)
{
    uint32_t dur;

    if (stat->samples == UINT32_MAX || !stat->started)
        return;

    stat->started = false;
    dur = tmr_get_ms() - stat->start_ms;
    stat->accum_ms += dur;
    stat->samples++;
    if (dur > stat->max)
        stat->max = dur;
    if (dur < stat->min)
        stat->min = dur;
}

/*
 * @brief Restart measurement of a duration interval.
 *
 * @param[in] stat Time duration statistic.
 *
 * If a duration measurement was in progress, it is ended and recorded, and a
 * new measurement is started. This function allows, for example, a loop
 * duration to be measured by simply calling this function at one location in
 * the loop.  This method ensures that no time is lost between ending a
 * measurement, and starting the next one.
 */
void stat_dur_restart(struct stat_dur* stat)
{
    uint32_t now_ms;
    uint32_t dur;

    if (stat->samples == UINT32_MAX)
        return;

    now_ms = tmr_get_ms();

    if (stat->started) {
        dur = now_ms - stat->start_ms;
        stat->accum_ms += dur;
        stat->samples++;
        if (dur > stat->max)
            stat->max = dur;
        if (dur < stat->min)
            stat->min = dur;
    }

    stat->started = true;
    stat->start_ms = now_ms;
}

/*
 * @brief Get average duration in us.
 *
 * @param[in] stat Time duration statistic.
 *
 * @return Average duration in us (0 if there are no samples).
 */
uint32_t stat_dur_avg_us(struct stat_dur* stat)
{
    if (stat->samples == 0)
        return 0;
    return (stat->accum_ms * 1000) / stat->samples;
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) functions
////////////////////////////////////////////////////////////////////////////////
