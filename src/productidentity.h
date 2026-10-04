/* AntiMicroX-Delta product identity
 * Copyright (C) 2026 BlueShapes contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PRODUCTIDENTITY_H
#define PRODUCTIDENTITY_H

#include "config.h"

#include <QString>
#include <QVersionNumber>

namespace ProductIdentity {
inline const QString displayName{QStringLiteral(PROJECT_PRODUCT_NAME)};
inline const QString applicationName{QStringLiteral(PROJECT_PRODUCT_SLUG)};
inline const QString desktopId{QStringLiteral(PROJECT_DESKTOP_ID)};
inline const QString dbusService{QStringLiteral(PROJECT_DBUS_SERVICE)};
inline const QString dbusInterface{QStringLiteral(PROJECT_DBUS_SERVICE ".InputDevice")};
inline const QString dbusObjectPath{QStringLiteral("/io/github/blueshapes/AntiMicroXDelta/inputdevice")};
inline const QString upstreamBaseVersion{QStringLiteral(PROJECT_UPSTREAM_BASE_VERSION)};
inline const QString projectUrl{QStringLiteral(PROJECT_URL)};
inline const QString issuesUrl{projectUrl + QStringLiteral("issues")};
inline const QString wikiUrl{projectUrl + QStringLiteral("wiki")};
inline const QString releasesUrl{projectUrl + QStringLiteral("releases/latest")};
inline const QString releaseApiUrl{QStringLiteral("https://api.github.com/repos/BlueShapes/antimicrox/releases/latest")};
inline const QString localSocketKey{QStringLiteral("antimicroxDeltaSignalListener")};
inline const QString configDirectoryName{QStringLiteral(PROJECT_PRODUCT_SLUG)};
inline const QString configFileName{QStringLiteral(PROJECT_PRODUCT_SLUG "_settings.ini")};

inline const QString version =
    QStringLiteral("%1.%2.%3").arg(PROJECT_MAJOR_VERSION).arg(PROJECT_MINOR_VERSION).arg(PROJECT_PATCH_VERSION) +
    (QStringLiteral(PROJECT_PRERELEASE_VERSION).isEmpty()
         ? QString()
         : QStringLiteral("-") + QStringLiteral(PROJECT_PRERELEASE_VERSION));

QVersionNumber normalizedReleaseVersion(const QString &tag);
bool isNewerRelease(const QString &tag);
bool isNewerRelease(const QString &tag, const QString &installedVersion);
} // namespace ProductIdentity

#endif // PRODUCTIDENTITY_H
