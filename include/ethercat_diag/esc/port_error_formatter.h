#pragma once

#include "ethercat_diag/esc/port_error_decoder.h"

#include <string>

std::string formatPortErrorCounters(
    const PortErrorCounters& counters);
