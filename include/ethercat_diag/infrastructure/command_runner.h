#pragma once

#include <string>

struct CommandResult
{
	int exit_code;
	std::string output;
};

CommandResult execCommand(const std::string& command);
