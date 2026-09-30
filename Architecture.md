# DCD (DConnect Daemon) — Architecture

Version documented: 1.1.1 (post-modernization tree: CMake build, poll()-based worker,
single user lock, hash command dispatch, sanitizer-clean under TSan/ASan).

---

## 1. What dcd is

`dcd` is a hub server for the Direct Connect file-sharing network (NMDC protocol,
the protocol spoken by NeoModus Direct Connect and compatible clients such as DC++).
A hub is a rendezvous point: clients connect to it, register a nick, exchange
presence information (`$MyINFO`), chat, search, and are introduced to each other so
that actual file transfers happen **directly between clients** — the hub never
relays file data.

Design properties inherited from the original 2002 design and preserved (or
strengthened) in this tree:

* pure C, single process, **threads, no forking** (`README.in`);
* low per-user memory: the user record is a fixed ~1 KB struct (`userrec_t`,
  `main.h:259`); receive/send buffers are allocated on demand;
* hard caps: `MAXUSERS = 5000` (`pch.h:16`), `MAXHUBS = 100`, `MAXBLOCK = 1 MiB`
  per dynamic buffer (`main.h:23`);
* everything is configured through plain-text files; `SIGHUP` re-reads them
  without a restart.

### Source layout

| File | Responsibility |
|---|---|
| `src/main.c` | startup, daemonize, threads, accept loops, protocol worker, cleaner, signals |
| `src/main.h` | defaults, `userrec_t` / `config_t` / `Tpenalty` / `hub_t` / `loginrec_t`, state & error constants |
| `src/cmd.c`, `cmd.h` | generic hash command dictionary (`cmd_dict_*`) |
| `src/cmd.dc.c` | NMDC protocol handlers (`$Key`, `$ValidateNick`, `$MyINFO`, …) |
| `src/cmd.user.c` | `#chat` commands (`#kick`, `#punish`, `#users`, …) — shared by DC users and the console |
| `src/cmd.conf.c` | `dcd.conf` directive handlers (~65 directives) |
| `src/cmd.udp.c` | hub-to-hub UDP commands (`$Up`, `$UpToo`, `$Search`, `$SR`, `$ConnectToMe`) |
| `src/cmd.opt_line.c` | command-line options (`-h`, `-v`, `-c`) |
| `src/console.c` | remote administration console (telnet) |
| `src/conf.c` | config/message/login/pattern/penalty/hublist file parsers |
| `src/net.c` | TCP/UDP send & receive, multicast helpers, telnet-IAC stripping, disconnect |
| `src/misc.c` | allocator wrappers, lock/key derivation, nick validation, nick→id hash, MyINFO parsing, buffers |
| `src/match.c` | wildcard matcher (`*`, `?`, `%`, `\` quoting) used for bans/nick patterns |
| `src/penalties.c` | punishment subsystem (bitmask penalties, persistence) |
| `src/log.c` | logging to syslog or per-facility files |
| `src/compatibility.c` | small portability shims (e.g. `daemon()` fallback) |
| `tests/` | unit + perf tests (C, `ctest`) and an end-to-end Python NMDC suite |

All modules include `src/pch.h`, a single umbrella header (classic precompiled-header
style) that pulls the POSIX headers and every project header.

---

## 2. Threading architecture

### 2.1 Thread inventory

Threads are created once in `make_threads()` (`main.c:970`) after the daemon has
daemonized. There is no dynamic thread pool.

```
main thread ── daemonize ──► supervisory loop: sleep(1); consume `rehash` / `term_sig`
   │
   ├── listen_thread_main ×N   (N = n_listen_main, default 2, max 5)
   │        accept() on listen_port_main → adduser()  (sends $Lock/$HubName)
   │
   ├── user_manager ×1         (the protocol worker — the heart of the hub)
   │        poll() over all user sockets → parse → dispatch → respond
   │
   ├── cleaner ×1              sleep(CLEANER_INTERVAL=5s) → reap QUIT users,
   │        reload message/login/nick files, write penalties, hub heartbeats
   │
   ├── listen_thread_udp ×1    recvfrom() on the UDP port → udp_cmd_exec()
   │
   └── listen_thread_cons ×1   accept() on listen_port_cons → addcons()
            (console sessions are then driven by the same user_manager loop)
```

* **Main thread.** `main()` (`main.c:1040`) parses CLI options, reads `dcd.conf`,
  creates and binds the listening sockets, drops privileges (`setgid`/`setuid`),
  re-opens logs, parses the auxiliary files, initializes mutexes, calls
  `daemon(0,0)`, loads penalties, spawns the threads above, and then runs a
  1-second supervisory loop that only services `rehash` and termination requests.
  The main thread never touches the user table.
* **Accept threads** (`listen_thread_main`, `main.c:372`). Up to `MAX_N_LISTEN_MAIN=5`
  threads `accept()` on the same listening socket; the kernel distributes
  connections. Each calls `adduser()` (`main.c:263`), which sets socket options,
  forces `O_NONBLOCK`, checks the user limit, allocates the `userrec_t`, generates
  the connection lock, sends `$Lock … Pk=DCONNECT..DAEMON|` + `$HubName …|`, and
  inserts the user into the table.
* **Protocol worker** (`user_manager`, `main.c:658`). Exactly **one** thread drives
  all protocol traffic for every TCP session (DC clients *and* console sessions).
  Each cycle, under the user lock: snapshot the non-quit users into a
  `struct pollfd pfds[MAXUSERS]` + parallel `puser[]` index, `poll()` with a 5 ms
  timeout, then for each ready user: `my_recv()` → `add2buf()` → `buf2cmd()` →
  `dc_cmd_exec()` (or `manage_cons()` for console sessions). It also performs the
  deferred per-user work that cannot run in the accept path: sending the welcome
  message (`STATE_WELCOME → STATE_CONNECTED`) and the min-slots evaluation
  (`test_slots()`).
* **Cleaner** (`cleaner`, `main.c:910`). Every 5 s, under the user lock: re-reads
  `dcd.motd`/`dcd.welcome`/`dcd.rules`/`dcd.usercommands`, `dcd.users` and
  `nicks.allow` (so account edits take effect without a restart), flushes
  `dcd.penalties` if `penalties_update_interval` passed, sends a keep-alive `$|`
  to everyone, runs `check_timeout()` (idle / registration timeouts) and the
  permanent-ban check, and **reaps** users in `STATE_QUIT` — this is where the
  `$Quit` broadcast and `close()` actually happen. A quit user is invisible to
  `poll()` but stays in the table until the cleaner removes it, which is why
  kick/quit effects appear with up to `CLEANER_INTERVAL` latency.
* **UDP listener** (`listen_thread_udp`, `main.c:406`). Blocks in `recvfrom()`;
  each datagram is dispatched through the UDP command dictionary under the user
  lock. Used for hub linking and for the min-slots `$SR` replies.
* **Console acceptor** (`listen_thread_cons`, `console.c:70`). `accept()` on the
  console port, then `addcons()` checks `console.allow` (with `include:` support),
  sends the banner + `login: `, and registers the session as a `userrec_t` with
  `cons=1`. From then on the console session is just another socket handled by the
  protocol worker.

### 2.2 Synchronization model

The original 1.1.0 tree used five inconsistently-applied locks
(`mutex_userlist`, per-worker `mutex_user_manager[]`, `mutex_th_udp`,
`mutex_th_cleaner`, `mutex_is_managed`) plus a per-user "claim" mechanism
(`managed[]`). They did not cover the accept and console paths, so those raced
with the worker (28 data races reported by ThreadSanitizer).

The current design collapses all of that into **one recursive mutex**:

* `mutex_users` (`main.c:70`, initialized recursive at `main.c:1020`) guards
  `user[]`, `NUSERS`, every `userrec_t` field, and the config/pattern/login
  tables that the cleaner and rehash rebuild.
  * `lock_threads()`/`unlock_threads()` are thin wrappers used by the cleaner and
    by `do_rehash()`/`do_shutdown()`.
  * `userlist_add()`/`userlist_del()` lock it themselves; `adduser`/`addcons`
    hold it across the *user-limit check + insertion* so that pair is atomic.
  * `user_manager` holds it for the **entire cycle**, including the `if(!NUSERS)`
    fast path — the last remaining race.
  * Recursive because `adduser`/`addcons`/`cleaner` already hold it when they call
    `userlist_add/del`.
* `mutex_penalties` guards the `penalties[]` table (`penalties.c`).
* `mutex_inet_ntoa` serializes `inet_ntoa()` (static buffer) — `my_inet_ntoa()`
  copies out under it (`misc.c:73`).
* `mutex_myinfo` guards the pre-rendered `$MyINFO` cache string inside
  `userrec_t.myinfo.cache` (`net.c:172`, `misc.c:304`).
* Each command dictionary is built exactly once via `pthread_once()`
  (`dc_dict_once`, `chat_dict_once`, `udp_dict_once`, `conf_dict_once`).

Consequence of the single lock: additional protocol workers would only duplicate
work, so `conf.n_user_manager` is no longer used for thread creation
(`make_threads`, `main.c:986`). The config option is accepted but inert.

### 2.3 Signals

`init_var()` (`main.c:729`) installs handlers with `sigaction()` + `SA_RESTART`:

* `SIGPIPE` → `SIG_IGN` (sends return `EPIPE`, already handled);
* `SIGHUP` → sets `volatile sig_atomic_t rehash`;
* `SIGTERM`/`SIGINT`/`SIGQUIT`/`SIGSEGV` → records the first one in `term_sig`.

The handler (`finish`, `main.c:450`) is strictly async-signal-safe: it sets flags
only. The main loop then performs the real work:

* `do_rehash()` (`main.c:471`): `lock_threads()` → `parse_config()` →
  `make_paths()` → `log_open()` → `parse_hublist()` → `unlock_threads()`.
* `do_shutdown()` (`main.c:487`): `SIGSEGV` exits immediately after closing
  listeners; otherwise it cancels all threads (they use
  `PTHREAD_CANCEL_ASYNCHRONOUS`), disconnects all users, closes listeners, exits.

### 2.4 Lifecycle of a connection (happy path)

1. `accept()` → `adduser()`: `$Lock EXTENDEDPROTOCOL<99 chars> Pk=DCONNECT..DAEMON|`,
   `$HubName <name>|`; state `STATE_WELCOME`; expected key precomputed by
   `lock_key()`.
2. Worker's first cycle for the user: sends `dcd.welcome` as `<HUB> …|`, state →
   `STATE_CONNECTED`.
3. Client `$Key <derived>` → `STATE_KEY` (or disconnect if invalid and
   `allow_broken_key=0`).
4. `$ValidateNick` → `validnick()`; new nick → `$Hello <nick>|` **unicast to the
   joiner** only; other users learn about the joiner from the `$MyINFO` broadcast.
   Registered nick → `$GetPass|` → `$MyPass` (wrong → `$BadPass|` + quit; ops also
   get `$LogedIn`), state → `STATE_LOGGEDIN`.
5. `$MyINFO` → `fill_myinfo()`; if `min_slots` is configured the hub probes the
   client's upload slots (see §3.6); otherwise state → `STATE_REGISTERED` and the
   raw line is broadcast to all registered users.
6. Normal operation: chat, search, ConnectToMe relays, etc.
7. Quit/kick/rejection: state → `STATE_QUIT`; the cleaner reaps it — `$Quit`
   broadcast + `close()` happen there.

---

## 3. Protocol: NMDC as implemented by dcd

### 3.1 Framing and dispatch

One command per line, terminated by `|` (console sessions use `\n`). The worker
accumulates raw chunks per user (`add2buf`), and `buf2cmd()` (`main.c:521`)
splits off one complete line at a time. `dc_cmd_exec()` (`cmd.dc.c:1065`) splits
`cmd`/`param` and routes by first byte:

* `<` → public chat (`dc_chat` → `chat_cmd_exec`),
* `$` → NMDC command via the hash dictionary,
* anything else → garbage (currently ignored).

`max_commands` (default 100) caps how many commands one user may execute per
cycle; `max_receive_once` (≤1024) caps one `recv()`; a user whose unterminated
buffer exceeds `MAX_USER_BUFFER` (2 KiB) is disconnected ("too talkative").

### 3.2 Command dispatch: hash dictionary

`cmd.c`/`cmd.h` implement a small open-hash dictionary used by **all five**
command tables:

* 128 power-of-two buckets, per-entry chaining via fixed `short next[]` arrays —
  **no malloc**, index built once per table (`cmd_dict_build`);
* FNV-1a hash; case-insensitive mode folds ASCII in hash *and* compare
  (locale-independent, and it no longer mutates the caller's string);
* miss → the table's terminator entry, which carries the default handler
  (`dc_not_implemented`, `chat_not_found`, `udp_not_implemented`);
* `CMD_DICT_MAXENTRIES = 96` — sized above the largest table (`conf_set`, ~65
  entries); a too-small value would silently truncate the index.

| Table | Module | Mode | Init |
|---|---|---|---|
| `dc_set` (NMDC) | `cmd.dc.c` | case-sensitive | `pthread_once` |
| `chat_set` (`#commands`) | `cmd.user.c` | case-insensitive | `pthread_once` |
| `udp_set` (hub UDP) | `cmd.udp.c` | case-sensitive | `pthread_once` |
| `conf_set` (config directives) | `cmd.conf.c` | case-sensitive | `pthread_once` |
| `opt_set` (CLI options) | `cmd.opt_line.c` | case-sensitive | built directly (pre-threads) |

This replaced the original `bsearch` dispatch, which re-initialized the table on
the stack per call, required manually sorted arrays, and was O(log n) with
`strcmp` comparisons.

### 3.3 Lock/key handshake

`lock_gen()` (`misc.c:125`) builds the lock: the literal `EXTENDEDPROTOCOL`
prefix plus random printable bytes (99 chars total, `SALT_LEN=100`). The client
must reply `$Key <derived>` where the derivation (`lock_key`, `misc.c:146`) is:

1. XOR each byte with the previous one (rolling seed starting at 5);
2. swap the nibbles of each byte;
3. XOR the first byte with the last;
4. escape protocol characters (`0x00 0x05 0x7C 0x60 0x7E 0x24`) as
   `/%DCN%03i%/`, leave the rest verbatim.

`validkey()` compares the reply. `allow_broken_key=1` skips the check entirely.

### 3.4 User state machine

States are bit flags in `userrec_t.state` (`main.h:187`):
`WELCOME → CONNECTED → KEY → (PASSWORD →) LOGGEDIN → REGISTERED`, with
`SR`/`SLOTS_TESTED` for slot probing and `QUIT`/`QUIT_SHOW`/`SHOW_QUIT` for
teardown. `user_set_state()` refuses to change the state of a quitting user;
`STATE_QUIT` on a logged-in/registered user becomes `STATE_QUIT_SHOW` so the
cleaner broadcasts `$Quit`.

Nick validation (`validnick`, `misc.c:247`) enforces: ≤31 chars, no
`#$:<>?`/whitespace, no leading `#`, not in use (`nick2id`), not in
`reserved_nicks[]` (`Hub`, `Hub-Security`, `Vandel\Debug`, `Client`), and matching
`nicks.allow` patterns. Each failure has a distinct `NICK_*` code with a specific
client message.

### 3.5 Implemented NMDC commands

`$Key`, `$ValidateNick`, `$MyPass`, `$Version`, `$Supports` (echoes only
`NoHello`, `NoGetINFO`, `UserIP2`, `UserCommand`), `$MyINFO`, `$GetINFO`,
`$GetNickList` (`$NickList` + `$OpList`, plus `$UserIP` per user for `UserIP2`
clients), `$Search` (forwarded to all registered users; `Hub:` form for passive
clients; forwarding-IP check), `$MultiSearch` (UDP to linked hubs), `$SR`
(search results routed to the target nick), `$ConnectToMe`, `$RevConnectToMe`,
`$MultiConnectToMe` (UDP), `$To:` (private chat), `$Kick`, `$OpForceMove`,
`$Quit`, `<nick> text|` public chat (re-broadcast **including the sender**),
`$UserIP` (accepted, no-op — IPs are sent with every nick list). Unknown `$…`
commands hit `dc_not_implemented` (logged, not fatal).

Anti-spoofing: every command that names a nick compares it against the session's
validated nick ("Fake nick is not allowed" → disconnect).

### 3.6 Upload-slot (min_slots) probing

When `min_slots > 0`, after `$MyINFO` the hub sets `STATE_SR` and sends the
client two fake searches addressed to *the hub's own UDP port*
(`cmd.dc.c:333`). The client answers each with a UDP `$SR` back to that port;
`udp_SR()` (`cmd.udp.c`) parses `free/total` slots from the response and flips
the user to `STATE_SLOTS_TESTED`. The worker then runs `test_slots()`
(`main.c:596`): ≥ `min_slots` → `STATE_REGISTERED` + MOTD/UserCommands; below →
`$ForceMove` redirect or quit. `s`/`r` permission holders skip the check;
`register_timeout` catches clients that never answer.

### 3.7 Hub linking (hubnet)

`dcd.hublinks` lists peer hubs as `ip:port:pass_in:pass_out`. `parse_hublist()`
opens one UDP socket per peer and pre-renders `$Up`/`$UpToo` heartbeat packets.
Link maintenance:

* the cleaner re-sends `$Up` to any peer silent for >120 s;
* `udp_Up`/`udp_UpToo` validate the password and address, refresh `timeout`, and
  answer `$UpToo` — a peer is "linked" while `timeout` is fresh;
* `$MultiSearch` and `$MultiConnectToMe` are forwarded over UDP to linked hubs;
  inbound `$Search`/`$SR`/`$ConnectToMe` from a linked hub are re-broadcast on
  TCP (`cmd.udp.c`).

### 3.8 Chat commands (`#…`) and the console

Public chat lines are routed through `chat_cmd_exec()`; a leading `#` selects a
command from `chat_set` (case-insensitive). The same set serves the telnet
console, minus the `#`. Commands: `amnesty`, `amnesty_all`, `conf`, `date`/`time`,
`help`, `kick`, `lspunished`, `mmsg(_all)`, `msg(_all)`, `msg_prv(_all)`, `myip`,
`punish`, `quit`, `redirect(_all)`, `rehash`, `rules`, `showops`, `stat`,
`uptime`, `users`, `version`, `who`, `whoip`, `whois`.

Permission model (`dcd.users` permission letters): `s` superior operator,
`a` administrator, `c` console access, `n` reserved nick, `p` passive allowed,
`m` skip min-share, `r` skip min-slots, `f` may forward its IP, `i` ignores
`search_interval`. Kick/redirect enforce a strict hierarchy
(`s`=2 > `a`=1 > user=0; the actor's rank must be strictly greater).

The console (`console.c`) is a line-oriented telnet login: banner → `login:` →
nick → `password:` (requires `c`) → `> ` prompt. Telnet option negotiation is
answered with `DONT`/`WONT` and stripped from input by `strip_telnet()`
(`net.c:343`); console output uses `\r\n` framing instead of `|`.

### 3.9 Penalties

A penalty is an IP-keyed bitmask (`PENALTY_BANNED|NODL|NOPBLCHAT|NOPRVCHAT|
NOSEARCH`, `penalties.h`) with punisher, nick, start/end dates and a reason.
`#punish <nick> <type> [minutes] [reason]` creates one — `<type>` is a named
punishment (`ban`, `nodl`, `nochat`, `nopblchat`, `noprvchat`, `nosearch`),
duration defaults to `std_penalty_duration`; it is applied at
login (`penalty_welcome` — a `BANNED` match disconnects), enforced inline in the
search/chat/download handlers, and persisted to `dcd.penalties`
(`ip:nick:op:penalties:start:duration:reason\004\n`) on change and periodically
by the cleaner. `#amnesty` clears one; expired entries are dropped on the next
write. `dcd.banned` is a separate *permanent* ban list checked by the cleaner
every cycle (supports `include:` files and IP wildcards).

---

## 4. Configuration

### 4.1 Startup order

1. `init_var()` seeds every default (ports 411/511, `userlimit=1000`, timeouts,
   socket options, `CONFIGDIR`-relative file names) — `main.c:729`.
2. CLI options (`-c <file>`, `-h`, `-v`) via `opt_set`.
3. `parse_config()` reads `dcd.conf` line-by-line; each `option = value` line is
   parsed by `parse_config_line()` and dispatched through `conf_set`.
4. `make_paths()` resolves every auxiliary file name against `CONFIGDIR`.
5. Privilege drop, `log_open()`, message/login/pattern/hublist parsing, mutex
   init, `daemon()`, `penalties_read()`, `make_threads()`.

### 4.2 `dcd.conf`

Format: `name = value`, `#` comments. Directives group as:

* **startup-only** (ignored on rehash, enforced at bind time): `bind_address`,
  `listen_port_main` (also sets the UDP port), `listen_port_cons`,
  `n_listen_main`, `n_user_manager` (inert — see §2.2), `user`, `group`;
* **file pointers**: `conf_welcome`, `conf_motd`, `conf_rules`,
  `conf_usercommands`, `conf_banned`, `conf_cusers`, `conf_callow`,
  `conf_nallow`, `conf_penalties`, `conf_hublinks`;
* **policy**: `hubname`, `user_limit`, `min_share`, `min_slots`,
  `registered_only`, `allow_chat/passive/forwarding/downloads/search`,
  `allow_broken_key`, `allow_non_us_ascii_nicks`, `admin_contact`;
* **timeouts/limits**: `idle_timeout`, `register_timeout`, `search_interval`,
  `max_commands`, `max_receive_once`, `max_chat_length`, `kick_max_chat_length`,
  `minimal_sleep_time`, `penalties_update_interval`, `std_penalty_duration`;
* **redirects**: `redirect_switch` bitmask (access / full / min-share / min-slots)
  + `redirect_*` addresses (sent as `$ForceMove`);
* **logging**: `log_facility` bitmask (see §4.4); legacy `log_*`/`debug_level`
  names are accepted and reported obsolete.

Invalid values for critical directives abort startup (`exit(1)`); on rehash,
`parse_config(0,…)` skips the startup-only ones via the `start` flag in
`conf_param_t`.

### 4.3 Auxiliary files

| File | Format | Reloaded |
|---|---|---|
| `dcd.users` | `nick:password:email:description:permissions` (plain-text passwords); also reserves nicks with `n` | cleaner every 5 s; logins looked up by `bsearch` over a `qsort`ed table |
| `console.allow` | IPs/wildcards, `include:name` (one level) | per console connection |
| `nicks.allow` | one wildcard pattern per line | cleaner every 5 s |
| `dcd.banned` | `IP[:reason]`, wildcards, `include:` | checked by cleaner per user |
| `dcd.penalties` | see §3.9 | read at startup; written on change + periodically |
| `dcd.hublinks` | `ip:port:pass_in:pass_out` | on rehash |
| `dcd.welcome` / `dcd.motd` / `dcd.rules` / `dcd.usercommands` | raw text / raw `$UserCommand` lines | cleaner every 5 s |

Wildcard matching (`match.c`, public-domain IRC matcher) supports `*`, `?`,
legacy `%`, and `\` quoting, case-insensitively; it is used for bans, nick
patterns, and penalty lookups.

### 4.4 Logging

`log_write(facility, priority, fmt, …)` (`log.c`). Facilities: ACCESS, USER,
DEBUG, PROTOCOL, DAEMON, CHAT, NETWORK (`log.h`). Two backends:

* **syslog** (default): each facility maps to a distinct syslog facility
  (`LOG_AUTHPRIV`, `LOG_USER`, `LOG_LOCAL0…`), so operators can route them to
  separate files with syslog rules; `log_priority` filters by priority.
* **files**: `log_facility` bitmask opens one file per facility under `LOG_DIR`.

The PROTOCOL facility logs every TCP/UDP frame sent and received — the primary
debugging tool (and what the E2E suite greps).

### 4.5 Build-time configuration (CMake)

`CMakeLists.txt` (CMake ≥ 3.16, C17):

* cache variables: `DCD_CONFIG_DIR` (default `<prefix>/etc/dcd`, compiled in as
  `CONFIGDIR`), `DEFAULT_USER`/`DEFAULT_GROUP` (compile definitions), `LOG_DIR`;
* compile definitions: `PACKAGE_NAME`, `PACKAGE_VERSION`, `CONFIGDIR`,
  `DEFAULT_USER`, `DEFAULT_GROUP`, `BUILD_TIME`;
* `configure_file(@ONLY)` generates `dcd.conf`, `README`, `dcd.adduser`,
  logrotate and distro spec/init files from `.in` templates;
* default `CMAKE_BUILD_TYPE=Release` (`-O2 -DNDEBUG`) plus
  `-fno-strict-aliasing` (legacy code casts through `char*` buffers);
  optional `DCD_ENABLE_LTO`, `DCD_ENABLE_TSAN`, `DCD_ENABLE_ASAN`;
* the daemon sources (everything except `main.c`) compile once into an OBJECT
  library `dcd_obj`, shared by the binary and the tests;
* install: `dcd` + `dcd.adduser` to `sbin/`, 11 config files to
  `DCD_CONFIG_DIR` (mode 640 for `dcd.users`/`console.allow`), man pages.

---

## 5. Problems found and how they were solved

This tree is upstream 1.1.0 plus a hardening/modernization campaign. The
architecturally significant fixes:

### 5.1 The `FD_SETSIZE` landmine → `poll()` (design change)

`pch.h` defined `FD_SETSIZE = MAXUSERS+30` (5030), but glibc's `<sys/select.h>`
redefines it to 1024 *after* that — with >1023 open descriptors, `FD_SET()` in
the worker wrote past the stack `fd_set` (memory corruption). The "handles 2k
users" claim only held on era-distros with a patched glibc.
**Fix:** the worker loop uses `poll()` with `struct pollfd pfds[MAXUSERS]` and a
parallel `puser[]` mapping back to user indices (`main.c:647`). No fd ceiling up
to `MAXUSERS`, no fd_set bit-array copies.

### 5.2 Data races → one recursive lock (design change)

TSan reported 28 races + 7 use-after-free reports: the five original locks left
the accept, console and UDP paths unsynchronized against the worker.
**Fix:** single recursive `mutex_users` (§2.2); the per-worker claim machinery
(`is_managed()`, `managed[]`) and extra workers were removed — under one lock
they only duplicated work. ASan then localized the "UAF" as a heap
*underflow* in `match()`: a single-char `*` pattern moved the cursor to
`ma-1` and read out of bounds; guarded with `if (m < ma) break;` (`match.c:43`).
Result: TSan E2E run is clean (0 races; only benign thread-leaks at exit).

### 5.3 Signal handler not async-signal-safe

The old `finish()` locked mutexes, logged, `pthread_cancel()`ed and `exit()`ed
from the handler — undefined behavior, and a tautological guard
(`sig!=SIGKILL || sig!=SIGSEGV`) made it re-entrancy-blocked for every signal.
**Fix:** handler sets `volatile sig_atomic_t` flags only; the main loop performs
rehash/shutdown (`do_rehash`/`do_shutdown`). `SIGPIPE` → `SIG_IGN`; the
uncatchable `SIGKILL` handler was dropped.

### 5.4 Command dispatch cost

`bsearch` dispatch re-initialized its table on the stack per call and required
manually sorted arrays (silent breakage on unsorted edits). **Fix:** the
FNV-1a hash dictionary of §3.2 (~10× faster than the old linear scan in
`bench_cmd_dict`; config parsing, the hottest path, uses the same machinery).

### 5.5 Buffer handling

* `add2buf()` (called for every received chunk) did malloc+strlen+strcat×2+free
  full-buffer copies; replaced with in-place `realloc` + single `memcpy`
  (amortized O(1) append, ~8× in `bench_add2buf`); also fixed a latent dangling
  pointer when `MAXBLOCK` was hit.
* `my_vsprintf()`'s growth loop re-used a consumed `va_list` — any output beyond
  8 KiB read garbage; fixed with per-iteration `va_copy()`/`va_end()`.
* `cmd_udp` copied `MAX_RECEIVE_ONCE` bytes into a same-size buffer (no NUL);
  fixed to copy `MAX_RECEIVE_ONCE-1` and force-terminate.
* Raw `sprintf` → bounded `snprintf` at the remaining fixed-buffer sites
  (`$UserIP`, key expansion).

### 5.6 Privilege drop

`setuid()`/`setgid()` return values were ignored (daemon could stay root).
First fix made non-root starts fatal on EPERM — a regression; final form: fatal
only when still euid 0 after the attempt, otherwise warn and continue
(`main.c:1067`).

### 5.7 Smaller but real defects

* `accept()` results checked on an `unsigned int` (`<0` never true) in both
  accept loops — a failed accept passed fd `-1` into `adduser()`; now `int` +
  `socklen_t`.
* `parse_hublist()` leaked the config file handle on every rehash; `fclose()` added.
* `my_duplicate()` tested the wrong pointer (`!duplicate` instead of `!*duplicate`),
  hiding `strdup` OOM; `my_free1()` could deref a NULL `ptr`; `flag_myinfo()`
  indexed a static table with a raw client byte (OOB) — now range-checked.
* `dc_GetNickList` had a stray `;` after an `if` that made the nick-append
  unconditional; `dcd -v` exited with status 1 after success.
* `signal()` → `sigaction(SA_RESTART)`; `inet_addr()` → `inet_pton()` with a
  warning on malformed hub addresses; `getdtablesize()` → `sysconf(_SC_OPEN_MAX)`.
* Removed the debug `usleep(5)` before every `accept()` and the unconditional
  `usleep(2000)` per worker cycle (2–3 ms latency on every message batch);
  pacing is now the `poll()` timeout alone.

### 5.8 `nick2id()` linear scan → nick hash (design change)

Every nick-addressed command (`$To:`, `$Kick`, `#whois`, UDP `$SR`, …) and the
"nick already used" check in `validnick()` resolved nicks by scanning `user[]`
with `strcasecmp` — O(N) per call, up to 5000 comparisons at `MAXUSERS`.
**Fix:** an open-addressing nick→index hash in `misc.c` mirroring the `cmd.c`
style: FNV-1a (ASCII-folded; equality still `strcasecmp`, so non-ASCII nicks
keep their locale semantics), 16384 power-of-two slots (load ≤ 0.5 at
`MAXUSERS`), linear probing, backward-shift deletion (no tombstones).

Invariants: keys alias the owning `userrec_t.nick` string; `user_set_nick()`
is the only nick setter (replaces the five raw `my_duplicate(nick,&usr->nick)`
sites) and updates the hash; `userlist_del()` drops the removed user's key and
re-homes the index of the user swapped into its slot; `disconnect()` forgets
the key before the string is freed. The hash is guarded by the same
`mutex_users` as the user table. `bench_nick2id`: ~110 Mops/s vs ~1.1 Mops/s
for the scan at 1000 users (~100×).

### 5.9 Known protocol quirks (documented, not "fixed")

* `$Hello`/`$LogedIn` are **unicast** to the joining client; other users learn of
  a new user only via the `$MyINFO` broadcast (standard NMDC clients treat
  `$Hello` as broadcast).
* The `$To:` comment in `cmd.dc.c` documents a `$<<nick>>` form that does not
  parse; the working form is single-bracket `$<nick>`, which is also what
  `privmsg()` emits.
* A silently-closed registered socket is only noticed via `idle_timeout` /
  `register_timeout` — `my_recv()` treats EOF as an empty message and `POLLHUP`
  is not acted on. Production configs should set `idle_timeout` (e.g. 120).
* Rare message loss under load (~1/25 connections in full-suite runs) was
  observed once: `send()` reported full length with errno=0 yet frames after the
  banner never reached the client; not reproducible in isolation.

---

## 6. Known limitations / open issues

* `add2buf()` still runs `strlen(*buf)` per append (aggregate quadratic for a
  long unterminated stream); fixing needs per-buffer length tracking (API change).
* Hub linking is UDP-only with no TCP fallback; passwords in `dcd.users` and
  `dcd.hublinks` are plain text; no TLS, no IPv6 (both on the upstream TODO).
* Quit/kick visibility is delayed up to `CLEANER_INTERVAL` (5 s) by design.
* `n_user_manager` is accepted but inert (single worker under one lock).
* Threads are cancelled asynchronously at shutdown; the process exits without
  joining (the TSan "thread-leak" reports are expected and benign).

---

## 7. Testing infrastructure

* **Unit + perf tests** (`tests/`, run by `ctest --test-dir build`): a
  dependency-free harness (`tests/test.h`); `tests/globals.c`/`stubs.c` provide
  the `main.c`-owned globals so test binaries link against `dcd_obj` without
   `main()`. Suites: `test_cmd_dict`, `test_match`, `test_misc` (incl. the
   nick-hash suite), `test_myinfo`;
   perf floors: `bench_cmd_dict`, `bench_add2buf`, `bench_match`,
   `bench_nick2id` (ctest label
  `perf`).
* **End-to-end suite** (`tests/e2e/`): `nmdc.py` is a dependency-free NMDC
  client (frame parser, out-of-order inbox, `compute_key()` implementing the
  lock→key derivation, `ConsoleClient`); `test_e2e.py` runs 25 protocol/console
  scenarios against a real daemon started by `run_e2e.sh` in a writable install
  prefix.
* **Sanitizers**: `-DDCD_ENABLE_TSAN=ON` / `-DDCD_ENABLE_ASAN=ON` instrument the
  binary only; `DCD_SANITIZER=thread|address tests/e2e/run_e2e.sh` runs the E2E
  suite against an instrumented build and prints the sanitizer findings
  (`log_path` is used because `daemon()` sends stderr to `/dev/null`).

---

## 8. Operational model

* Start: `dcd [-c config]` — daemonizes; logs via syslog by default.
* Reconfigure: `SIGHUP` or `#rehash` (chat/console) — reloads everything except
  startup-only settings; the hub stays up.
* Stop: `SIGTERM` (or `SIGINT`/`SIGQUIT`) — cancels threads, disconnects users,
  closes listeners.
* Administration: telnet console on `listen_port_cons` (default 511, restricted
  by `console.allow` + `c` permission) or `#commands` in the DC chat window.
* `dcd.adduser` is an interactive helper that appends accounts to `dcd.users`.
