#pragma once

#include "ethercat_diag/common/diag_types.h"
#include <string>

bool parseMasterOutput(
	const std::string& output,
	MasterSnapshot& snapshot);
