//
// Created by crocdialer on 12/13/25.
//

#pragma once

#include <chrono>

namespace vierkant
{
/**
 * @brief   timestamp_diff return the difference between two provided uint64_t time-points in milliseconds.
 *
 * @param   start               a start gpu-timepoint
 * @param   end                 an end gpu-timepoint
 * @param   timestamp_period    time-period between two time-points in nanoseconds
 * @return  a double millisecond value
 */
inline double timestamp_diff(uint64_t start, uint64_t end, float timestamp_period)
{
    using namespace std::chrono;
    return duration_cast<duration<double, std::milli>>(
                   nanoseconds(static_cast<uint64_t>(static_cast<double>(end - start) * timestamp_period)))
            .count();
}

/**
 * @brief   timestamp_millis is an utility for working with timestamp arrays with following format:
 *          [start_0, end_0, ..., start_N, end_N]
 *
 * @param   timestamps          array of start/end time-points
 * @param   idx                 index into above array
 * @param   timestamp_period    provided time-stamp period in nanoseconds
 * @return  the time between start/end of idx
 */
inline double timestamp_millis(const uint64_t *timestamps, int32_t idx, float timestamp_period)
{
    return timestamp_diff(timestamps[2 * idx], timestamps[2 * idx + 1], timestamp_period);
}
}// namespace vierkant
