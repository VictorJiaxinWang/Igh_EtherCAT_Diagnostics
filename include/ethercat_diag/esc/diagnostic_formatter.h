#pragma once

#include "ethercat_diag/esc/al_status_code_decoder.h"
#include "ethercat_diag/esc/al_status_decoder.h"
#include "ethercat_diag/esc/dl_status_decoder.h"

#include <optional>
#include <string>

std::string formatDlStatus(const DlStatusInfo& status);
std::string formatAlStatus(const AlStatusInfo& status);
std::string formatAlStatusCode(
    const AlStatusCodeInfo& code,
    const std::optional<AlStatusInfo>& al_status
);
