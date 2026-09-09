# Security Policy

## Supported release policy

The official Windows release is the installer package built with Qt 6.10.2.
AntiMicroX-Delta no longer publishes an official Portable Windows archive.
Windows loads dependent DLLs before application startup code can apply its
security policy, so a portable archive cannot provide the same assurance.
Locally built portable binaries may be used only as standard-user processes.

AntiMicroX-Delta does not support running the full process elevated. On Windows, the
application queries `TOKEN_ELEVATION` before creating `QApplication` and
refuses both elevated and indeterminate token states. On Unix-like systems it
also refuses a root (`euid == 0`) start. The UI does not offer an elevation
action. Existing restrictions on profile program execution, profile migration
write-back, and user-controlled log files remain defense in depth.

## Profiles are active content

Profiles can configure commands and mappings. Only open profiles from sources
you trust, review any configured program action before use, and do not treat a
profile as harmless data. The XML profile reader rejects files larger than
16 MiB and rejects DTD declarations before profile parsing or migration to
reduce memory-exhaustion risk.

## Security reporting

Please report potential vulnerabilities privately through the repository's
security advisory channel when available. Include affected version, platform,
reproduction steps, and impact. Do not publish a working exploit before the
maintainers have had a reasonable opportunity to investigate.

## 1.0.0 security audit notes

Official Windows and AppImage workflows bundle Qt 6.10.2. DEB packages use the
distribution-provided Qt security updates. CVE-2026-15037 concerns QDom
serialization; AntiMicroX-Delta profile processing uses QXmlStreamReader and
QXmlStreamWriter, so that issue does not directly apply to this code path.

Release workflows pin third-party Actions and AppImage build tools to immutable
commits or release assets. Downloaded build executables and the embedded
AppImage runtime are verified against repository-managed SHA-256 digests before
execution.
