#include "ethercat_diag/esc/al_status_code_decoder.h"

AlStatusCodeInfo decodeAlStatusCode(std::uint16_t raw)
{
	AlStatusCodeInfo result;

	result.raw = raw;
	result.known = true;
		
	switch(result.raw)
	{
		case 0x0000:
			result.description = "No error";
			break;

		case 0x0003:
			result.description = "Invalid device setup";
			break;
		
		case 0x0011:
			result.description = "Invalid requested state change";
			break;

		case 0x0016:
			result.description = "Invalid mailbox configuration";
			break;

		case 0x001A:
			result.description = "Synchronization error";
			break;

		case 0x001B:
			result.description = "SyncManager watchdog";
			break;

		case 0x001D:
			result.description = "Invalid output configuration";
			break;

		case 0x001E:
			result.description = "Invalid input configuration";
			break;

		case 0x002C:
			result.description = "Fatal sync error";
			break;

		case 0x002D:
			result.description = "No sync error";
			break;

		case 0x0035:
			result.description = "Invalid DC sync cycle time";
			break;

		case 0x0052:
			result.description = "External hardware not ready";
			break;

		default:
			result.description = "Unknown AL status code";
			result.known = false;
			break;
	}

	return result;
}

AlStatusCodeContext classifyAlStatusCode(
    std::uint16_t code,
    bool error_indication,
    bool warning_indication
)
{
    if (code == 0)
    {
        return AlStatusCodeContext::NoError;
    }
    else
    {
        if (error_indication == true)
        {
            return AlStatusCodeContext::ActiveError;
        }
        else if (warning_indication == true)
        {
            return AlStatusCodeContext::ActiveWarning;
        }
        else
        {
            return AlStatusCodeContext::Inactive;
        }   
    }
}