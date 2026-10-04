#ifndef WINDOWSTOKENELEVATION_H
#define WINDOWSTOKENELEVATION_H

#ifdef _WIN32

    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>

namespace WindowsTokenElevation {
enum class State
{
    NotElevated,
    Elevated,
    Unknown,
};

struct Api
{
    decltype(&OpenProcessToken) openProcessToken;
    decltype(&GetTokenInformation) getTokenInformation;
    decltype(&CloseHandle) closeHandle;
    decltype(&GetCurrentProcess) getCurrentProcess;
};

[[nodiscard]] State query(const Api &api) noexcept;
[[nodiscard]] State current() noexcept;
} // namespace WindowsTokenElevation

#endif // _WIN32

#endif // WINDOWSTOKENELEVATION_H
