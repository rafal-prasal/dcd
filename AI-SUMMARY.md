# AI Summary: tcp-wrappers removal + autotools → CMake migration

Date: 2026-09-28

## Task

1. Remove support for tcp-wrappers entirely (code, documentation, etc.)
2. Change build system from autotools to CMake
3. Make sure that CMake builds the code

## 1. tcp-wrappers removal

### Code (`src/`)

- `pch.h` — removed `#include <tcpd.h>`
- `misc.c` — removed `my_hosts_access()` (used `request_init`/`fromhost`/`hosts_access` from libwrap), `denied()`, `extern pthread_mutex_t mutex_my_hosts_access`, `extern int allow_severity/deny_severity`
- `misc.h` — removed `char denied(int sock)` declaration
- `main.c` — removed `allow_severity`/`deny_severity` definitions and their init, `mutex_my_hosts_access` definition and its `pthread_mutex_init()`, and the `denied()` gate in `adduser()` that disconnected users not listed in hosts.allow
- `console.c` — `if (denied(sock) || !validhost(...))` → `if (!validhost(...))`; dcd's own `console.allow` check is kept
- `compatibility.c` / `compatibility.h` — removed the Red Hat buggy-libwrap `yp_get_default_domain()` workaround (`INCLUDED_YP_GET_DEFAULT_DOMAIN`)
- `config.h` — removed `HAVE_PROCESS_OPTIONS` (libwrap option-processing define)

### Documentation

- `README.in` — removed libwrap feature bullet, `libwrap(tcp_wrappers)` requirement, `--with-libwrap-path` option, and the whole hosts.allow/hosts.deny usage section
- `FAQ` — removed 4 Q&A pairs (EN + PL): libwrap link failure on RedHat, "You're not allowed to connect" (referred to hosts.allow/hosts.deny)
- `TODO` — removed "finish libwrap conditional" entries (EN + PL `ac/am:` sections)
- `doc/dcd.sgml` — removed libwrap feature bullet; requirements list now `libpthread` + `CMake` (was libwrap/automake/autoconf/libtool/gettext)
- `doc/dcd_man.sgml` — removed BUGS refsect ("DCD is not building on RedHat, because there is only static libwrap library")
- `doc/pl/dcd.sgml`, `doc/pl/dcd_man.sgml` — same removals in the Polish docs (files are ISO-8859-2; edited byte-safely)
- `contrib/PLD/dcd.spec.in`, `contrib/MandrakeLinux/dcd.spec.in` — removed `BuildRequires: libwrap-devel` / `Requires: tcp_wrappers` / `tcp_wrappers-devel`; also replaced autoconf/automake/libtool BuildRequires with `cmake` and switched `%build` from autogen/configure to CMake

Historical records (`ChangeLog`, `NEWS`, spec `%changelog` history) were intentionally left untouched.

## 2. Build system: autotools → CMake

### Deleted (autotools machinery + stale generated artifacts)

- Root: `configure`, `configure.ac`, `Makefile.am`, `Makefile.in`, `aclocal.m4`, `acinclude.m4`, `autogen.sh`, `config.h.in`, `ltmain.sh`, `depcomp`, `install-sh`, `missing`, `config.guess`, `config.sub`
- Nested: all `Makefile.am`/`Makefile.in` in `src/`, `conf/`, `doc/`, `doc/pl/`, `contrib/`, `contrib/PLD/`, `contrib/MandrakeLinux/`
- Stale generated files (now produced by CMake at configure time): `README`, `conf/dcd.conf`, `src/dcd.adduser`, `contrib/logrotate.dcd`, `contrib/PLD/dcd.spec`, `contrib/PLD/dcd.init`, `contrib/MandrakeLinux/dcd.spec`, `contrib/MandrakeLinux/dcd.init`

Note: the hand-maintained portability header `config.h` (root) is kept as-is; it is not an autotools artifact.

### Added `CMakeLists.txt`

- `cmake_minimum_required(VERSION 3.10)`, `project(dcd VERSION 1.1.1 LANGUAGES C)`
- Cache variables: `DCD_CONFIG_DIR` (default `<prefix>/etc/dcd`), `DEFAULT_USER`/`DEFAULT_GROUP` (default `daemon`), `LOG_DIR` (default `/var/log/dcd`)
- `find_package(Threads REQUIRED)`; links `Threads::Threads`
- Builds single target `dcd` from all 15 sources in `src/`, `-Wall`
- Compile definitions: `PACKAGE_NAME`, `PACKAGE_VERSION`, `CONFIGDIR`, `DEFAULT_USER`, `DEFAULT_GROUP`, `BUILD_TIME` (UTC timestamp)
- `configure_file(... @ONLY)` for: `README.in`→README, `conf/dcd.conf.in`, `src/dcd.adduser.in`, `contrib/logrotate.dcd.in`, PLD/MandrakeLinux spec + init templates
- Install: `dcd` and `dcd.adduser` to `sbin/`; all 11 config files into `${DCD_CONFIG_DIR}` (mode 640 for `dcd.users` and `console.allow`, 644 otherwise); man pages `doc/dcd.2`, `doc/dcd.console.2` → `share/man/man2`, Polish `doc/pl/dcd.1`, `doc/pl/dcd.console.1` → `share/man/pl/man1`

