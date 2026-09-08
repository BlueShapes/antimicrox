#include "applicationsecuritypolicy.h"
#include "profilefilesizepolicy.h"
#include "profilexmlsafety.h"

#include <QFile>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <iostream>

#ifdef _WIN32
    #include "windowstokenelevation.h"
#elif defined(Q_OS_UNIX)
    #include <sys/stat.h>
#endif

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

void testProfileXmlSafety()
{
    {
        QTemporaryFile profile;
        expect(profile.open(), "a normal temporary profile can be created");
        profile.write("<?xml version=\"1.0\"?><unsupported/>");
        profile.flush();
        profile.close();

        QXmlStreamReader reader;
        expect(ProfileXmlSafety::prepareReader(reader, profile), "a DTD-free XML profile reaches its root element");
        expect(reader.isStartElement(), "the safe profile reader is positioned at the root element");
        expect(!profile.isOpen(), "profile parsing uses a closed, bounded snapshot");
    }

    {
        QTemporaryFile profile;
        expect(profile.open(), "an oversized temporary profile can be created");
        expect(profile.resize(ProfileFileSizePolicy::maximumBytes + 1), "the oversized profile can be resized");
        profile.flush();
        profile.close();

        QXmlStreamReader reader;
        expect(!ProfileXmlSafety::prepareReader(reader, profile), "a profile above the byte limit is rejected");
        expect(reader.hasError(), "oversized profile rejection records a parser error");
    }

    {
        QTemporaryFile profile;
        expect(profile.open(), "a malicious temporary profile can be created");
        profile.write("<!DOCTYPE joystick [<!ENTITY payload \"AAAAAAAA\">]><joystick><name>&payload;</name></joystick>");
        profile.flush();
        profile.close();

        QXmlStreamReader reader;
        expect(!ProfileXmlSafety::prepareReader(reader, profile), "a profile containing an internal DTD is rejected");
        expect(reader.errorString().contains("DTD"), "DTD rejection has an explicit diagnostic");

        reader.clear();
        expect(!ProfileXmlSafety::prepareReader(reader, profile), "re-reading cannot bypass DTD rejection");
    }

#ifdef Q_OS_UNIX
    {
        QTemporaryDir directory;
        expect(directory.isValid(), "a temporary directory for a FIFO profile can be created");
        const QString fifoPath = directory.filePath("profile.fifo");
        const QByteArray nativePath = QFile::encodeName(fifoPath);
        expect(::mkfifo(nativePath.constData(), 0600) == 0, "a FIFO profile can be created");

        QFile profile(fifoPath);
        QXmlStreamReader reader;
        expect(!ProfileXmlSafety::prepareReader(reader, profile), "a FIFO profile is rejected without blocking");
    }
#endif
}

#ifdef _WIN32
bool fakeOpenSucceeds = false;
bool fakeQuerySucceeds = false;
bool fakeElevated = false;
bool fakeHandleClosed = false;

HANDLE WINAPI fakeCurrentProcess() { return reinterpret_cast<HANDLE>(1); }

BOOL WINAPI fakeOpenProcessToken(HANDLE, DWORD, PHANDLE token)
{
    if (!fakeOpenSucceeds)
        return FALSE;
    *token = reinterpret_cast<HANDLE>(2);
    return TRUE;
}

BOOL WINAPI fakeGetTokenInformation(HANDLE, TOKEN_INFORMATION_CLASS, LPVOID information, DWORD, PDWORD returnedLength)
{
    if (!fakeQuerySucceeds)
        return FALSE;
    auto *elevation = static_cast<TOKEN_ELEVATION *>(information);
    elevation->TokenIsElevated = fakeElevated ? TRUE : FALSE;
    *returnedLength = sizeof(TOKEN_ELEVATION);
    return TRUE;
}

BOOL WINAPI fakeCloseHandle(HANDLE)
{
    fakeHandleClosed = true;
    return TRUE;
}

