"""NMDC (Nano Message Direct Connect) client for end-to-end testing of DCD.

Protocol as actually implemented by DCD (verified by tests/e2e):

  * Every client<->hub frame is a single line terminated by '|'.
  * Hub -> client frames:
      $Lock EXTENDEDPROTOCOL<99 chars> Pk=DCONNECT..DAEMON|  banner on accept
      $HubName <name>|                  sent right after $Lock
      <HUB> <text>|                     welcome message / status / error
      $Hello <nick>|                    sent ONLY to the joining client
      $LogedIn <nick>|                  sent ONLY to the operator logging in
      $GetPass|                         nick requires a password
      $BadPass|                         wrong password, then disconnect
      $NickList n1$$n2$$...|            answer to $GetNickList
      $OpList o1$$o2$$...|              operators only
      $UserIP <nick> <ip>|              with UserIP2 support
      $MyINFO $ALL <nick> ...$|         broadcast to all registered users
      $Quit <nick>|                     user left (sent when cleaner reaps)
      $ForceMove <addr>|                redirect
      $Supports <tokens>|               echo of supported extensions
      <nick> <text>|                    public chat (rebroadcast to ALL
                                        registered users, sender included)
      $To: <me> From: <src> $<src> <text>|   private message
  * Client -> hub frames:
      $Key <derived key>|               see compute_key (NMDC lock algorithm)
      $ValidateNick <nick>|, $MyPass <pass>|,
      $MyINFO $ALL <nick> <desc>$ $<ctype><flag>$<email>$<share>$|,
      $Supports <tokens>|, $GetNickList|, $Search <ip>:<port> <opts>|,
      $ConnectToMe <nick> <ip>:<port>|, $RevConnectToMe <a> <b>|,
      $To: <dst> From: <src> $<src> <text>|   (single brackets: the $<<x>>
          form in the source comment does NOT parse in dc_To),
      $Kick <nick>|, <nick> <text>| (public chat), $Quit <nick>|

  * Hub messages ($Quit broadcast, socket close after any rejection/kick)
    arrive only after the cleaner thread runs (every 5s, CLEANER_INTERVAL).
"""

import errno
import select
import socket


class NMDCError(Exception):
    pass


class NMDCDisconnected(NMDCError):
    pass


_PROTOCOL_CHARS = {0, 5, 124, 96, 126, 36}
SALT_LEN = 100


def compute_key(lock):
    """Derive the $Key answer from the $Lock salt (DCD src/misc.c lock_key)."""
    tmp = []
    save = 5
    for i in range(SALT_LEN - 1):
        c = ord(lock[i]) if i < len(lock) else 0
        t = c ^ save
        t = ((t & 0x0F) << 4) | ((t & 0xF0) >> 4)
        tmp.append(t & 0xFF)
        save = c
    tmp[0] ^= tmp[SALT_LEN - 2]

    out = []
    for b in tmp:
        signed = b - 256 if b > 127 else b
        if b in _PROTOCOL_CHARS:
            out.append("/%%DCN%03i%%/" % signed)
        else:
            out.append(chr(b))
    return "".join(out)


class Message:
    """One parsed NMDC frame."""

    def __init__(self, raw):
        self.raw = raw
        if raw.startswith("$"):
            self.kind = "dc"
            cmd, _, arg = raw[1:].partition(" ")
            self.cmd = cmd
            self.arg = arg
        elif raw.startswith("<"):
            nick, _, text = raw[1:].partition("> ")
            self.kind = "hub" if nick == "HUB" else "chat"
            self.cmd = nick
            self.arg = text
        else:
            self.kind = "text"
            self.cmd = ""
            self.arg = raw

    def __repr__(self):
        return f"Message({self.raw!r})"