### Updated `build.sh`

```sh
cmake -B build -S .
cmake --build build
```

## 3. Verification

- Clean `cmake -B build -S .` + `cmake --build build`: succeeds, `[100%] Built target dcd`, only pre-existing warnings (unused vars, misleading indentation)
- `ldd build/dcd`: no libwrap linked
- `./build/dcd -h`: usage screen prints, version banner `DConnect Daemon v1.1.1` OK
- Staged install (`DESTDIR=/tmp/dcd-stage cmake --install build`) produced the expected tree: `usr/local/sbin/{dcd,dcd.adduser}`, `usr/local/etc/dcd/*` (11 config files), man pages in `man2/` and `pl/man1/`

## 4. Code analysis & defect fixes (2026-09-28, follow-up)

Method: GCC `-Wall -Wextra -Wformat=2 -Wshadow`, `-O2` (`-Wstringop-*`, `warn_unused_result`), and GCC static analyzer (`-fanalyzer`). All findings triaged manually; false positives (e.g. the ~25 `leak of *usr.reason` reports in cmd.dc.c — freed later in `disconnect()` via net.c:322) were not touched.

### Real defects fixed

| Location | Defect | Fix |
|---|---|---|
| `src/main.c`, `src/console.c` accept loops | `new_sock` declared `unsigned int` → `accept()<0` check always false; on error, fd `-1` (as 4294967295) passed to `adduser()`/`addcons()` | `int new_sock`, `socklen_t remote_size/size_addr` |
| `src/main.c` (main) | `setuid()`/`setgid()` return values ignored — daemon could keep running as root after failed privilege drop | check both, log + `exit(1)` on failure (setgid before setuid) |
| `src/conf.c` (`parse_hublist`) | file handle from `fopen(conf_hublinks)` never `fclose()`d → fd leak on every config reload | added `fclose(fp)` after the loop |
| `src/cmd.dc.c:232` (`dc_GetNickList`) | stray `;` after `if(!user_tst_supports(...));` made the nick-append block unconditional (dead guard) | removed the semicolon |
| `src/misc.c` `my_duplicate()` | error check tested `!duplicate` (pointer arg, never NULL) instead of `!*duplicate` → strdup OOM undetected, caller later derefs NULL | check `!*duplicate` |
| `src/misc.c` `make_path()` | `if (!**file)` deref before NULL check; unchecked `my_realloc()` failure (which frees the original) left dangling `*file` + `sprintf(NULL,...)` | NULL guard, realloc-failure handling (`*file=NULL`, free temp, return), `tmp` guard after `my_duplicate` |
| `src/misc.c` `my_free1()` | `*ptr=NULL` executed even when `ptr==NULL` → segfault path | moved inside the `if (ptr && *ptr)` guard |
| `src/misc.c` `flag_myinfo()` | `flags[myinfo->flag]` — flag is a raw client-supplied char from MyINFO; negative/≥12 value = out-of-bounds static read (network-triggered) | unsigned index + range check, fallback to `flags[0]` |
| `src/cmd.udp.c` (`cmd_udp`) | `strncpy(tmp,line,MAX_RECEIVE_ONCE)` into same-size buffer → missing NUL terminator on long input; `strsep`/`strlen` then run off the end | copy `MAX_RECEIVE_ONCE-1`, force NUL terminator |
| `src/cmd.conf.c` `parse_config_line()` | possible NULL `line` deref (`strlen(line)`) | early `return -1` guard (caller already handles -1) |
| `src/cmd.opt_line.c` `opt_version()` | `dcd -v` exited with status 1 after successful run | `exit(0)` |
| `src/net.c:48` | implicit switch fallthrough into `default:` in socket-error handler (intended: log + set ec=1) | explicit "fall through" comment, behavior unchanged |

### Reviewed and intentionally left as-is

- Remaining `-Wextra` noise: unused callback params (`param`, `args`), set-but-unused locals (`No`, `ret`, `write_result`), benign sign-comparisons against always-non-negative values, intentional shadowing of `conf`/`starttime` in helper functions
- `buf2cmd()` dead `&& !cmd` check (my_malloc aborts on OOM; removing risks changing console semantics for no gain)
- Historical ChangeLog/NEWS/spec-changelog entries mentioning tcp_wrappers

### Verification after fixes

- Clean builds: `-Wall -Wextra` and `-O2 -Wall` — zero new warnings, all previously flagged defect classes gone (only benign noise remains)
- `-fanalyzer`: remaining reports are OOM-modeling false positives around the `my_free1`/`my_duplicate` wrapper macros
- Smoke test: `build/dcd -v` prints version, exit=0; `dcd -h` OK

## 5. Performance optimizations (2026-09-28, follow-up)

