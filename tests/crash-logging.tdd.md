# Crash logging TDD evidence

## Source and user journeys

No separate plan file was supplied. The tests were derived from the requested
Windows minidump generation, bounded logs, and preservation of the final
completed log record during abrupt process termination.

- A Windows user receives a usable minidump after an unhandled exception or Qt fatal error.
- A user keeps recent diagnostics without earlier sessions being erased or logs growing without bound.
- A completed log call remains readable even if the process is terminated immediately afterwards.

## RED/GREEN evidence

| Behavior | RED evidence | GREEN evidence | Guarantee |
|---|---|---|---|
| Append, startup retention, abrupt-exit tail | `ctest --test-dir build -R '^CrashLoggingTests$' --output-on-failure` failed with 3 expected assertions | Same command passed | Existing log content and the last completed record survive; oversized startup logs retain their recent tail |
| Windows exception dump | CMake failed because the new `windowscrashhandler` implementation did not yet exist | Crash probe passed and produced an `MDMP` file | An access violation produces a readable dump with an exception stream and the expected exception code |
| Runtime cap and Qt fatal dump | Focused test failed with 2 expected assertions | Focused test passed | Active logs remain at or below 8 MiB and `qFatal()` produces one dump |
| MSVC Qt fatal termination | GitHub Actions run `34230947800` failed on both MSVC Qt 5 and Qt 6 because the fatal-dump child remained alive for the full 15-second timeout | The rebuilt local Windows suite passed five consecutive runs after replacing the Windows `abort()` path; MSVC CI confirmation remains pending | The owned Windows `qFatal()` path writes its dump and terminates directly instead of entering the MSVC CRT abort/reporting path |
| Fatal path under logger contention | The new contention probe failed because `qFatal()` waited more than 5 seconds for a logger lock and created no dump | The focused suite passed after moving the owned Windows fatal path ahead of ordinary logger locking | A Qt fatal message bypasses a blocked logger, terminates promptly, and still creates one minidump |
| Dump retention | Focused test failed with the expected retention assertion | Focused test passed | Startup retains at most four old AntiMicroX dumps, leaving room for the next dump within a five-dump limit |
| Logger shutdown lifetime | Focused test failed because a post-shutdown log call threw | Focused test passed | New log calls are safely dropped after logger destruction, while in-flight synchronous calls finish first |

Final focused verification command:

```text
ctest --test-dir build -R '^(SecurityPolicyTests|CrashLoggingTests)$' --output-on-failure
```

## Test specification

| What is guaranteed | Test type | Location |
|---|---|---|
| Opening an existing log appends rather than truncating it | Integration | `tests/crashlogging/testloggerdurability.cpp` |
| Startup and active logging retain recent data within 8 MiB | Integration | `tests/crashlogging/testloggerdurability.cpp` |
| Immediate `TerminateProcess` does not lose the last completed log call | Subprocess E2E | `tests/crashlogging/testloggerdurability.cpp` |
| Access violation dump has `MDMP` signature and matching exception stream | Subprocess E2E | `tests/crashlogging/testloggerdurability.cpp` |
| Qt fatal termination creates a dump with an `MDMP` signature | Subprocess E2E | `tests/crashlogging/testloggerdurability.cpp` |
| Qt fatal termination bypasses a logger blocked in another thread | Multithreaded subprocess E2E | `tests/crashlogging/testloggerdurability.cpp` |
| Only AntiMicroX-named old dumps are pruned | Integration | `tests/crashlogging/testloggerdurability.cpp` |
| Logger destruction is synchronized with message submission | Multithreaded integration | `tests/crashlogging/testloggerdurability.cpp` |

## Coverage and known gaps

The project build does not currently enable a coverage-instrumented C++ target,
so no numeric coverage percentage is claimed. The focused suite executes every
requested failure mode. `TerminateProcess`, Windows fail-fast termination, and
power loss cannot invoke an unhandled-exception filter; the synchronous logger
still covers completed log calls for ordinary process termination, while a dump
requires an exception, an owned Qt fatal path, or a C++ terminate path. A raw
third-party `abort()` is not intercepted because invoking DbgHelp from a C++
signal handler is unsafe.
The current Windows implementation writes dumps in-process; severe corruption
or a crash while the Windows loader is locked can still prevent DbgHelp from
completing. A separate reporter process would be required to remove that class
of failure entirely.
On Windows, the owned Qt fatal path deliberately bypasses the ordinary logger
before taking any logger lock. The last previously completed record remains
durable, but the fatal message text itself is not guaranteed to reach the log.
If crash-handler initialization reports failure at startup, the application
continues without its own minidump facility and fatal termination still favors
prompt shutdown over entering the potentially blocking MSVC abort-reporting
path.
