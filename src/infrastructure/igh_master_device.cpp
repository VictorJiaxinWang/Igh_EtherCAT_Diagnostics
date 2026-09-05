#include "ethercat_diag/infrastructure/igh_master_device.h"
#include "ethercat_diag/infrastructure/igh_ioctl_abi.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

LinuxCalls productionLinuxCalls()
{
    return {
        [](const char* path, int flags) {
            return ::open(path, flags);
        },
        [](int fd, unsigned long request, void* argument) {
            return ::ioctl(fd, request, argument);
        },
        [](int fd) {
            return ::close(fd);
        }
    };
}

IghMasterDevice::IghMasterDevice(LinuxCalls calls)
    : calls_(std::move(calls))
{
}

IghMasterDevice::~IghMasterDevice()
{
    if (fd_ >= 0 && calls_.close)
    {
        calls_.close(fd_);
    }
}

bool IghMasterDevice::openAndValidate(
    int master_index,
    IghDeviceAccess access)
{
    clearError();

    if (master_index < 0)
    {
        error_.code = IghDeviceErrorCode::INVALID_MASTER_INDEX;
        error_.operation = "open";
        error_.message = "master index must not be negative";
        return false;
    }

    if (fd_ >= 0 || !calls_.open || !calls_.ioctl || !calls_.close)
    {
        error_.code = IghDeviceErrorCode::INVALID_DATA;
        error_.operation = "open";
        error_.message = "invalid device state or Linux call table";
        return false;
    }

    const std::string path =
        "/dev/EtherCAT" + std::to_string(master_index);

    const int flags =
        access == IghDeviceAccess::READ_ONLY
            ? O_RDONLY
            : O_RDWR;

    do
    {
        fd_ = calls_.open(path.c_str(), flags);
    }
    while (fd_ < 0 && errno == EINTR);

    if (fd_ < 0)
    {
        const int saved_errno = errno;
        setSystemError(
            IghDeviceErrorCode::OPEN_FAILED,
            saved_errno,
            "open");
        return false;
    }

    ec_ioctl_module_t module{};

    if (!call(
            EC_IOCTL_MODULE,
            &module,
            "EC_IOCTL_MODULE"))
    {
        return false;
    }

    if (module.ioctl_version_magic !=
        igh_ioctl_abi::version_magic)
    {
        error_.code = IghDeviceErrorCode::ABI_MISMATCH;
        error_.operation = "EC_IOCTL_MODULE";
        error_.expected_magic = igh_ioctl_abi::version_magic;
        error_.actual_magic = module.ioctl_version_magic;

        std::ostringstream message;
        message
            << "IgH ioctl ABI mismatch: expected "
            << error_.expected_magic
            << ", got "
            << error_.actual_magic;
        error_.message = message.str();
        return false;
    }

    clearError();
    return true;
}

bool IghMasterDevice::call(
    unsigned long request,
    void* argument,
    const std::string& operation)
{
    if (fd_ < 0 || !calls_.ioctl)
    {
        error_.code = IghDeviceErrorCode::INVALID_DATA;
        error_.operation = operation;
        error_.message = "device is not open";
        return false;
    }

    int result;

    do
    {
        result = calls_.ioctl(fd_, request, argument);
    }
    while (result < 0 && errno == EINTR);

    if (result < 0)
    {
        const int saved_errno = errno;
        setSystemError(
            IghDeviceErrorCode::IOCTL_FAILED,
            saved_errno,
            operation);
        return false;
    }

    clearError();
    return true;
}

const IghDeviceError& IghMasterDevice::error() const noexcept
{
    return error_;
}

bool IghMasterDevice::isOpen() const noexcept
{
    return fd_ >= 0;
}

void IghMasterDevice::clearError()
{
    error_ = {};
}

void IghMasterDevice::setSystemError(
    IghDeviceErrorCode code,
    int system_errno,
    const std::string& operation)
{
    error_.code = code;
    error_.system_errno = system_errno;
    error_.operation = operation;
    error_.message =
        operation + ": " + std::strerror(system_errno);
}
