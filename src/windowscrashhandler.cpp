/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2026 AntiMicroX contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "windowscrashhandler.h"

#include <windows.h>
#include <dbghelp.h>
#include <strsafe.h>

#include <cwchar>
#include <exception>

namespace
{
constexpr size_t pathCapacity = 32768;
constexpr size_t reservedFilenameCharacters = 96;
constexpr unsigned maximumRetainedDumpsBeforeCrash = 4;
wchar_t crashDirectory[pathCapacity] = {};
wchar_t dumpPath[pathCapacity] = {};
volatile LONG dumpInProgress = 0;
volatile LONG dumpSequence = 0;

bool copyAbsolutePath(const wchar_t *source, wchar_t (&destination)[pathCapacity]) noexcept
{
    if (source == nullptr || source[0] == L'\0')
        return false;

    const DWORD length = GetFullPathNameW(source, static_cast<DWORD>(pathCapacity), destination, nullptr);
    return length > 0 && length < pathCapacity - reservedFilenameCharacters;
}

bool createDirectory(const wchar_t *path) noexcept
{
    const DWORD attributes = GetFileAttributesW(path);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

    return CreateDirectoryW(path, nullptr) != FALSE || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool createDirectoryTree(const wchar_t *path) noexcept
{
    wchar_t partial[pathCapacity] = {};
    if (FAILED(StringCchCopyW(partial, pathCapacity, path)))
        return false;

    size_t start = 0;
    if (partial[0] != L'\0' && partial[1] == L':')
    {
        start = 3;
    }
    else if (partial[0] == L'\\' && partial[1] == L'\\')
    {
        wchar_t *serverEnd = std::wcschr(partial + 2, L'\\');
        wchar_t *shareEnd = serverEnd == nullptr ? nullptr : std::wcschr(serverEnd + 1, L'\\');
        if (shareEnd == nullptr)
            return createDirectory(partial);
        start = static_cast<size_t>(shareEnd - partial + 1);
    }

    for (size_t index = start; partial[index] != L'\0'; ++index)
    {
        if (partial[index] != L'\\' && partial[index] != L'/')
            continue;

        const wchar_t separator = partial[index];
        partial[index] = L'\0';
        if (partial[0] != L'\0' && !createDirectory(partial))
            return false;
        partial[index] = separator;
    }

    return createDirectory(partial);
}

bool appendPathComponent(wchar_t (&path)[pathCapacity], const wchar_t *component) noexcept
{
    const size_t length = std::wcslen(path);
    if (length == 0 || length >= pathCapacity - reservedFilenameCharacters)
        return false;

    if (path[length - 1] != L'\\' && path[length - 1] != L'/')
    {
        if (FAILED(StringCchCatW(path, pathCapacity, L"\\")))
            return false;
    }
    return SUCCEEDED(StringCchCatW(path, pathCapacity, component));
}

bool pruneOldCrashDumps(const wchar_t *directory) noexcept
{
    wchar_t searchPattern[pathCapacity] = {};
    if (FAILED(StringCchPrintfW(searchPattern, pathCapacity, L"%s\\antimicrox-delta-crash-*.dmp", directory)))
        return false;

    for (;;)
    {
        WIN32_FIND_DATAW data = {};
        HANDLE search = FindFirstFileW(searchPattern, &data);
        if (search == INVALID_HANDLE_VALUE)
            return GetLastError() == ERROR_FILE_NOT_FOUND;

        unsigned count = 0;
        bool haveOldest = false;
        FILETIME oldestTime = {};
        wchar_t oldestName[MAX_PATH] = {};
        do
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                continue;

            ++count;
            if (!haveOldest || CompareFileTime(&data.ftLastWriteTime, &oldestTime) < 0)
            {
                haveOldest = true;
                oldestTime = data.ftLastWriteTime;
                if (FAILED(StringCchCopyW(oldestName, MAX_PATH, data.cFileName)))
                {
                    FindClose(search);
                    return false;
                }
            }
        } while (FindNextFileW(search, &data) != FALSE);
        FindClose(search);

        if (count <= maximumRetainedDumpsBeforeCrash)
            return true;

        wchar_t oldestPath[pathCapacity] = {};
        if (!haveOldest
            || FAILED(StringCchPrintfW(oldestPath, pathCapacity, L"%s\\%s", directory, oldestName))
            || DeleteFileW(oldestPath) == FALSE)
        {
            return false;
        }
    }
}

bool selectDefaultDirectory(wchar_t (&directory)[pathCapacity]) noexcept
{
#if defined(WIN_PORTABLE_PACKAGE)
    const DWORD length = GetModuleFileNameW(nullptr, directory, static_cast<DWORD>(pathCapacity));
    if (length == 0 || length >= pathCapacity - reservedFilenameCharacters)
        return false;

    wchar_t *lastSeparator = std::wcsrchr(directory, L'\\');
    if (lastSeparator == nullptr)
        return false;
    *lastSeparator = L'\0';
    return appendPathComponent(directory, L"crashes");
#else
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, static_cast<DWORD>(pathCapacity));
    if (length > 0 && length < pathCapacity - reservedFilenameCharacters
        && appendPathComponent(directory, L"antimicrox-delta") && appendPathComponent(directory, L"crashes"))
    {
        return true;
    }