void testWindowsTokenElevation()
{
    const WindowsTokenElevation::Api api = {
        &fakeOpenProcessToken, &fakeGetTokenInformation, &fakeCloseHandle, &fakeCurrentProcess};

    fakeOpenSucceeds = false;
    fakeHandleClosed = false;
    expect(WindowsTokenElevation::query(api) == WindowsTokenElevation::State::Unknown,
           "failure to open the process token fails closed");
    expect(!fakeHandleClosed, "an unopened token is not closed");

    fakeOpenSucceeds = true;
    fakeQuerySucceeds = false;
    fakeHandleClosed = false;
    expect(WindowsTokenElevation::query(api) == WindowsTokenElevation::State::Unknown,
           "failure to query token elevation fails closed");
    expect(fakeHandleClosed, "a token is closed after a failed elevation query");

    fakeQuerySucceeds = true;
    fakeElevated = false;
    fakeHandleClosed = false;
    expect(WindowsTokenElevation::query(api) == WindowsTokenElevation::State::NotElevated,
           "a standard-user token is detected");
    expect(fakeHandleClosed, "a standard-user token handle is closed");

    fakeElevated = true;
    fakeHandleClosed = false;
    expect(WindowsTokenElevation::query(api) == WindowsTokenElevation::State::Elevated,
           "an elevated token is detected");
    expect(fakeHandleClosed, "an elevated token handle is closed");
}
#endif
} // namespace

int main()
{
    using ElevationState = ApplicationSecurityPolicy::ElevationState;

    const ApplicationSecurityPolicy normalInstalled(ElevationState::NotElevated, false);
    expect(!normalInstalled.isElevated(), "normal installed build is not elevated");
    expect(!normalInstalled.isPortablePackage(), "normal installed build is not portable");
    expect(normalInstalled.allowsUserControlledLogFile(), "normal mode permits file logging");
    expect(normalInstalled.allowsProfileProgramExecution(), "normal mode permits profile program execution");
    expect(normalInstalled.allowsProfileMigrationWriteback(), "normal mode permits profile migration writeback");
    expect(!normalInstalled.allowsElevationRequest(), "installed build does not offer full-process elevation");

    const ApplicationSecurityPolicy elevatedInstalled(ElevationState::Elevated, false);
    expect(elevatedInstalled.isElevated(), "elevated installed build reports elevation");
    expect(!elevatedInstalled.allowsUserControlledLogFile(), "elevated mode blocks file logging");
    expect(!elevatedInstalled.allowsProfileProgramExecution(), "elevated mode blocks profile program execution");
    expect(!elevatedInstalled.allowsProfileMigrationWriteback(), "elevated mode blocks profile migration writeback");
    expect(!elevatedInstalled.allowsElevationRequest(), "an elevated process cannot request elevation again");

    const ApplicationSecurityPolicy normalPortable(ElevationState::NotElevated, true);
    expect(normalPortable.isPortablePackage(), "portable build reports portable packaging");
    expect(!normalPortable.allowsElevationRequest(), "portable build blocks elevation requests");
    expect(normalPortable.allowsUserControlledLogFile(), "normal portable mode retains normal logging behavior");

    const ApplicationSecurityPolicy elevatedPortable(ElevationState::Elevated, true);
    expect(elevatedPortable.mustRefuseStartup(), "elevated portable build must refuse startup");
    expect(elevatedInstalled.mustRefuseStartup(), "elevated installed build must refuse startup");
    expect(!normalPortable.mustRefuseStartup(), "normal portable build may start");

    const ApplicationSecurityPolicy unknownInstalled(ElevationState::Unknown, false);
    expect(!unknownInstalled.hasReliableElevationState(), "an unknown elevation state is reported as unreliable");
    expect(!unknownInstalled.allowsUserControlledLogFile(), "unknown elevation blocks file logging");
    expect(!unknownInstalled.allowsProfileProgramExecution(), "unknown elevation blocks profile program execution");
    expect(!unknownInstalled.allowsProfileMigrationWriteback(), "unknown elevation blocks profile migration writeback");
    expect(!unknownInstalled.allowsElevationRequest(), "unknown elevation blocks a new elevation request");
    expect(unknownInstalled.mustRefuseStartup(), "unknown installed build must refuse startup");

    const ApplicationSecurityPolicy unknownPortable(ElevationState::Unknown, true);
    expect(unknownPortable.mustRefuseStartup(), "unknown portable build must refuse startup");

    expect(ProfileFileSizePolicy::isAllowed(0), "an empty profile is within the size limit");
    expect(ProfileFileSizePolicy::isAllowed(ProfileFileSizePolicy::maximumBytes),
           "a profile at the size limit is accepted");
    expect(!ProfileFileSizePolicy::isAllowed(ProfileFileSizePolicy::maximumBytes + 1),
           "a profile above the size limit is rejected");
    expect(!ProfileFileSizePolicy::isAllowed(-1), "an unreadable profile size is rejected");

    testProfileXmlSafety();

#ifdef _WIN32
    testWindowsTokenElevation();
#endif

    if (failures != 0)
    {
        std::cerr << failures << " security policy test(s) failed\n";
        return 1;
    }

    std::cout << "All security policy tests passed\n";
    return 0;
}
