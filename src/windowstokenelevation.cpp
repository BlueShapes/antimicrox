#include "windowstokenelevation.h"

#ifdef _WIN32

WindowsTokenElevation::State WindowsTokenElevation::query(const Api &api) noexcept
{
    HANDLE token = nullptr;
    if (!api.openProcessToken(api.getCurrentProcess(), TOKEN_QUERY, &token))
        return State::Unknown;

    TOKEN_ELEVATION elevation = {};
    DWORD returnedLength = 0;
    const bool tokenRead =
        api.getTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returnedLength) != FALSE;
    api.closeHandle(token);

    if (!tokenRead)
        return State::Unknown;

    return elevation.TokenIsElevated ? State::Elevated : State::NotElevated;
}

WindowsTokenElevation::State WindowsTokenElevation::current() noexcept
{
    const Api api = {&OpenProcessToken, &GetTokenInformation, &CloseHandle, &GetCurrentProcess};
    return query(api);
}

#endif // _WIN32
