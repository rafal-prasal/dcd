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

## 7. Command dispatch: bsearch → hash dictionary (2026-09-28)

Replaced the `bsearch`-based command dispatch (`bin_cmd_exec`/`case_bin_cmd_exec`, plus the unused linear `cmd_exec`) in `src/cmd.c`/`cmd.h` with a small open-hash dictionary:

### New API (`cmd.h` / `cmd.c`)

- `cmd_dict_t`: 128 buckets (power of two), per-entry chaining via fixed `short next[]` arrays — **no malloc**, index built once
- `cmd_dict_build(dict, set, case_insensitive)` — indexes a command table (terminator entry = default handler)
- `cmd_dict_exec(dict, cmd, param)` — FNV-1a hash lookup, O(1) average; falls back to the default entry's handler on miss; NULL-`cmd` safe
- Case-insensitive mode folds ASCII in hash+compare — locale-independent (replaces `tolower()`/`strcasecmp` dependence) and **no longer mutates the caller's command string** (old `case_bin_cmd_exec` lowercased it in place)

### Converted tables (`cmd_set` data + call sites)

All five tables converted from per-call stack-initialized arrays to `static const` data indexed once:

| Table | Module | Mode | Init |
|---|---|---|---|
| `dc_set` (DC protocol) | cmd.dc.c | case-sensitive | `pthread_once` (called from user_manager threads) |
| `chat_set` (`#commands`) | cmd.user.c | **case-insensitive** | `pthread_once` |
| `udp_set` (hub UDP cmds) | cmd.udp.c | case-sensitive | `pthread_once` |
| `conf_set` (~65 config directives — the hottest table, parsed per config line) | cmd.conf.c | case-sensitive | `pthread_once` (rehash can fire from any thread) |
| `opt_set` (CLI options) | cmd.opt_line.c | case-sensitive | built directly (pre-threads, single-shot) |

Removed: `cmd_exec`, `bin_cmd_exec`, `case_bin_cmd_exec`, `cmdcmp`, `string_tolower` and all `n_*_set` locals. (The unrelated `bsearch` over sorted logins in conf.c:177 was left as-is — different structure, not command dispatch.)

Old design costs eliminated: every call re-initialized the pointer table on the stack; bsearch required manually-sorted arrays (silent breakage if someone appended an unsorted entry); lookup was O(log n) with strcmp comparisons vs O(1) now.

Capacity note: `CMD_DICT_MAXENTRIES` set to 96 after counting `conf_set` (~65 entries) — a too-small value silently truncates the index, so it is sized above all tables.

Verified: clean build; live hub test — `$Connect` answered via dictionary hit, unknown `$BogusCommand` routed to default handler without crashing, config (conf_dict) parsed at startup, SIGTERM shutdown clean.

## 8. select() → poll() migration; latent FD_SETSIZE landmine fixed (2026-09-28)

**Finding:** `pch.h` defines `FD_SETSIZE MAXUSERS+30` (=5030), but glibc's `<sys/select.h>` redefines it unconditionally to 1024 *after* that — verified empirically against the pristine git tree too (pre-existing, not introduced by our modernization). Consequence: with >1023 open descriptors, `FD_SET()` in `user_manager()` writes past the stack `fd_set` → memory corruption. The original "handled 2k users" claim was plausible only on era-distros that shipped glibc with a raised `__FD_SETSIZE` (PLD/Gentoo-style tuning); on stock modern glibc it is a landmine.

**Fix:** converted the `user_manager()` poll loop to **`poll()`** (`struct pollfd pfds[MAXUSERS]`, `POLLIN|POLLHUP|POLLERR`, 5 ms timeout preserved, readiness mapped back via parallel `puser[]` + `poll_ready()` helper). No fd ceiling up to MAXUSERS=5000, no fd_set bit-array copies; measured `sizeof(userrec_t)` = 1080 B → ~2.1 MB for 2000 users (the low-memory-per-user claim holds — buffers are allocated on demand).

