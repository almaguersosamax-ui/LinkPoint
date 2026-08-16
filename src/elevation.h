#pragma once

#include <QCoreApplication>
#include <QString>

#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

inline bool isElevated()
{
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)
        && elevation.TokenIsElevated;
    CloseHandle(token);
    return ok;
#else
    return true;
#endif
}

inline bool relaunchAsAdmin()
{
#ifdef Q_OS_WIN
    const std::wstring commandLine = QCoreApplication::applicationFilePath().toStdWString();
    const INT_PTR result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"runas", commandLine.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
#else
    return false;
#endif
}
