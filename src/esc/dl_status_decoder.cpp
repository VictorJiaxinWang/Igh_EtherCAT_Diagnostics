#include "ethercat_diag/esc/dl_status_decoder.h"

DlStatusInfo decodeDlStatus(std::uint16_t raw)
{
	DlStatusInfo status;
    
    status.raw = raw;

    status.pdi_operational = ((status.raw & 0x01) != 0);
    status.pdi_watchdog_reloaded = ((status.raw & 0x02) != 0);
    status.enhanced_link_detection = ((status.raw & 0x04) != 0);
    
    for(std::uint8_t i = 0; i < 4; i++)
    {
        status.ports[i].physical_link = ((status.raw & (1 << (4 + i))) != 0);
        status.ports[i].loop_closed = ((status.raw & (1 << (8 + 2 * i))) != 0);
        status.ports[i].communication_established = ((status.raw & (1 << (9 + 2 * i))) != 0);
    }

    return status;
}
