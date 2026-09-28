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

## Status

All changes are **uncommitted** in the working tree (`git status`: modified sources/docs, deleted autotools files, new `CMakeLists.txt`).