    length = GetTempPathW(static_cast<DWORD>(pathCapacity), directory);
    if (length == 0 || length >= pathCapacity - reservedFilenameCharacters)
        return false;
    return appendPathComponent(directory, L"antimicrox-delta-crashes");
#endif
}

bool writeMiniDump(EXCEPTION_POINTERS *exceptionPointers) noexcept
{
    if (InterlockedCompareExchange(&dumpInProgress, 1, 0) != 0)
        return false;

    SYSTEMTIME timestamp = {};
    GetSystemTime(&timestamp);
    const DWORD processId = GetCurrentProcessId();
    const LONG sequence = InterlockedIncrement(&dumpSequence);

    bool dumpWritten = false;
    const HRESULT formatResult = StringCchPrintfW(
        dumpPath, pathCapacity, L"%s\\antimicrox-delta-crash-%04u%02u%02u-%02u%02u%02u-%03u-%lu-%ld.dmp", crashDirectory,
        timestamp.wYear, timestamp.wMonth, timestamp.wDay, timestamp.wHour, timestamp.wMinute, timestamp.wSecond,
        timestamp.wMilliseconds, static_cast<unsigned long>(processId), static_cast<long>(sequence));

    if (SUCCEEDED(formatResult))
    {
        HANDLE dumpFile = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (dumpFile != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION exceptionInformation = {};
            exceptionInformation.ThreadId = GetCurrentThreadId();
            exceptionInformation.ExceptionPointers = exceptionPointers;
            exceptionInformation.ClientPointers = FALSE;
            MINIDUMP_EXCEPTION_INFORMATION *exceptionInformationPointer =
                exceptionPointers == nullptr ? nullptr : &exceptionInformation;

            const MINIDUMP_TYPE detailedType = static_cast<MINIDUMP_TYPE>(
                MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
            BOOL written = MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFile, detailedType,
                                             exceptionInformationPointer, nullptr, nullptr);
            if (written == FALSE)
            {
                LARGE_INTEGER beginning = {};
                if (SetFilePointerEx(dumpFile, beginning, nullptr, FILE_BEGIN) != FALSE && SetEndOfFile(dumpFile) != FALSE)
                {
                    written = MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFile, MiniDumpNormal,
                                                exceptionInformationPointer, nullptr, nullptr);
                }
            }

            FlushFileBuffers(dumpFile);
            CloseHandle(dumpFile);
            dumpWritten = written != FALSE;
            if (written == FALSE)
                DeleteFileW(dumpPath);
        }
    }

    return dumpWritten;
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS *exceptionPointers) noexcept
{
    return writeMiniDump(exceptionPointers) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

[[noreturn]] void WindowsCrashHandler::terminateWithDump() noexcept
{
    writeMiniDump(nullptr);
    TerminateProcess(GetCurrentProcess(), 3);
    ExitProcess(3);
}

bool WindowsCrashHandler::install(const wchar_t *directoryOverride) noexcept
{
    wchar_t selectedDirectory[pathCapacity] = {};
    const bool selected = directoryOverride != nullptr ? copyAbsolutePath(directoryOverride, selectedDirectory)
                                                        : selectDefaultDirectory(selectedDirectory);
    if (!selected || !createDirectoryTree(selectedDirectory) || !pruneOldCrashDumps(selectedDirectory))
        return false;

    if (FAILED(StringCchCopyW(crashDirectory, pathCapacity, selectedDirectory)))
        return false;

    InterlockedExchange(&dumpInProgress, 0);
    SetUnhandledExceptionFilter(&unhandledExceptionFilter);
    std::set_terminate(&WindowsCrashHandler::terminateWithDump);
    return true;
}

bool WindowsCrashHandler::writeDump() noexcept { return writeMiniDump(nullptr); }