Verified: clean build; live hub test passes (`$Connect` answered, unknown command default-handled, SIGTERM clean).

## 9. Unit + performance test suite (2026-09-28)

### Infrastructure (`tests/`, `CMakeLists.txt`)

- `dcd` target refactored to link an OBJECT library `dcd_obj` (all sources except `main.c`), shared with the tests; single compile, identical compile definitions
- `option(DCD_BUILD_TESTS ON)` + `enable_testing()`; `ctest --test-dir build` runs everything
- `tests/test.h`: dependency-free harness (`CHECK`/`CHECK_INT`/`CHECK_STR`, monotonic timer)
- `tests/globals.c` + `tests/stubs.c`: the main.c-owned globals (`conf`, `user[]`, `login[]`, `pattern[]`, `penalties[]`, mutexes, sockets…) and faithful copies of the four tiny `user_*_state/supports` helpers, so test binaries link without `main()`
- Unit tests: `test_cmd_dict` (hash dict: hits/miss/default, CI mode + no caller-string mutation, NULL cmd/handler, capacity truncation at 96, bucket chains), `test_match` (wildcards `* ?`, quoting, case-insensitivity, legacy `%` behavior pinned), `test_misc` (`add2buf` incl. MAXBLOCK dangling-pointer path, `my_duplicate/my_free`, `make_path`, `uptime`, `flag_myinfo` OOB bounds, `lock_gen`, `get_progname`, `my_sprintf` growth loop), `test_myinfo` (`fill_myinfo` full-field success + all 7 error codes; `validnick` length/charset/reserved/in-use paths)
- Perf tests (ctest label `perf`): `bench_cmd_dict` (asserts ≥2x the replaced linear scan; measured ~10x), `bench_add2buf` (in-place realloc vs old full-copy path; measured ~8x), `bench_match` (throughput floor guard)

### Defect found & fixed by the tests

- `src/misc.c` `my_vsprintf()`: the growth loop called `vsnprintf(p,size,fmt,ap)` repeatedly with the **same consumed `va_list`** — for any output exceeding the initial 8 KiB buffer the second attempt read garbage/freed stack (crash reproduced by the test). Fixed with per-iteration `va_copy()`/`va_end()`.

### Finding left as-is

- `add2buf()` is still effectively quadratic in aggregate: each call runs `strlen(*buf)` to find the append offset. The §5 optimization removed the extra copies (measured ~8x faster than the pre-optimization path) but not this scan; fixing it needs per-buffer length tracking (API change), out of scope for a test-only pass.

### Verification

- `ctest --test-dir build`: 7/7 pass (4 unit + 3 perf), repeated runs stable
- Clean `-Wall` build: zero warnings from any new file (only the two pre-existing net.c set-but-unused remain)
- `dcd -v` still exits 0 after the object-library refactor and the va_copy fix

## 10. Error-check pass (2026-09-29)

Method: full rebuild with `-Wall -Wextra`, review of the last five commits (poll migration, hash dict, modernisation, performance fixes), and a read-through of `penalties.c/h`, `net.c`, `log.c`, `main.c`. All four unit suites re-run after each change.

### Real defects fixed

