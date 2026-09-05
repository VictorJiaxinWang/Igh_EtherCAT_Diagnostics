#pragma once

#include "ethercat_diag/root_cause/root_cause_report.h"

#include <string>

const char* rootCauseKindToString(RootCauseKind kind) noexcept;

std::string formatRootCauseReport(const RootCauseReport& report);

std::string rootCauseReportToJson(const RootCauseReport& report);

bool appendRootCauseReportJsonl(
    const std::string& file_path,
    const RootCauseReport& report);
