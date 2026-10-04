/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2026 AntiMicroX contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PROFILEXMLSAFETY_H
#define PROFILEXMLSAFETY_H

class QFile;
class QXmlStreamReader;

namespace ProfileXmlSafety {
/**
 * Validate and read a bounded snapshot of a regular profile file, leaving the
 * reader positioned on its root start element. Profile DTDs are intentionally
 * unsupported: allowing them would permit attacker-controlled entity
 * expansion before normal profile validation runs.
 */
bool prepareReader(QXmlStreamReader &reader, QFile &file);
} // namespace ProfileXmlSafety

#endif // PROFILEXMLSAFETY_H