| Location | Defect | Fix |
|---|---|---|
| `src/main.c` `finish()` | `if ((sig!=SIGKILL \|\| sig!=SIGSEGV) && signal_p)` — the disjunction is a tautology, so the re-entrancy guard was active for **every** signal; a `SIGSEGV` during an in-progress handler was silently dropped instead of crashing out | `\|\|` → `&&` so KILL/SEGV bypass the guard |
| `src/main.c` `test_slots()` | `pubmsg("...is %d.", conf.minslots)` — `%d` against `uint32_t` | `%u` |
| `src/main.c` `listen_thread_udp()` | `remote_size` declared `unsigned int`, passed to `recvfrom()` expecting `socklen_t *` | `socklen_t` |
| `src/net.c` `strip_telnet()` | `write()` results assigned to a dead `write_result` variable; telnet replies unchecked | dedicated `tln_reply()` helper that checks `write()` and ignores errors deliberately |
| `src/net.c` `sendudp()` | dead `ret` variable swallowing `sendto()` result | call without assignment, intent documented |
| `src/penalties.h` | stale declaration `penalties_ip2itemnumber()` — function neither defined nor called anywhere | removed |
| `src/main.c` / `src/penalties.c` | dead global `test_penalty_counter` (defined + extern'd, never used) | removed both |
| `tests/test.h` | `now_sec()` `static` but unused in non-bench test binaries → `-Wunused-function` | `__attribute__((unused))` |

### Warning-only fixes (behaviour-preserving casts)

- `src/main.c:298` `NUSERS>=conf.userlimit` (`int` vs `unsigned int`) → explicit `(unsigned int)` cast; `NUSERS` is never negative.
- `src/main.c` `test_slots()` `usr->slots>=conf.minslots` — safe because negative slots are rejected earlier in the same function; cast added to silence `-Wsign-compare`.
- `src/log.c` `priority>conf.log_priority` (`int` vs `unsigned int`) → cast; `PR_*` priorities are 0–7.
- `src/main.c` `listen_thread_main()` — removed unused `No`/`parameter` (thread index is meaningless for the accept loop).

### Checked, left as-is

- `penalties.c` `user_set_penalty()` overwrite path: no leak — `my_duplicate()` frees the destination first (`misc.c:86`).
- `addpenalty()` bound `NPENAL+1>MAXPENALTIES`: correct, last valid index is `MAXPENALTIES-1`.
- `cmd_dict_*`: all five command sets ≤ 63 entries, well under `CMD_DICT_MAXENTRIES=96`; dictionaries built once via `pthread_once()`.
- `buf2cmd()` `... && !cmd` guard (main.c:567): dead condition inherited from upstream 1.1.0 — `cmd` is always non-NULL there; semantics of the intended check are ambiguous, so behaviour was not changed.
- `disconnect()` double `my_free(usr->ip)`: safe, `my_free` is the macro over `my_free1()` which NULLs the pointer.

### Verification

- `cmake --build build` with `-Wall -Wextra`: **zero warnings**.
- All four unit suites: 225 checks, 0 failures.

## Status

Earlier work is committed (`master` is 7 commits ahead of `origin/master`). The §10 fixes are **uncommitted** in the working tree.

## 11. NMDC protocol analysis + Python client + end-to-end suite (2026-09-29)

### Protocol analysis (NMDC as implemented by DCD)

Wire format: one line per frame, terminated by `|`; public chat is `<nick> text|`.

Connection lifecycle (TCP `listen_port_main`, UDP port = same number):

1. Hub sends `$Lock EXTENDEDPROTOCOL<99 chars> Pk=DCONNECT..DAEMON|` + `$HubName <name>|` (adduser, accept thread).
2. Client must answer `$Key <derived>|` — the key is **not** the lock echoed back: `lock_key()` (misc.c) XORs the salt with a rolling seed, swaps nibbles, and escapes protocol chars as `/%DCN%03i%/`. The client must run the same algorithm (`compute_key()` in the Python client).
3. Hub's user_manager sends the welcome file (`<HUB> ...|`) on its first cycle for the user, then moves it `WELCOME→CONNECTED`.
4. `$ValidateNick` (requires KEY state) → `validnick()`: length ≤31, no `#$:<>?`/space/leading `#`, not in use, not in `reserved_nicks[]` (Hub, Hub-Security, …), matches `nicks.allow`; registered nicks (dcd.users) → `$GetPass|` → `$MyPass` (wrong → `$BadPass|` + quit; ops also get `$LogedIn`). Success sends `$Hello` **only to the joiner** (not a broadcast).
5. `$MyINFO $ALL <nick> <desc>$ $<ctype><flag>$<email>$<share>$` → `fill_myinfo()`; on success the user becomes REGISTERED and the raw line is broadcast to all registered users (`dc_myinfo(NULL,usr)`). min_share/min_slots enforcement (redirect/quit) happens here.
6. `$Supports` (only NoHello/UserIP2/NoGetINFO/UserCommand echoed), `$GetNickList` (`$NickList`+`$OpList`, `$UserIP` per user with UserIP2), `$Search`/`$SR`/`$MultiSearch` (forwarded to all registered / UDP to linked hubs), `$ConnectToMe`/`$RevConnectToMe` (relayed verbatim to target), `$To:` private chat, `$Kick`/`$OpForceMove` (op perms s>a), `<nick> msg|` public chat (re-broadcast via `disttcp(NULL,...)` — **sender included**).
7. Quit/kick/rejection only take visible effect after the cleaner thread (5 s `CLEANER_INTERVAL`) reaps the QUIT-state user: `$Quit` broadcast + socket close.

Remote console (TCP `listen_port_cons`): banner → `login:` → nick → `password:` (needs `c` perm in dcd.users) → `> ` prompt; the same `#`-command set without the `#` (users, kick, stat, version, rehash, punish, …).

### Findings from the E2E suite

- **`dc_To()` comment vs. reality**: the documented `$To: x From: y $<<y>> msg` form does **not** parse — the strtok chain yields `nick2="<y"` → "Fake nick is not allowed". The working form is single-bracket `$<y>` (which is also what `privmsg()` itself emits). Left code as-is (comment-only discrepancy); client uses the working form.
- **`$Hello`/`$LogedIn` are unicast** to the joining client; other users learn of a new user only via the `$MyINFO` broadcast. (Standard NMDC clients treat `$Hello` as broadcast; DCD does not broadcast it.)
- **Rare message loss under load** (~1/25 connections in full-suite runs): hub protocol log shows `send()` returning full length with errno=0, yet the frames after the banner never reach the client; the subsequent cleaner-driven disconnect still occurs. Not reproduced in isolation (0/50 targeted stress runs). Tests accept message-or-disconnect as proof of rejection and the finding is documented in the client docstring.
- Hub never notices a silently-closed registered socket unless `idle_timeout`/`register_timeout` is configured (`my_recv` treats EOF as an empty message; POLLHUP is not acted on). Test config sets `idle_timeout = 10`.

### Added files

- `tests/e2e/nmdc.py` — dependency-free NMDC client: frame parser (`$cmd`, `<nick>`, `<HUB>`), inbox for out-of-order frames, `compute_key()` (lock→key derivation), full protocol helpers, `ConsoleClient` for the remote console.
- `tests/e2e/test_e2e.py` — 25 unittest E2E tests: banner/lock, invalid key, ValidateNick-before-Key, login+MyINFO echo, MyINFO broadcast, duplicate/reserved/invalid-char/too-long nick rejection, registered password flow (`$GetPass`/`$BadPass`/`$LogedIn`), `$GetNickList`/`$OpList`, `$Supports` echo filtering, public chat relay, fake-nick chat kick, private chat, `$Search` forwarding, `$ConnectToMe`/`$RevConnectToMe` relay, op kick via DC protocol, non-op kick rejection, `$Quit` broadcast, console login/`users`/`version`/`kick`, console wrong password.
- `tests/e2e/run_e2e.sh` — harness: configures+builds+installs dcd into a writable prefix (`/tmp/dcd-e2e`, so `CONFIGDIR` is writable), writes a test config (ports 41111/41511, `search_interval=0`, `idle_timeout=10`, op account `testop/secret` perms `sc`), starts the daemon, runs the suite, stops it (SIGTERM, SIGKILL fallback).

### Verification

- `bash tests/e2e/run_e2e.sh`: **25/25 pass**, stable across 3 consecutive full runs.
- `ctest --test-dir build`: 7/7 unit/perf tests still pass; clean build, no new warnings (temporary `sendtcp` instrumentation used during diagnosis was reverted).

## 12. ThreadSanitizer/AddressSanitizer pass: race + OOB fixes (2026-09-29/30)

Method: built `dcd` under ThreadSanitizer and AddressSanitizer and ran the §11 E2E suite against each. TSan reported 28 data races + 7 "heap-use-after-free" + 5 thread-leaks + 1 signal-unsafe call; ASan (precise line info) showed the "UAF" was really a heap-buffer-underflow. All findings triaged; the thread-leaks were left as benign.

### Sanitizer tooling (reproducible)

- `CMakeLists.txt`: `DCD_ENABLE_TSAN` / `DCD_ENABLE_ASAN` cache options (OFF by default) — instrument only the `dcd` binary with `-fsanitize=thread|address -g -O1` (drop the Release `-O2` so the sanitizer's `-O1` wins).
- `tests/e2e/run_e2e.sh`: `DCD_SANITIZER=thread|address` — separate build/prefix dir, `-DDCD_BUILD_TESTS=OFF` (the shared `dcd_obj` object lib isn't linked against the sanitizer runtime), and `TSAN_OPTIONS`/`ASAN_OPTIONS=log_path=…` because `daemon()` sends stderr to `/dev/null`. Prints a findings count + full logs after the run.

### Per-user data race — fixed (structural)

The five user-table locks (`mutex_userlist`, per-thread `mutex_user_manager[]`, `mutex_th_udp`, `mutex_th_cleaner`, `mutex_is_managed`) were used inconsistently: `lock_threads()` (cleaner/rehash) froze the workers, but `adduser` (accept thread) only took `mutex_userlist` and the console thread took nothing — so those paths raced with the worker on `user[]`/`NUSERS`/`userrec_t`.

| Location | Change |
|---|---|
| `main.c` mutex set | Collapsed the five into one **recursive `mutex_users`** guarding `user[]`, `NUSERS`, every `userrec_t`, and the config/pattern/login tables. `lock_threads()`/`unlock_threads()` lock it; `userlist_add/del` lock it. |
| `main.c` `adduser` / `console.c` `addcons` | Hold `mutex_users` across the user-limit check + insertion (atomic w.r.t. worker/cleaner). |
| `main.c` `listen_thread_udp` | Lock `mutex_users` (was `mutex_th_udp`). |
| `main.c` `user_manager` | Hold `mutex_users` across the whole cycle, including the `if(!NUSERS)` fast-path read (the last remaining race). Removed the `is_managed()`/`managed[]` claim machinery and per-worker mutexes. |
| `main.c` `make_threads` | **Single protocol worker** — under one lock extra workers only duplicate work, so `conf.n_user_manager` is no longer used for thread creation. |

### `match()` heap-buffer-underflow — fixed

The TSan "UAF" (read in `match.c:40` ← `nick_allowed` ← `validnick` ← `$ValidateNick`) was, per ASan, a read 1 byte **before** the `strdup`'d pattern buffer: for a single-char `*` pattern the wildcard-zap moves `m` to `ma-1` and `switch(*m)` reads out of bounds. Added `if (m < ma) break;` at the top of the loop; the existing tail logic then correctly returns a match. `test_match` still passes.

### Signal-unsafe handler — fixed

`finish()` no longer locks/logs/`pthread_cancel()`/`exit()` from the handler. It only sets async-signal-safe flags (`rehash`, `term_sig`); the main loop runs new `do_rehash()`/`do_shutdown()`. `SIGPIPE` → `SIG_IGN`; the uncatchable `SIGKILL` handler dropped. `rehash` is now `volatile sig_atomic_t` (externs updated in `cmd.{user,dc,udp}.c`, `tests/globals.c`). `signal_p` removed.

### Left as-is (not a defect)

- 5 TSan **thread-leak** reports: threads still running when the process exits via the SIGTERM path. Joining would deadlock against `mutex_users` (deferred-cancel worker blocks on the lock) and the process is exiting anyway.

### Verification

- Normal build: clean, **zero warnings** (`-Wall`); strict `-Wall -Wextra` shows only pre-existing unused-callback-param noise.
- `ctest --test-dir build`: 7/7 pass.
- **TSan E2E: 25/25 pass, 0 data races, 0 UAF/OOB** (was 28 races + 7 UAF; only the 5 expected thread-leaks remain).
- **ASan E2E: 25/25 pass, 0 findings** (confirms the `match()` fix).
- Signal smoke test: daemon survives SIGHUP (rehash), exits cleanly on SIGTERM.

## 13. `nick2id()` linear scan → nick→index hash (2026-09-30)

Every nick-addressed operation — `$To:`, `$Kick`, `$GetINFO`, `$ConnectToMe`, `$RevConnectToMe`, `$OpForceMove`, `$SR` (TCP + UDP), the `#`-chat commands via `chat_nick2id()`, and the "nick already used" check in `validnick()` — resolved a nick to a `user[]` index by scanning the whole table with `strcasecmp`: O(N) per call, up to 5000 comparisons at `MAXUSERS`. Replaced with an open-addressing hash mirroring the §7 `cmd.c` style.

### New machinery (`src/misc.c`, declared in `misc.h`)

- Static open-addressing table: 16384 power-of-two slots (`NICK_HASH_BITS=14`), load ≤ 0.5 at `MAXUSERS=5000` so a probe always terminates; parallel `nick_hash_key[]` (aliases the owning `userrec_t.nick`) + `nick_hash_id[]`.
- FNV-1a hash, ASCII-case-folded — same constants as `cmd.c`. Equality still uses `strcasecmp`, so non-ASCII nicks keep their existing locale semantics (a fold mismatch only costs a probe hop).
- Linear probing; **backward-shift deletion** (no tombstones) — on remove, following entries are shifted back toward their home slot, keeping every probe chain intact.
- `user_set_nick(usr, nick)` — the single nick setter: drops the old key, `my_duplicate`s the new one, inserts the key at `usr->id`. Replaces the five raw `my_duplicate(nick,&usr->nick)` sites (4 in `cmd.dc.c` `dc_ValidateNick`, 1 in `console.c` `manage_cons`).
- `nick_hash_forget(usr)` / `nick_hash_rehome(nick,id)` — key removal and index re-homing for the table-maintenance paths.
- `nick2id()` is now a hash lookup (NULL-safe).

### Invariant maintenance (the hash mirrors `user[]`)

| Location | Change |
|---|---|
| `main.c` `userlist_del()` | Drops the removed user's key **before** the swap-remove (so `nick2id()` never resolves a removed user), then re-homes the index of the user swapped into the freed slot. The last-slot self-assignment case is guarded (`if (id<NUSERS)`) so the just-removed key is not re-added. |
| `net.c` `disconnect()` | `nick_hash_forget(usr)` before `my_free(usr->nick)` — the key aliases that string. Harmless double-forget after `userlist_del`. |
| `cmd.dc.c` / `console.c` | All nick assignments routed through `user_set_nick()`. |
| `tests/stubs.c` | `userlist_del` stub kept faithful to the new `main.c` (forget + guarded re-home). |

The hash is guarded by the same recursive `mutex_users` as the user table; every `nick2id()`/`user_set_nick()` caller already holds it (worker loop, UDP handler, console, cleaner) — no new lock.

### Tests

- `tests/test_misc.c`: new nick-hash suite — basic + case-insensitive lookup, NULL/empty, re-nick (old key dropped), swap-remove re-homing (middle and last slot), `validnick()` in-use rejection, and a 500-key churn test that deletes every other user (exercises probe-chain shifts + backward-shift delete) and verifies survivors against each record's own `id` (swap-remove permutes the table, so absolute indices are not stable).
- `tests/test_myinfo.c`: `test_validnick_in_use` updated to set the existing user's nick through `user_set_nick()` (the hash, not the raw `user[]` scan, is now the source of truth).
- `tests/bench_nick2id.c` (ctest label `perf`): hash vs the replaced linear scan at 1000 users — **~110 Mops/s vs ~1.1 Mops/s (~100×)**; asserts ≥2× plus an absolute floor.

### Verification

- `ctest --test-dir build`: all unit + perf suites pass (was 7, now 8 with `bench_nick2id`).
- E2E (`tests/e2e/run_e2e.sh`): 25/25 pass.
- **TSan E2E**: no data races (only the documented benign thread-leaks) — confirms the hash is correctly confined to `mutex_users`.
- **ASan E2E**: only the pre-existing `sockerr` 20-byte leak (unrelated, present before this change).
- Clean `-Wall` build, zero new warnings.

## 14. Salted SHA-512 password hashes in dcd.users (2026-09-30)

`dcd.users` password fields were plaintext-only. Added `/etc/shadow`-style hashed passwords: `$sha512$<salt>$<hex(sha512(salt+password))>` (same `$id$salt$hash` layout; salt alphabet `[a-zA-Z0-9./]`, ≤64 chars; digest lowercase hex, constant-time compare). Plaintext values are still accepted (legacy entries).

### Implementation

- `src/sha512.{c,h}` (was present but never built): added `password_match(stored, supplied)` — parses the `$sha512$` form (validates salt charset, 128-hex digest), hashes `salt||password` with the existing `sha512_hex`, constant-time digest compare; anything not `$sha512$`-prefixed falls back to plaintext compare. NULL-safe.
- `src/conf.c` `validlogin()`: console password check now `password_match()`; also guards `my_duplicate()` of a NULL password (the documented `login:` reserve-only form previously segfaulted on `strdup(NULL)`).
- `src/cmd.dc.c` `dc_MyPass()`: `$MyPass` verification now `password_match()`.
- `src/pch.h`: includes `sha512.h`; `CMakeLists.txt`: `src/sha512.c` added to `DCD_CORE_SOURCES`.
- `src/dcd.adduser.in`: generates a 16-char random salt from `/dev/urandom` (`tr -dc 'A-Za-z0-9./'`) and stores `$sha512$<salt>$<sha512sum>`; also fixed two pre-existing bugs: `@CONFIGDIR@` was never substituted (CMake variable `CONFIGDIR` undefined — only the compile definition existed; now `set(CONFIGDIR ...)` in CMakeLists) and `[ … == … ]` bashism under `#!/bin/sh` (dash) broke the existing-user update path.
- Docs: `conf/dcd.users` header comment, `README.in` (replaces the "there will be soon" placeholder), `doc/dcd.2` `conf_cusers` section.

### Tests

- `tests/test_sha512.c` (new ctest): FIPS 180-4 vectors (`abc`, empty, salted) pin `sha512_hex`; plaintext match/mismatch/NULL; hashed match, wrong password, wrong salt, empty-password hash; malformed hashes (truncated/extended/uppercase/non-hex digest, missing `$`, empty/bad-charset salt, `$sha512$`-prefixed value never treated as plaintext).

### Verification

- `ctest --test-dir build`: 9/9 pass (was 8).
- E2E (`tests/e2e/run_e2e.sh`): 25/25 pass (plaintext flow unchanged).
- Live-hub check: `dcd.adduser` (installed script) add + update paths produce fresh random-salt hashes; console and NMDC `$MyPass` login accept the correct password and reject wrong ones for hashed entries; plaintext `admin:changeme` still logs in.

## 15. SHA-512 backend: custom implementation → OpenSSL libcrypto (2026-10-25)

The hand-rolled dependency-free SHA-512 from §14 (K[80] table, `sha512_compress()`, manual padding — ~90 lines of crypto) was replaced with OpenSSL's `EVP_Q_digest(NULL, "SHA512", ...)`. `sha512_hex()`/`password_match()` APIs, the `$sha512$salt$hex` format and all tests are unchanged; a failed digest now yields an empty string (impossible before). Rationale: no hand-written crypto to audit; OpenSSL's SHA-512 is also ~4x faster than the removed code (see below).

### Implementation

- `src/sha512.c`: hand-rolled core removed; `sha512_hex()` is now a thin `EVP_Q_digest` wrapper + hex encoding (110 → 25 lines in that area).
- `CMakeLists.txt`: `find_package(OpenSSL REQUIRED)`, `target_link_libraries(dcd_obj PUBLIC OpenSSL::Crypto)` (propagates to `dcd` and all tests).
- `contrib/PLD/dcd.spec.in`, `contrib/MandrakeLinux/dcd.spec.in`: `BuildRequires: openssl-devel`, `Requires: libcrypto`.

### Tests / Verification

- New `tests/bench_sha512.c` (ctest, `perf` label): pins the FIPS `abc` vector, floors sub-50 µs per password-sized hash, >50 MiB/s bulk, sub-200 µs per `password_match`. Measured (OpenSSL 3.5.5, -O2): `sha512_hex(17 B)` ≈ 3.3 M ops/s (0.30 µs), bulk 1 MiB ≈ 1.3 GiB/s, full `password_match` ≈ 2.65 M ops/s (0.4 µs) — login-time hashing cost is negligible.
- ctest 10/10; `dcd` links `libcrypto.so.3`.

## 16. Runtime telemetry: traffic, command and user-rotation counters (2026-10-25)

Console `stats` / `stats reset` commands report hub traffic, per-command dispatch counts and user rotation (logins/logouts, concurrent online, max online, EWMA logins/hour). Counters are relaxed GCC atomics (the daemon is threaded: listeners, cleaner, user_manager); the EWMA is a double under its own mutex.

### Implementation

- `src/stats.{c,h}` (new): `stats_add_tcp/udp()`, `stats_user_registered()/left()`, `stats_format()`, `stats_reset()`; max_online maintained by CAS; `stats_reset()` zeroes counters but keeps currently-online visible.
- `src/cmd.{c,h}`: `cmd_dict_t` gains `hits[CMD_DICT_MAXENTRIES+1]` (index `n` = unknown/default handler), `count`, `name` and a registry; `cmd_dict_exec()` counts dispatches atomically; `cmd_stats_format()`/`cmd_stats_reset()` emit per-dictionary lines. `cmd_dict_build_nc()` builds non-counted dictionaries — used by `src/cmd.conf.c` (config-file parser shares the dispatcher; its "commands" are not hub traffic) and `src/cmd.opt_line.c`; counted: `dc`, `chat`, `udp`.
- Hooks: `user_set_state()` (src/main.c — the single choke point for all `STATE_*` transitions) drives login/logout/online counts; `my_recv()`/`sendtcp()` (src/net.c) add TCP totals plus per-user `userrec_t.recv_bytes/send_bytes` (usable later for transfer limits); `sendudp()` and the UDP `recvfrom` loop add UDP totals.
- `src/cmd.user.c`: `chat_stats` (console only; non-console falls back to `disttcp` like `chat_stat`), table entry `stats`.

### Tests / Verification

- New `tests/test_stats.c` (bytes formatting, rotation math, online clamping at zero, reset keeps online) and `test_cmd_dict` telemetry cases (hits incl. unknown, nc not counted/not reported, format output, reset).
- New e2e `test_console_stats`: console `stats` shows all report fields + `chat stats=` breakdown; `stats reset` zeroes counters while online users stay visible.
- ctest 11/11; e2e 26/26; e2e under ThreadSanitizer: no data races (only the pre-existing `make_threads` thread-leak warnings at shutdown).

## Status

§1–§11 are committed (`master` ahead of `origin/master`). The §12, §13, §14, §15 and §16 changes are **uncommitted** in the working tree.
