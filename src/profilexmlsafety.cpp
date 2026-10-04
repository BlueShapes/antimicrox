/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2026 AntiMicroX contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "profilexmlsafety.h"

#include "profilefilesizepolicy.h"

#include <QByteArray>
#include <QFile>
#include <QXmlStreamReader>
#include <QtGlobal>

#ifdef Q_OS_UNIX
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <unistd.h>
#elif defined(Q_OS_WIN)
    #include <fcntl.h>
    #include <io.h>
    #include <windows.h>
#endif

namespace {
bool openRegularProfile(QFile &file)
{
#ifdef Q_OS_UNIX
    if (!file.isOpen())
    {
        int flags = O_RDONLY | O_NONBLOCK;
    #ifdef O_CLOEXEC
        flags |= O_CLOEXEC;
    #endif
        const QByteArray nativePath = QFile::encodeName(file.fileName());
        const int descriptor = ::open(nativePath.constData(), flags);
        if (descriptor < 0)
            return false;

        if (!file.open(descriptor, QFile::ReadOnly, QFileDevice::AutoCloseHandle))
        {
            ::close(descriptor);
            return false;
        }
    }

    struct stat status = {};
    if (::fstat(file.handle(), &status) != 0 || !S_ISREG(status.st_mode))
    {
        file.close();
        return false;
    }
#else
    #ifdef Q_OS_WIN
    if (!file.isOpen())
    {
        // Do not follow a final reparse point. A competing replacement with a
        // symlink to a pipe/device is opened as the reparse point itself and
        // rejected by the same-handle attribute check below.
        HANDLE handle =
            CreateFileW(reinterpret_cast<LPCWSTR>(file.fileName().utf16()), GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;

        const int descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_RDONLY | _O_BINARY);
        if (descriptor == -1)
        {
            CloseHandle(handle);
            return false;
        }

        if (!file.open(descriptor, QFile::ReadOnly, QFileDevice::AutoCloseHandle))
        {
            _close(descriptor);
            return false;
        }
    }

    const intptr_t nativeHandle = _get_osfhandle(file.handle());
    BY_HANDLE_FILE_INFORMATION information = {};
    const bool isRegularDiskFile =
        nativeHandle != -1 && GetFileType(reinterpret_cast<HANDLE>(nativeHandle)) == FILE_TYPE_DISK &&
        GetFileInformationByHandle(reinterpret_cast<HANDLE>(nativeHandle), &information) != FALSE &&
        (information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT)) ==
            0;
    if (!isRegularDiskFile)
    {
        file.close();
        return false;
    }
    #else
    if (!file.isOpen() && !file.open(QFile::ReadOnly | QFile::Text))
        return false;
    #endif
#endif

    if (file.isSequential())
    {
        file.close();
        return false;
    }

    return true;
}
} // namespace

bool ProfileXmlSafety::prepareReader(QXmlStreamReader &reader, QFile &file)
{
    reader.clear();

    if (!ProfileFileSizePolicy::isAllowed(file.size()))
    {
        reader.raiseError(QStringLiteral("Profile exceeds the %1 MiB size limit.")
                              .arg(ProfileFileSizePolicy::maximumBytes / (1024 * 1024)));
        file.close();
        return false;
    }

    if (!openRegularProfile(file))
    {
        reader.raiseError(QStringLiteral("Profile must be a regular readable file."));
        file.close();
        return false;
    }

    if (!file.seek(0))
    {
        reader.raiseError(QStringLiteral("Could not seek to the start of the profile file."));
        file.close();
        return false;
    }

    const QByteArray profileData = file.read(ProfileFileSizePolicy::maximumBytes + 1);
    file.close();
    if (!ProfileFileSizePolicy::isAllowed(profileData.size()))
    {
        reader.raiseError(QStringLiteral("Profile exceeds the %1 MiB size limit.")
                              .arg(ProfileFileSizePolicy::maximumBytes / (1024 * 1024)));
        return false;
    }

    reader.addData(profileData);

    while (!reader.atEnd())
    {
        const QXmlStreamReader::TokenType token = reader.readNext();

        if (token == QXmlStreamReader::DTD)
        {
            reader.raiseError(QStringLiteral("DTD declarations are not allowed in profile files."));
            return false;
        }

        if (token == QXmlStreamReader::StartElement)
            return true;
    }

    if (!reader.hasError())
        reader.raiseError(QStringLiteral("Profile does not contain a root element."));

    return false;
}
