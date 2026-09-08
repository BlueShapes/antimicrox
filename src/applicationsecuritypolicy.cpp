#include "applicationsecuritypolicy.h"

#include <QtGlobal>

#if defined(Q_OS_WIN)
    #include "winextras.h"
#endif

const ApplicationSecurityPolicy &ApplicationSecurityPolicy::current() noexcept
{
#if defined(Q_OS_WIN)
    static const bool elevated = WinExtras::IsRunningAsAdmin();
#else
    constexpr bool elevated = false;
#endif

#if defined(WIN_PORTABLE_PACKAGE)
    constexpr bool portablePackage = true;
#else
    constexpr bool portablePackage = false;
#endif

    static const ApplicationSecurityPolicy policy(elevated, portablePackage);
    return policy;
}
