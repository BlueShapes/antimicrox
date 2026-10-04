#include "applicationsecuritypolicy.h"

#include <QtGlobal>

#if defined(Q_OS_WIN)
    #include "winextras.h"
#elif defined(Q_OS_UNIX)
    #include <unistd.h>
#endif

const ApplicationSecurityPolicy &ApplicationSecurityPolicy::current() noexcept
{
#if defined(Q_OS_WIN)
    static const ElevationState elevationState = [] {
        switch (WinExtras::elevationState())
        {
        case WinExtras::ElevationState::NotElevated:
            return ElevationState::NotElevated;
        case WinExtras::ElevationState::Elevated:
            return ElevationState::Elevated;
        case WinExtras::ElevationState::Unknown:
            return ElevationState::Unknown;
        }

        return ElevationState::Unknown;
    }();
#elif defined(Q_OS_UNIX)
    static const ElevationState elevationState = geteuid() == 0 ? ElevationState::Elevated : ElevationState::NotElevated;
#else
    constexpr ElevationState elevationState = ElevationState::NotElevated;
#endif

#if defined(WIN_PORTABLE_PACKAGE)
    constexpr bool portablePackage = true;
#else
    constexpr bool portablePackage = false;
#endif

    static const ApplicationSecurityPolicy policy(elevationState, portablePackage);
    return policy;
}
