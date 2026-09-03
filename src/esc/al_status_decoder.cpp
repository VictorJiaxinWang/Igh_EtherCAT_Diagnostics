#include "ethercat_diag/esc/al_status_decoder.h"

AlStatusInfo decodeAlStatus(std::uint16_t raw)
{
    AlStatusInfo status;

    status.raw = raw;

    const std::uint16_t state_bits =
        raw & 0x000F;

    switch (state_bits)
    {
    case 0x0001:
        status.state = EscAlState::Init;
        break;

    case 0x0002:
        status.state = EscAlState::PreOp;
        break;

    case 0x0003:
        status.state = EscAlState::Boot;
        break;

    case 0x0004:
        status.state = EscAlState::SafeOp;
        break;

    case 0x0008:
        status.state = EscAlState::Op;
        break;

    default:
        status.state = EscAlState::Unknown;
        break;
    }

    status.error_indication = ((status.raw & 0x10) != 0);
    status.explicit_device_id_loaded = ((status.raw & 0x20) != 0);
    status.warning_indication = ((status.raw & 0x40) != 0);

    return status;
}

std::string_view alStateName(EscAlState state)
{
    switch (state)
    {
    case EscAlState::Init:
        return "INIT";

    case EscAlState::PreOp:
        return "PREOP";

    case EscAlState::Boot:
        return "BOOT";

    case EscAlState::SafeOp:
        return "SAFEOP";

    case EscAlState::Op:
        return "OP";

    case EscAlState::Unknown:
    default:
        return "UNKNOWN";
    }
}