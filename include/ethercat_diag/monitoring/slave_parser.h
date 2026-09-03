#pragma once

#include "ethercat_diag/common/diag_types.h"

#include <string>
#include <vector>

bool parseSlavesOutput(const std::string& output, 
    std::vector<SlaveSnapshot>& slaves);
