#include "productidentity.h"
#include "settingsmigration.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>

namespace
{
// Capture the version before main, as common.h does in production translation units.
const QString startupVersion = ProductIdentity::version;
int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testPublicIdentity()
{
    expect(ProductIdentity::displayName == QStringLiteral("AntiMicroX-Delta"), "the display name is Delta-specific");
    expect(ProductIdentity::applicationName == QStringLiteral("antimicrox-delta"),
           "the application name is Delta-specific");
    expect(ProductIdentity::version == QStringLiteral("1.1.0"), "the public version is 1.1.0");
    expect(startupVersion == ProductIdentity::version, "the version is available during static initialization");
    expect(ProductIdentity::upstreamBaseVersion == QStringLiteral("3.6.1"),
           "the upstream base version remains separately identifiable");
    expect(ProductIdentity::projectUrl == QStringLiteral("https://github.com/BlueShapes/antimicrox/"),
           "the project URL targets the Delta repository");
    expect(ProductIdentity::releaseApiUrl
               == QStringLiteral("https://api.github.com/repos/BlueShapes/antimicrox/releases/latest"),
           "the release API targets the Delta repository");
    expect(ProductIdentity::desktopId == QStringLiteral("io.github.blueshapes.antimicrox_delta"),
           "the desktop ID cannot collide with upstream");
    expect(ProductIdentity::dbusService == QStringLiteral("io.github.blueshapes.AntiMicroXDelta"),
           "the D-Bus service cannot collide with upstream");
    expect(ProductIdentity::localSocketKey == QStringLiteral("antimicroxDeltaSignalListener"),
           "the local socket cannot collide with upstream");
    expect(ProductIdentity::configDirectoryName == QStringLiteral("antimicrox-delta"),
           "the settings directory cannot collide with upstream");
    expect(ProductIdentity::configFileName == QStringLiteral("antimicrox-delta_settings.ini"),
           "the settings file cannot collide with upstream");
}

void testReleaseVersionComparison()
{
    expect(ProductIdentity::normalizedReleaseVersion(QStringLiteral("v1.2.3")) == QVersionNumber(1, 2, 3),
           "a conventional v-prefixed tag is accepted");
    expect(ProductIdentity::normalizedReleaseVersion(QStringLiteral("1.2.3")) == QVersionNumber(1, 2, 3),
           "a bare version tag remains accepted");
    expect(ProductIdentity::normalizedReleaseVersion(QStringLiteral("v1.2.3-rc.1")) == QVersionNumber(1, 2, 3),
           "a prerelease suffix is ignored for numeric comparison");
    expect(ProductIdentity::normalizedReleaseVersion(QStringLiteral("release-1.2.3")).isNull(),
           "an unrelated tag format is rejected");
    expect(ProductIdentity::isNewerRelease(QStringLiteral("v1.1.1")), "a greater Delta version is an update");
    expect(!ProductIdentity::isNewerRelease(QStringLiteral("v1.1.0")), "the installed version is not an update");
    expect(!ProductIdentity::isNewerRelease(QStringLiteral("v0.9.9")), "an older version is not an update");
    expect(!ProductIdentity::isNewerRelease(QStringLiteral("3.7.0")),
           "the legacy fork version cannot supersede the independent Delta series");
    expect(ProductIdentity::isNewerRelease(QStringLiteral("v1.0.0"), QStringLiteral("1.0.0-rc.1")),
           "a stable release supersedes the prerelease with the same numeric version");
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

void testSettingsMigration()
{
    QTemporaryDir temporaryDirectory;
    expect(temporaryDirectory.isValid(), "a temporary settings directory can be created");
    if (!temporaryDirectory.isValid())
        return;

    const QString firstSource = QDir(temporaryDirectory.path()).filePath(QStringLiteral("missing.ini"));
    const QString secondSource = QDir(temporaryDirectory.path()).filePath(QStringLiteral("upstream/settings.ini"));
    const QString destination = QDir(temporaryDirectory.path()).filePath(QStringLiteral("delta/settings.ini"));
    expect(QDir().mkpath(QFileInfo(secondSource).absolutePath()), "the upstream settings directory can be created");
    expect(writeFile(secondSource, QByteArrayLiteral("upstream=true\n")), "upstream settings can be prepared");

    const SettingsMigration::Outcome copied =
        SettingsMigration::copyFirstAvailable({firstSource, secondSource}, destination);
    expect(copied.result == SettingsMigration::Result::Copied, "the first available regular settings file is copied");
    expect(QFileInfo(copied.sourcePath).canonicalFilePath() == QFileInfo(secondSource).canonicalFilePath(),
           "the migration reports the source it copied");
    expect(readFile(destination) == QByteArrayLiteral("upstream=true\n"), "the copied Delta settings are intact");
    expect(readFile(secondSource) == QByteArrayLiteral("upstream=true\n"), "the upstream settings remain intact");

    expect(writeFile(destination, QByteArrayLiteral("delta=true\n")), "independent Delta settings can be prepared");
    const SettingsMigration::Outcome preserved =
        SettingsMigration::copyFirstAvailable({secondSource}, destination);
    expect(preserved.result == SettingsMigration::Result::DestinationExists,
           "existing Delta settings prevent another migration");
    expect(readFile(destination) == QByteArrayLiteral("delta=true\n"), "existing Delta settings are never overwritten");

    const QString absentDestination =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("another-delta/settings.ini"));
    const SettingsMigration::Outcome noSource =
        SettingsMigration::copyFirstAvailable({firstSource}, absentDestination);
    expect(noSource.result == SettingsMigration::Result::NoSource, "missing settings sources are harmless");
    expect(!QFileInfo::exists(absentDestination), "no destination is created when no source exists");

    const QString blockedDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("blocked"));
    const QString blockedDestination = QDir(blockedDirectory).filePath(QStringLiteral("settings.ini"));
    expect(writeFile(blockedDirectory, QByteArrayLiteral("not a directory")), "a blocked destination can be prepared");
    const SettingsMigration::Outcome failed =
        SettingsMigration::copyFirstAvailable({secondSource}, blockedDestination);
    expect(failed.result == SettingsMigration::Result::CopyFailed, "an I/O failure is reported without a destination");
    expect(!QFileInfo::exists(blockedDestination), "a failed migration leaves no partial settings file");

    expect(QFile::remove(blockedDirectory), "the blocked destination can be repaired");
    const SettingsMigration::Outcome retried =
        SettingsMigration::copyFirstAvailable({secondSource}, blockedDestination);
    expect(retried.result == SettingsMigration::Result::Copied, "a failed migration can be retried successfully");
    expect(readFile(blockedDestination) == QByteArrayLiteral("upstream=true\n"),
           "the retried migration produces complete settings");

    const QString linkedSource = QDir(temporaryDirectory.path()).filePath(QStringLiteral("linked-settings.ini"));
    if (QFile::link(secondSource, linkedSource) && QFileInfo(linkedSource).isSymLink())
    {
        const QString linkedDestination =
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("linked-delta/settings.ini"));
        const SettingsMigration::Outcome rejectedLink =
            SettingsMigration::copyFirstAvailable({linkedSource}, linkedDestination);
        expect(rejectedLink.result == SettingsMigration::Result::NoSource,
               "a symbolic-link settings source is not imported");
        expect(!QFileInfo::exists(linkedDestination), "a rejected symbolic link creates no Delta settings");
    }
}
} // namespace

int main()
{
    testPublicIdentity();
    testReleaseVersionComparison();
    testSettingsMigration();

    if (failures == 0)
        std::cout << "All product identity tests passed.\n";
    return failures == 0 ? 0 : 1;
}
