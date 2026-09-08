/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2026 AntiMicroX contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WINDOWSCRASHHANDLER_H
#define WINDOWSCRASHHANDLER_H

namespace WindowsCrashHandler
{
bool install(const wchar_t *directoryOverride = nullptr) noexcept;
bool writeDump() noexcept;
} // namespace WindowsCrashHandler

#endif // WINDOWSCRASHHANDLER_H
