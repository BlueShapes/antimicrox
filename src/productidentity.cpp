/* AntiMicroX-Delta product identity
 * Copyright (C) 2026 BlueShapes contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "productidentity.h"

#include <QRegularExpression>

namespace
{
struct ParsedRelease
{
    QVersionNumber number;
    QString prerelease;
};

ParsedRelease parseRelease(const QString &text)
{
    static const QRegularExpression releaseTag(
        QStringLiteral(R"(^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z][0-9A-Za-z.-]*))?$)"));
    const QRegularExpressionMatch match = releaseTag.match(text.trimmed());
    if (!match.hasMatch())
        return {};

    return {{match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt()}, match.captured(4)};
}
} // namespace

namespace ProductIdentity
{
const QString version =
    QStringLiteral("%1.%2.%3").arg(PROJECT_MAJOR_VERSION).arg(PROJECT_MINOR_VERSION).arg(PROJECT_PATCH_VERSION)
    + (QStringLiteral(PROJECT_PRERELEASE_VERSION).isEmpty()
           ? QString()
           : QStringLiteral("-") + QStringLiteral(PROJECT_PRERELEASE_VERSION));

QVersionNumber normalizedReleaseVersion(const QString &tag)
{
    return parseRelease(tag).number;
}

bool isNewerRelease(const QString &tag)
{
    return isNewerRelease(tag, version);
}

bool isNewerRelease(const QString &tag, const QString &installedVersion)
{
    const QString trimmedTag = tag.trimmed();
    if (!trimmedTag.startsWith(QLatin1Char('v')))
        return false;

    const ParsedRelease candidate = parseRelease(trimmedTag);
    const ParsedRelease installed = parseRelease(installedVersion);
    if (candidate.number.isNull() || installed.number.isNull())
        return false;

    const int numericComparison = QVersionNumber::compare(candidate.number, installed.number);
    if (numericComparison != 0)
        return numericComparison > 0;

    return candidate.prerelease.isEmpty() && !installed.prerelease.isEmpty();
}
} // namespace ProductIdentity
