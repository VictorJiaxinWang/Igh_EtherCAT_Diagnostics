#include "ethercat_diag/infrastructure/igh_ioctl_abi.h"

#include <cassert>
#include <cstddef>

int main()
{
    static_assert(EC_IOCTL_VERSION_MAGIC == 37);
    static_assert(igh_ioctl_abi::version_magic == 32U);
    static_assert(EC_MAX_NUM_DEVICES == 1);

    // These literals were observed from the installed /bin/ethercat process.
    // They protect the structure sizes encoded into Linux ioctl requests.
    assert(EC_IOCTL_MODULE == 0x8008a400UL);
    assert(EC_IOCTL_MASTER == 0x80f8a401UL);
    assert(EC_IOCTL_SLAVE == 0xc1a0a402UL);
    assert(EC_IOCTL_SLAVE_REG_READ == 0xc018a412UL);

    static_assert(sizeof(ec_ioctl_master_t) == 248U);
    static_assert(sizeof(ec_ioctl_slave_t) == 416U);
    static_assert(sizeof(ec_ioctl_slave_reg_t) == 24U);
    static_assert(offsetof(ec_ioctl_master_t, phase) == 20U);
    static_assert(offsetof(ec_ioctl_master_t, devices) == 24U);
    static_assert(offsetof(ec_ioctl_slave_t, alias) == 24U);
    static_assert(offsetof(ec_ioctl_slave_t, al_state) == 148U);
    static_assert(offsetof(ec_ioctl_slave_t, name) == 352U);
}
