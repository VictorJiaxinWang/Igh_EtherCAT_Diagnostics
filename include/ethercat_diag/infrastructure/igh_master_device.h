#pragma once

#include <cstdint>
#include <functional>
#include <string>

enum class IghDeviceAccess
{
    READ_ONLY,
    READ_WRITE
};

enum class IghDeviceErrorCode
{
    NONE,
    INVALID_MASTER_INDEX,
    OPEN_FAILED,
    IOCTL_FAILED,
    ABI_MISMATCH,
    INVALID_DATA
};

struct IghDeviceError
{
    IghDeviceErrorCode code{IghDeviceErrorCode::NONE};
    int system_errno{};
    std::string operation;
    std::string message;
    std::uint32_t expected_magic{};
    std::uint32_t actual_magic{};
};

struct LinuxCalls
{
    std::function<int(const char*, int)> open;
    std::function<int(int, unsigned long, void*)> ioctl;
    std::function<int(int)> close;
};

LinuxCalls productionLinuxCalls();

class IghMasterDevice
{
public:
    explicit IghMasterDevice(
        LinuxCalls calls = productionLinuxCalls());

    ~IghMasterDevice();

    IghMasterDevice(const IghMasterDevice&) = delete;
    IghMasterDevice& operator=(const IghMasterDevice&) = delete;

    bool openAndValidate(
        int master_index,
        IghDeviceAccess access);

    bool call(
        unsigned long request,
        void* argument,
        const std::string& operation);

    const IghDeviceError& error() const noexcept;
    bool isOpen() const noexcept;

private:
    void clearError();
    void setSystemError(
        IghDeviceErrorCode code,
        int system_errno,
        const std::string& operation);

    LinuxCalls calls_;
    int fd_{-1};
    IghDeviceError error_;
};
