#include "ethercat_diag/infrastructure/igh_master_device.h"

#include "ethercat_diag/infrastructure/igh_ioctl_abi.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>

namespace
{

struct FakeLinux
{
    int open_count{};
    int ioctl_count{};
    int close_count{};
    int opened_flags{-1};
    std::string opened_path;
    int open_result{42};
    int open_errno{};
    int ioctl_errno{};
    int ioctl_failures_before_success{};
    std::uint32_t magic{igh_ioctl_abi::version_magic};

    LinuxCalls calls()
    {
        return {
            [this](const char* path, int flags) {
                ++open_count;
                opened_path = path;
                opened_flags = flags;
                if (open_result < 0)
                {
                    errno = open_errno;
                }
                return open_result;
            },
            [this](int fd, unsigned long request, void* argument) {
                ++ioctl_count;
                assert(fd == open_result);
                if (ioctl_count <= ioctl_failures_before_success)
                {
                    errno = ioctl_errno;
                    return -1;
                }
                if (request == EC_IOCTL_MODULE)
                {
                    auto* module = static_cast<ec_ioctl_module_t*>(argument);
                    module->ioctl_version_magic = magic;
                    module->master_count = 1;
                }
                return 0;
            },
            [this](int fd) {
                assert(fd == open_result);
                ++close_count;
                return 0;
            }
        };
    }
};

void testRejectsNegativeMasterIndexWithoutOpening()
{
    FakeLinux fake;
    IghMasterDevice device(fake.calls());

    assert(!device.openAndValidate(-1, IghDeviceAccess::READ_ONLY));
    assert(fake.open_count == 0);
    assert(fake.ioctl_count == 0);
    assert(device.error().code == IghDeviceErrorCode::INVALID_MASTER_INDEX);
}

void testOpensExpectedReadOnlyDeviceAndClosesOnce()
{
    FakeLinux fake;
    {
        IghMasterDevice device(fake.calls());
        assert(device.openAndValidate(3, IghDeviceAccess::READ_ONLY));
        assert(device.isOpen());
        assert(fake.opened_path == "/dev/EtherCAT3");
        assert(fake.opened_flags == O_RDONLY);
        assert(fake.ioctl_count == 1);
        assert(device.error().code == IghDeviceErrorCode::NONE);
    }
    assert(fake.close_count == 1);
}

void testUsesReadWriteFlagWhenRequested()
{
    FakeLinux fake;
    IghMasterDevice device(fake.calls());

    assert(device.openAndValidate(0, IghDeviceAccess::READ_WRITE));
    assert(fake.opened_flags == O_RDWR);
}

void testReportsOpenErrorsAndDoesNotClose()
{
    for (const int expected_errno : {ENOENT, EACCES})
    {
        FakeLinux fake;
        fake.open_result = -1;
        fake.open_errno = expected_errno;
        {
            IghMasterDevice device(fake.calls());
            assert(!device.openAndValidate(0, IghDeviceAccess::READ_ONLY));
            assert(device.error().code == IghDeviceErrorCode::OPEN_FAILED);
            assert(device.error().system_errno == expected_errno);
            assert(device.error().operation == "open");
        }
        assert(fake.close_count == 0);
    }
}

void testRejectsMismatchedAbiAndReportsBothValues()
{
    FakeLinux fake;
    fake.magic = 31;
    IghMasterDevice device(fake.calls());

    assert(!device.openAndValidate(0, IghDeviceAccess::READ_ONLY));
    assert(device.error().code == IghDeviceErrorCode::ABI_MISMATCH);
    assert(device.error().expected_magic == 32U);
    assert(device.error().actual_magic == 31U);
}

void testRetriesIoctlAfterEintr()
{
    FakeLinux fake;
    fake.ioctl_failures_before_success = 1;
    fake.ioctl_errno = EINTR;
    IghMasterDevice device(fake.calls());

    assert(device.openAndValidate(0, IghDeviceAccess::READ_ONLY));
    assert(fake.ioctl_count == 2);
}

void testReportsIoctlErrorWithoutRetryingOtherErrnos()
{
    FakeLinux fake;
    fake.ioctl_failures_before_success = 1;
    fake.ioctl_errno = EIO;
    IghMasterDevice device(fake.calls());

    assert(!device.openAndValidate(0, IghDeviceAccess::READ_ONLY));
    assert(fake.ioctl_count == 1);
    assert(device.error().code == IghDeviceErrorCode::IOCTL_FAILED);
    assert(device.error().system_errno == EIO);
    assert(device.error().operation == "EC_IOCTL_MODULE");
}

} // namespace

int main()
{
    testRejectsNegativeMasterIndexWithoutOpening();
    testOpensExpectedReadOnlyDeviceAndClosesOnce();
    testUsesReadWriteFlagWhenRequested();
    testReportsOpenErrorsAndDoesNotClose();
    testRejectsMismatchedAbiAndReportsBothValues();
    testRetriesIoctlAfterEintr();
    testReportsIoctlErrorWithoutRetryingOtherErrnos();

    std::cout << "IghMasterDevice tests passed\n";
}