class NMDCClient:
    """Blocking NMDC client with a small inbox for out-of-order frames."""

    def __init__(self, host="127.0.0.1", port=41111, timeout=5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = None
        self.inbox = []
        self.lock = None
        self.hubname = None
        self.nick = None
        self._buf = b""

    # -- connection ---------------------------------------------------------

    def connect(self):
        self.sock = socket.create_connection((self.host, self.port), self.timeout)
        self.sock.settimeout(self.timeout)
        return self

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None

    def __enter__(self):
        return self.connect()

    def __exit__(self, *exc):
        self.close()

    # -- raw io -------------------------------------------------------------

    def send(self, line):
        if not line.endswith("|"):
            line += "|"
        self.sock.sendall(line.encode("latin-1"))

    def _read_until(self, delim=b"|"):
        while delim not in self._buf:
            try:
                chunk = self.sock.recv(4096)
            except socket.timeout:
                raise NMDCError(f"timeout waiting for frame (buffered: {self._buf!r})")
            except ConnectionResetError:
                raise NMDCDisconnected("connection reset by hub")
            if not chunk:
                raise NMDCDisconnected("connection closed by hub")
            self._buf += chunk
        head, _, self._buf = self._buf.partition(delim)
        return head.decode("latin-1")

    # -- framed io ----------------------------------------------------------

    def _recv_socket(self, timeout=None):
        if timeout is not None and self.sock:
            self.sock.settimeout(timeout)
        try:
            raw = self._read_until()
        finally:
            if self.sock:
                self.sock.settimeout(self.timeout)
        return Message(raw)

    def recv(self, timeout=None):
        """Return the next Message (inbox first). Raises NMDCDisconnected on EOF."""
        if self.inbox:
            return self.inbox.pop(0)
        return self._recv_socket(timeout)

    def expect(self, pred, timeout=5.0):
        """Wait for a frame matching pred(cmd, arg) or a Message predicate.

        pred may be a string (exact dc command name), a callable taking a
        Message, or a (cmd, substring) tuple. Returns the matching Message.
        Non-matching frames are parked in the inbox for later recv().
        """
        if isinstance(pred, str):
            match = lambda m: m.kind == "dc" and m.cmd == pred
        elif isinstance(pred, tuple):
            cmd, sub = pred
            match = lambda m: m.kind == "dc" and m.cmd == cmd and sub in m.arg
        else:
            match = pred

        import time
        for i, m in enumerate(self.inbox):
            if match(m):
                return self.inbox.pop(i)

        deadline = time.monotonic() + timeout
        while True:
            try:
                m = self._recv_socket(timeout=max(0.05, deadline - time.monotonic()))
            except NMDCError:
                raise NMDCError(f"expected frame not received before timeout")
            if match(m):
                return m
            self.inbox.append(m)
            if time.monotonic() >= deadline:
                raise NMDCError(f"expected frame not received before timeout")

    def expect_closed(self, timeout=8.0):
        """Assert the hub closes the connection (e.g. after a kick).

        DCD's cleaner thread reaps QUIT-state users every CLEANER_INTERVAL
        (5s), so allow a generous window.
        """
        import time
        deadline = time.monotonic() + timeout
        while True:
            try:
                m = self._recv_socket(timeout=max(0.05, deadline - time.monotonic()))
            except NMDCDisconnected:
                return
            except NMDCError:
                raise NMDCError("connection still open, expected disconnect")
            self.inbox.append(m)
            if time.monotonic() >= deadline:
                raise NMDCError("connection still open, expected disconnect")

    def drain(self, quiet=0.3):
        """Collect everything the hub sends within a quiet window."""
        out = []
        while True:
            try:
                out.append(self.recv(timeout=quiet))
            except (NMDCError, NMDCDisconnected):
                return out

    # -- protocol helpers ---------------------------------------------------

    def handshake(self):
        """Read the banner ($Lock, $HubName) and answer $Key with the
        derived key (NMDC lock algorithm, see compute_key)."""
        lock_msg = self.expect("Lock", timeout=self.timeout)
        self.lock = lock_msg.arg.split(" ")[0]
        hub = self.expect("HubName", timeout=self.timeout)
        self.hubname = hub.arg
        self.send(f"$Key {compute_key(self.lock)}")
        return self.lock

    def validate_nick(self, nick):
        self.nick = nick
        self.send(f"$ValidateNick {nick}")

    def myinfo(self, desc="e2e client", ctype="DSL", flag=1,
               email="e2e@example.com", share=1024 * 1024 * 1024):
        # flag is a raw protocol byte (valid range 1..11), not an ASCII digit
        self.send(f"$MyINFO $ALL {self.nick} {desc}$ ${ctype}{chr(flag)}${email}${share}$")

    def login(self, nick, password=None, desc="e2e client"):
        """Full registration: handshake, ValidateNick, optional MyPass, MyINFO.

        Note: DCD sends $Hello/$LogedIn only to the joining client; other
        users learn about the new user via the $MyINFO broadcast.
        """
        self.handshake()
        self.validate_nick(nick)
        if password is not None:
            self.expect("GetPass")
            self.send(f"$MyPass {password}")
        self.expect("Hello")
        self.myinfo(desc=desc)
        # wait for our own MyINFO broadcast: it only happens once the hub
        # has moved us to REGISTERED, so later commands see a registered user
        self.expect(lambda m: m.kind == "dc" and m.cmd == "MyINFO" and self.nick in m.arg)

    def chat(self, text):
        self.send(f"<{self.nick}> {text}")

    def pm(self, to, text):
        # DCD's dc_To() strtok chain only yields a matching nick2 for the
        # single-bracket form; the $<<nick>> form in the source comment parses
        # as "<nick" and is rejected as a fake nick.
        self.send(f"$To: {to} From: {self.nick} $<{self.nick}> {text}")

    def search(self, pattern, opts="F?F?0?1?", ip="127.0.0.1", port=None):
        port = port or self.port
        self.send(f"$Search {ip}:{port} {opts}{pattern}")

    def connect_to_me(self, nick, ip="127.0.0.1", port=0):
        self.send(f"$ConnectToMe {nick} {ip}:{port}")

    def rev_connect_to_me(self, target):
        self.send(f"$RevConnectToMe {self.nick} {target}")

    def kick(self, victim):
        self.send(f"$Kick {victim}")

    def get_nicklist(self):
        self.send("$GetNickList")

    def supports(self, *tokens):
        self.send("$Supports " + " ".join(tokens))

    def quit(self):
        if self.nick:
            self.send(f"$Quit {self.nick}")


class ConsoleClient:
    """DCD remote console (TCP, line based, CRLF)."""

    def __init__(self, host="127.0.0.1", port=41511, timeout=5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = None
        self._buf = b""

    def connect(self):
        self.sock = socket.create_connection((self.host, self.port), self.timeout)
        self.sock.settimeout(self.timeout)
        return self

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None

    def __enter__(self):
        return self.connect()

    def __exit__(self, *exc):
        self.close()

    def _read_some(self):
        try:
            chunk = self.sock.recv(4096)
        except socket.timeout:
            raise NMDCError("console read timeout")
        except ConnectionResetError:
            raise NMDCDisconnected("console connection reset by hub")
        if not chunk:
            raise NMDCDisconnected("console connection closed")
        self._buf += chunk

    def read_until(self, marker, timeout=5.0):
        """Read until marker (bytes) appears; returns decoded text so far."""
        import time
        deadline = time.monotonic() + timeout
        while marker not in self._buf:
            self.sock.settimeout(max(0.05, deadline - time.monotonic()))
            self._read_some()
            if time.monotonic() >= deadline:
                break
        out, self._buf = self._buf, b""
        return out.decode("latin-1")

    def login(self, nick, password):
        banner = self.read_until(b"login:")
        self.sock.sendall((nick + "\r\n").encode())
        self.read_until(b"password:")
        self.sock.sendall((password + "\r\n").encode())
        return banner, self.read_until(b"> ")

    def command(self, cmd):
        self.sock.sendall((cmd + "\r\n").encode())
        return self.read_until(b"> ")

    def quit(self):
        try:
            self.sock.sendall(b"quit\r\n")
        except OSError:
            return
        # wait for the hub to process quit and close the console session
        try:
            while True:
                self.sock.settimeout(6.0)
                if not self.sock.recv(4096):
                    break
        except (OSError, NMDCError):
            pass
