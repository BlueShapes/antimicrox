#include "applicationsecuritypolicy.h"

#include <iostream>

namespace
{
int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
} // namespace

int main()
{
    const ApplicationSecurityPolicy normalInstalled(false, false);
    expect(!normalInstalled.isElevated(), "normal installed build is not elevated");
    expect(!normalInstalled.isPortablePackage(), "normal installed build is not portable");
    expect(normalInstalled.allowsUserControlledLogFile(), "normal mode permits file logging");
    expect(normalInstalled.allowsProfileProgramExecution(), "normal mode permits profile program execution");
    expect(normalInstalled.allowsProfileMigrationWriteback(), "normal mode permits profile migration writeback");
    expect(normalInstalled.allowsElevationRequest(), "installed build permits an elevation request");

    const ApplicationSecurityPolicy elevatedInstalled(true, false);
    expect(elevatedInstalled.isElevated(), "elevated installed build reports elevation");
    expect(!elevatedInstalled.allowsUserControlledLogFile(), "elevated mode blocks file logging");
    expect(!elevatedInstalled.allowsProfileProgramExecution(), "elevated mode blocks profile program execution");
    expect(!elevatedInstalled.allowsProfileMigrationWriteback(), "elevated mode blocks profile migration writeback");
    expect(elevatedInstalled.allowsElevationRequest(), "installed build remains eligible for elevation requests");

    const ApplicationSecurityPolicy normalPortable(false, true);
    expect(normalPortable.isPortablePackage(), "portable build reports portable packaging");
    expect(!normalPortable.allowsElevationRequest(), "portable build blocks elevation requests");
    expect(normalPortable.allowsUserControlledLogFile(), "normal portable mode retains normal logging behavior");

    const ApplicationSecurityPolicy elevatedPortable(true, true);
    expect(elevatedPortable.mustRefuseStartup(), "elevated portable build must refuse startup");
    expect(!elevatedInstalled.mustRefuseStartup(), "elevated installed build may start in restricted mode");
    expect(!normalPortable.mustRefuseStartup(), "normal portable build may start");

    if (failures != 0)
    {
        std::cerr << failures << " security policy test(s) failed\n";
        return 1;
    }

    std::cout << "All security policy tests passed\n";
    return 0;
}
