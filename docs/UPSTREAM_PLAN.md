# Public-repository upstream plan

The contest guide requires changes to public openvela repositories such as
`nuttx` and `nuttx-apps` to be submitted to their `dev-ai-contest-2026`
branches for organizer review. The contest manifest keeps the current tested
integration reproducible, but exposes every public-tree replacement as an
explicit `linkfile`; it does not hide a monolithic patch.

## Contest-owned board series

These new directories form the A733/Cubie A7Z board submission and should
ultimately move to the appropriate Allwinner vendor repository:

- `vendor/allwinnertech/chips/a733`
- `vendor/allwinnertech/boards/a733/cubie-a7z`
- `apps/system/a733wifi`
- `apps/system/a733services`
- `apps/system/a733ftpd`
- `apps/include/system/a733_services.h`

## nuttx-apps PR series

Split by subsystem and keep each commit independently reviewable:

1. A733 Wi-Fi/network integration and NSH command changes.
2. FTP large-file, UTF-8 path, passive-mode and session recovery changes.
3. NTP/DNS timing and retry changes.
4. libssh NuttX initialization, server PTY/NSH, cleanup and client/SCP fixes.
5. curl NuttX configuration required by the target.

Before sending, rebase each file against the current official branch and
remove any workaround that is no longer necessary upstream.

## NuttX PR series

1. ARM64 early/fatal/syscall/task-start changes, with architecture tests.
2. FAT long-name/path fix, with filesystem tests.
3. public A733 VIP2 device ABI header.
4. initialization/build/uname changes, minimized to generic behavior.

Every public PR must include the actual A733 failure evidence, a focused
explanation, tests on another supported target where relevant, and the CLA
required by the project. The contest integration should be updated to the
upstream commits once merged.