### Build level (`CMakeLists.txt`)

- Default `CMAKE_BUILD_TYPE=Release` with `-O2 -DNDEBUG` — previously the CMake build produced **unoptimized** binaries when no build type was passed
- `-fno-strict-aliasing` on the target (legacy code casts through `char*` buffers; guards against aliasing miscompiles at -O2)
- Optional `DCD_ENABLE_LTO` cache option (`-flto`), off by default

### Hot-path fixes

- `src/main.c` accept loop: removed leftover debug `usleep(5)` executed before every `accept()` (added per-connection latency, marked "ONLY for testing purposes")
- `src/main.c` `user_manager()` (the 10 worker threads that drive all protocol traffic):
  - removed the unconditional `usleep(2000)` before each poll — it forced ~2–3 ms processing latency on every message batch; polling is now paced by the `select()` timeout alone (raised 1 ms → 5 ms), which also **cuts idle wakeups** (~333/s → ~200 per thread)
  - `select()` nfd argument was the global `maxfd` (`getdtablesize()`, capped at FD_SETSIZE=5030); now bounded by the actual highest socket in the set — kernel fd-table scan is O(nfd), so small hubs no longer pay a 5030-bit scan per cycle
  - added per-cycle `FD_ZERO`: previously quit users' fds stayed set across cycles, so a closed fd could linger and make `select()` fail with EBADF
  - empty-hub fast path now sleeps 5 ms instead of busy-`continue` (needed once the pacing sleep was removed)
- `src/misc.c` `add2buf()` (called for every received TCP chunk): replaced malloc+strlen+strcat×2+free full-copy with in-place `realloc` + single `memcpy` — removes the double copy and makes appends amortized O(1) instead of re-copying the whole buffer each chunk; also fixes a latent dangling pointer on MAXBLOCK realloc failure

### Regression caught & fixed during testing

- The earlier privilege-drop fix (`setuid`/`setgid` checked) made dcd refuse to start as non-root (EPERM → exit). Refined: fatal only when still euid 0 after the attempt; non-root runs continue with a warning.

### Verification

- Clean `-Wall -Wextra` and `-O2` builds, no new warnings
- **Live protocol smoke test**: hub started on port 41111 (config generated by CMake), real TCP client sent `$Connect ...|`, received `$Lock`, `$HubName`, welcome message — full accept → user_manager select-loop → send path exercised; daemon stayed alive and shut down cleanly via SIGTERM

## 6. Modernization (2026-09-28, follow-up)

### Build system (`CMakeLists.txt`)

- `cmake_minimum_required` 3.10 → **3.16**; explicit `CMAKE_C_STANDARD 17` (`REQUIRED`, GNU extensions kept for `strsep`/`daemon` fallbacks)
- `install(TARGETS dcd RUNTIME DESTINATION sbin)` — modern signature

### Headers

- `config.h`: gutted the ~490-line autoheader/Win32 cruft left from 2003 (verified every macro/typedef unused in `src/` first: `BYTE/WORD/DWORD`, `TRUE/FALSE/BOOL`, `_EOL/_PATH`, `_vXhz_*` calling-convention/export macros, `HAVE_LINUX*`, `HOST_OS/CPU`, ~250 `#undef HAVE_*` leftovers, the dangerous `NULL` redefinition). Now a 35-line header: guard + `<limits.h>` (provides `PATH_MAX` used by conf.c) + pointer that build-time definitions come from CMake compile definitions
- `src/pch.h`: removed all dead `#ifdef WIN32` branches (winsock/windows headers); dropped obsolete `<memory.h>`; `<sys/signal.h>` → standard `<signal.h>`; added explicit `<time.h>` and `<sys/select.h>` (previously satisfied only by accident of transitive includes)

### APIs

- `signal()` → **`sigaction()`** with `SA_RESTART` (portable, well-defined semantics matching what glibc `signal()` gave on Linux); removed invalid `signal(SIGKILL,...)` (SIGKILL cannot be caught)
- `inet_addr()` → **`inet_pton(AF_INET,...)`** in hublinks parsing, with a warning log for invalid addresses (`inet_addr` is deprecated and silently accepts malformed forms like `"10.1"`)
- `getdtablesize()` → `sysconf(_SC_OPEN_MAX)` (deprecated SVr4 API; POSIX replacement)

### Defensive formatting

- raw `sprintf` → `snprintf` with explicit bounds at the remaining fixed-buffer sites: `$UserIP` construction ×3 (`cmd.dc.c`, buffer 58 B) and the protocol-key expansion (`misc.c`, exact-budget 10-byte slots); all other sprintf sites were already provably bounded by their allocation arithmetic

### Verified semantics

- SIGHUP confirmed to be *rehash* (config reload — daemon stays up), SIGTERM clean shutdown; live `$Connect` smoke test passes after all changes
- Clean `-Wall -Wextra` build, no new warnings

## Status

All changes are **uncommitted** in the working tree (`git status`: modified sources/docs, deleted autotools files, new `CMakeLists.txt`).
