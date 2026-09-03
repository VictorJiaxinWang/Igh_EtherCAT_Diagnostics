#include "ethercat_diag/infrastructure/command_runner.h"

#include <array>
#include <cstdio>
#include <sys/wait.h>

CommandResult execCommand(const std::string& command)
{
	CommandResult result{-1, ""};

	const std::string full_command = "(" + command + ") 2>&1";

	FILE* pipe = popen(full_command.c_str(), "r");

	if (pipe == nullptr)
	{
		return result;
	}

	std::array<char, 256> buffer{};

	while(fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
	{
		result.output += buffer.data();
	}

	const int status = pclose(pipe);
	
	if (status == -1)
	{
		return result;
	}

	if (WIFEXITED(status))
	{
		result.exit_code = WEXITSTATUS(status);
	}

	return result;
}
