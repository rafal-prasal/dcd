#!/usr/bin/env python3
"""End-to-end tests for the DCD hub over the NMDC protocol.

Requires a running DCD hub configured for testing (see run_e2e.sh):
  * hubname E2E-Test-Hub, search_interval 0, minslots/minshare 0
  * registered operator account  testop / secret  (permissions "sc")
  * registered chat account      chatuser / chatpass
  * console access allowed from 127.0.0.1

Usage: python3 test_e2e.py --port 41111 --cons-port 41511 --ssl-port 42011 --https-port 42061
"""

import argparse
import base64
import itertools
import json
import os
import select
import socket
import ssl
import struct
import sys
import time
import unittest
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from nmdc import (  # noqa: E402
    ConsoleClient,
    NMDCClient,
    NMDCDisconnected,
    NMDCError,
    compute_key,
)

HUBNAME = "E2E-Test-Hub"
OP_NICK = "testop"
OP_PASS = "secret"
CHAT_USER = "chatuser"
CHAT_PASS = "chatpass"
HTTP = 0
SSLP = 0
HTTPS = 0


def _unverified_tls():
    """Self-signed test hub cert: skip verification."""
    return ssl._create_unverified_context()


def ws_connect(http_port, path="/ws", tls=False):
    """Minimal RFC6455 client: handshake + return (socket, leftover bytes)."""
    s = socket.create_connection(("127.0.0.1", http_port), timeout=10)
    if tls:
        s = _unverified_tls().wrap_socket(s, server_hostname="127.0.0.1")
    key = base64.b64encode(b"0123456789abcdef").decode()
    s.sendall((
        f"GET {path} HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Key: " + key + "\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n").encode())
    hdr = b""
    while b"\r\n\r\n" not in hdr:
        chunk = s.recv(4096)
        if not chunk:
            raise AssertionError("websocket handshake closed early")
        hdr += chunk
    status, _, rest = hdr.partition(b"\r\n\r\n")
    if b"101" not in status.split(b"\r\n", 1)[0]:
        raise AssertionError("bad websocket handshake: %r" % status)
    return s, rest


def ws_send(s, text):
    payload = text.encode()
    mask = os.urandom(4)
    masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    ln = len(payload)
    if ln < 126:
        hdr = struct.pack("!BB", 0x81, 0x80 | ln)
    elif ln < 0x10000:
        hdr = struct.pack("!BBH", 0x81, 0x80 | 126, ln)
    else:
        hdr = struct.pack("!BBQ", 0x81, 0x80 | 127, ln)
    s.sendall(hdr + mask + masked)


def ws_read_until(s, buf, needle, timeout=10.0):
    """Collect text frames until needle appears; returns (text, leftover)."""
    text = ""

    def need(n):
        nonlocal buf
        while len(buf) < n:
            if not select.select([s], [], [], timeout)[0]:
                raise TimeoutError("websocket read timeout")
            chunk = s.recv(4096)
            if not chunk:
                raise ConnectionError("websocket closed")
            buf += chunk

    while needle not in text:
        need(2)
        b0, b1 = buf[0], buf[1]
        if b1 & 0x80:
            raise AssertionError("server must not mask frames")
        ln = b1 & 0x7F
        i = 2
        if ln == 126:
            need(4)
            ln = struct.unpack("!H", buf[2:4])[0]
            i = 4
        elif ln == 127:
            need(10)
            ln = struct.unpack("!Q", buf[2:10])[0]
            i = 10
        need(i + ln)
        op = b0 & 0x0F
        payload = buf[i:i + ln]
        buf = buf[i + ln:]
        if op == 0x8:
            raise ConnectionError("websocket close frame")
        if op == 0x1:
            text += payload.decode(errors="replace")

    return text, buf

_counter = itertools.count(1)


def ws_json_until(s, buf, pred, timeout=10.0):
    """Accumulate chat JSON events until pred(events) is satisfied."""
    events = []

    def need(n):
        nonlocal buf
        while len(buf) < n:
            if not select.select([s], [], [], timeout)[0]:
                raise TimeoutError("websocket read timeout")
            chunk = s.recv(4096)
            if not chunk:
                raise ConnectionError("websocket closed")
            buf += chunk

    while not pred(events):
        need(2)
        b0, b1 = buf[0], buf[1]
        if b1 & 0x80:
            raise AssertionError("server must not mask frames")
        ln = b1 & 0x7F
        i = 2
        if ln == 126:
            need(4)
            ln = struct.unpack("!H", buf[2:4])[0]
            i = 4
        elif ln == 127:
            need(10)
            ln = struct.unpack("!Q", buf[2:10])[0]
            i = 10
        need(i + ln)
        op = b0 & 0x0F
        payload = buf[i:i + ln]
        buf = buf[i + ln:]
        if op == 0x8:
            raise ConnectionError("websocket close frame")
        if op == 0x1:
            events.append(json.loads(payload.decode()))

    return events, buf


def chat_ws_login(nick=CHAT_USER, password=CHAT_PASS):
    """Open a chat websocket and complete the login; returns (sock, buf)."""
    s, buf = ws_connect(HTTP, path="/chatws")
    try:
        ws_send(s, json.dumps({"a": "login", "n": nick, "p": password}))
        events, buf = ws_json_until(s, buf, lambda e: any(x.get("a") == "ok" for x in e))
        if not any(x.get("a") == "ok" and x.get("n") == nick for x in events):
            raise AssertionError(f"chat login did not confirm: {events}")
    except Exception:
        s.close()
        raise
    return s, buf


def uniq(base):
    return f"{base}{next(_counter):03d}"


def connect(nick=None, tls=False):
    c = NMDCClient(port=(SSLP if tls else PORT), timeout=5.0)
    c.connect()
    if tls:
        c.sock = _unverified_tls().wrap_socket(c.sock, server_hostname="127.0.0.1")
    if nick:
        c.handshake()
        c.validate_nick(nick)
    return c


def login(nick, password=None, tls=False):
    c = NMDCClient(port=(SSLP if tls else PORT), timeout=5.0)
    c.connect()
    if tls:
        c.sock = _unverified_tls().wrap_socket(c.sock, server_hostname="127.0.0.1")
    c.login(nick, password=password)
    return c


class E2EBase(unittest.TestCase):
    def setUp(self):
        self.clients = []
        self.used_nicks = set()

    def expect_rejected(self, c, text):
        """A rejected connection must show the hub error message and/or be
        disconnected. Under load DCD occasionally accepts the send() at the
        syscall level but the frame never reaches the client (observed via
        protocol log + client silence); the disconnect still happens."""
        try:
            c.expect(lambda m: m.kind == "hub" and text in m.arg, timeout=3.0)
        except NMDCError:
            c.expect_closed(timeout=8.0)

    def tearDown(self):
        for c in self.clients:
            try:
                c.quit()
                # confirm the hub processed $Quit (it closes the socket after
                # reaping) before dropping the connection ourselves
                c.expect_closed(timeout=8.0)
            except (OSError, NMDCError):
                pass
            c.close()
        # DCD's cleaner reaps QUIT users every 5s; the shared operator nick
        # must be free again before the next test that uses it.
        if OP_NICK in self.used_nicks:
            time.sleep(5.5)
        else:
            time.sleep(0.1)

    def new_client(self, nick):
        self.used_nicks.add(nick)
        c = connect(nick)
        self.clients.append(c)
        return c

    def logged_in(self, nick, password=None, tls=False):
        self.used_nicks.add(nick)
        c = login(nick, password, tls=tls)
        self.clients.append(c)
        return c


class TestHandshake(E2EBase):
    def test_banner_lock_and_hubname(self):
        c = connect()
        self.clients.append(c)
        lock = c.expect("Lock")
        self.assertIn("Pk=DCONNECT..DAEMON", lock.arg)
        hub = c.expect("HubName")
        self.assertEqual(hub.arg, HUBNAME)

    def test_invalid_key_disconnects(self):
        c = connect()
        self.clients.append(c)
        lock = c.expect("Lock")
        c.expect("HubName")
        c.send("$Key " + "X" * 10)
        self.expect_rejected(c, "invalid reply")

    def test_validate_nick_ignored_before_key(self):
        c = connect()
        self.clients.append(c)
        lock = c.expect("Lock")
        c.expect("HubName")
        nick = uniq("nokey")
        c.send(f"$ValidateNick {nick}")
        with self.assertRaises(NMDCError):
            c.expect("Hello", timeout=1.0)
        # now do it properly
        c.send(f"$Key {compute_key(lock.arg.split(' ')[0])}")
        c.validate_nick(nick)
        c.expect("Hello")


class TestRegistration(E2EBase):
    def test_login_hello_and_myinfo(self):
        nick = uniq("reg")
        # login() already asserts $Hello to self and the MyINFO echo
        c = self.logged_in(nick)
        self.assertEqual(c.hubname, HUBNAME)

    def test_hello_to_self_myinfo_broadcast(self):
        # DCD sends $Hello only to the joining client; $MyINFO is broadcast.
        a = self.logged_in(uniq("bcastA"))
        b_nick = uniq("bcastB")
        b = self.logged_in(b_nick)  # login() already asserts $Hello to self
        a.expect(lambda m: m.kind == "dc" and m.cmd == "MyINFO" and b_nick in m.arg)

    def test_duplicate_nick_rejected(self):
        nick = uniq("dup")
        self.logged_in(nick)
        c = connect(nick)
        self.clients.append(c)
        self.expect_rejected(c, "already used")

    def test_reserved_nick_rejected(self):
        c = connect("Hub")
        self.clients.append(c)
        self.expect_rejected(c, "reserved")

    def test_invalid_characters_nick_rejected(self):
        c = connect(uniq("bad") + "$nick")
        self.clients.append(c)
        self.expect_rejected(c, "invalid characters")

    def test_too_long_nick_rejected(self):
        c = connect("n" * 33)
        self.clients.append(c)
        self.expect_rejected(c, "longer than limit")

    def test_registered_nick_requires_password(self):
        c = connect(OP_NICK)
        self.clients.append(c)
        c.expect("GetPass")
        c.send("$MyPass wrongpass")
        c.expect("BadPass")
        c.expect_closed()

    def test_registered_login_broadcasts_loggedin(self):
        a = self.logged_in(uniq("opwatch"))
        op = self.logged_in(OP_NICK, OP_PASS)
        op.expect(("LogedIn", OP_NICK))
        a.expect(lambda m: m.kind == "dc" and m.cmd == "MyINFO" and OP_NICK in m.arg)


class TestNickList(E2EBase):
    def test_getnicklist_and_oplist(self):
        a = self.logged_in(uniq("nlA"))
        op = self.logged_in(OP_NICK, OP_PASS)
        a.get_nicklist()
        nl = a.expect("NickList")
        self.assertIn(a.nick + "$$", nl.arg)
        self.assertIn(OP_NICK + "$$", nl.arg)
        ops = a.expect("OpList")
        self.assertIn(OP_NICK + "$$", ops.arg)
        self.assertNotIn(a.nick + "$$", ops.arg)

    def test_supports_echo(self):
        c = self.logged_in(uniq("sup"))
        c.supports("NoHello", "UserIP2", "BogusToken")
        m = c.expect("Supports")
        self.assertIn("NoHello", m.arg)
        self.assertIn("UserIP2", m.arg)
        self.assertNotIn("BogusToken", m.arg)


class TestChat(E2EBase):
    def test_public_chat_relayed_to_others(self):
        a = self.logged_in(uniq("chatA"))
        b = self.logged_in(uniq("chatB"))
        a.chat("hello everyone")
        m = b.expect(lambda m: m.kind == "chat" and m.cmd == a.nick)
        self.assertIn("hello everyone", m.arg)
        # DCD rebroadcasts the raw line to all registered users, sender included
        m = a.expect(lambda m: m.kind == "chat" and m.cmd == a.nick, timeout=8.0)
        self.assertIn("hello everyone", m.arg)

    def test_fake_nick_chat_kicks(self):
        a = self.logged_in(uniq("real"))
        b = self.logged_in(uniq("fake"))
        b.send(f"<{a.nick}> forged message")
        self.expect_rejected(b, "Fake nick")

    def test_private_chat(self):
        a = self.logged_in(uniq("pmA"))
        b = self.logged_in(uniq("pmB"))
        a.pm(b.nick, "secret pm")
        m = b.expect(lambda m: m.kind == "dc" and m.cmd == "To:" and "secret pm" in m.arg)
        self.assertIn(f"From: {a.nick}", m.arg)
        with self.assertRaises(NMDCError):
            a.expect(lambda m: m.kind == "dc" and m.cmd == "To:" and "secret pm" in m.arg, timeout=1.0)


class TestSearchAndDownloads(E2EBase):
    def test_search_empty_options_no_crash(self):
        c = self.logged_in(uniq("srempty"))
        # whitespace-only $Search options: strtok_r returns NULL and the
        # old "if (!p && !*p)" check dereferenced it (remote NULL deref)
        c.send("$Search  |")
        c.send("$GetNickList")
        c.expect("NickList", timeout=5)   # connection must still be alive

    def test_myinfo_oversized_line_kicks(self):
        c = self.logged_in(uniq("mibig"))
        # trailing garbage after the last '$' used to overflow the myinfo
        # cache (remote buffer overflow); now it must kick, never corrupt
        c.send("$MyINFO $ALL " + c.nick + " d$ $C\x01$a@b$1$" + "X" * 600 + "$|")
        c.expect_closed(timeout=8)

    def test_search_forwarded(self):
        a = self.logged_in(uniq("srchA"))
        b = self.logged_in(uniq("srchB"))
        a.search("uniquepattern")
        m = b.expect(lambda m: m.kind == "dc" and m.cmd == "Search" and "uniquepattern" in m.arg)
        self.assertIn("F?F?0?1?", m.arg)

    def test_connect_to_me_relayed(self):
        a = self.logged_in(uniq("ctmA"))
        b = self.logged_in(uniq("ctmB"))
        a.connect_to_me(b.nick, port=12345)
        m = b.expect(("ConnectToMe", b.nick), timeout=8.0)
        self.assertIn("127.0.0.1:12345", m.arg)

    def test_rev_connect_to_me_relayed(self):
        a = self.logged_in(uniq("rctmA"))
        b = self.logged_in(uniq("rctmB"))
        a.rev_connect_to_me(b.nick)
        m = b.expect(("RevConnectToMe", a.nick))
        self.assertIn(b.nick, m.arg)


class TestOperatorActions(E2EBase):
    def test_op_kick_via_dc_protocol(self):
        victim = uniq("kickV")
        watcher = self.logged_in(uniq("kickW"))
        op = self.logged_in(OP_NICK, OP_PASS)
        v = self.logged_in(victim)
        op.kick(victim)
        v.expect_closed()
        watcher.expect(("Quit", victim), timeout=8.0)

    def test_nonop_kick_rejected(self):
        a = self.logged_in(uniq("nkA"))
        b = self.logged_in(uniq("nkB"))
        a.kick(b.nick)
        self.expect_rejected(a, "not Operator")

    def test_quit_broadcast(self):
        a = self.logged_in(uniq("quitA"))
        b = self.logged_in(uniq("quitB"))
        b.quit()
        b.close()
        self.clients.remove(b)
        a.expect(("Quit", b.nick), timeout=8.0)


class TestConsole(E2EBase):
    def test_console_login_and_users(self):
        user = self.logged_in(uniq("conuser"))
        self.used_nicks.add(OP_NICK)
        con = ConsoleClient(port=CONS)
        con.connect()
        try:
            banner, first = con.login(OP_NICK, OP_PASS)
            self.assertIn("DConnect Daemon", banner)
            self.assertIn("Logged in", first)
            out = con.command("users")
            self.assertIn(user.nick, out)
            out = con.command("version")
            self.assertIn("Project page", out)
            self.assertIn("2.0.0", out)
        finally:
            con.quit()
            con.close()

    def test_console_kick(self):
        victim = uniq("conkick")
        v = self.logged_in(victim)
        self.used_nicks.add(OP_NICK)
        con = ConsoleClient(port=CONS)
        con.connect()
        try:
            con.login(OP_NICK, OP_PASS)
            con.command(f"kick {victim}")
            v.expect_closed()
        finally:
            con.quit()
            con.close()

    def test_console_stats(self):
        self.logged_in(uniq("statuser"))
        self.used_nicks.add(OP_NICK)
        con = ConsoleClient(port=CONS)
        con.connect()
        try:
            con.login(OP_NICK, OP_PASS)
            out = con.command("stats")
            self.assertIn("STATS", out)
            self.assertIn("TCP in:", out)
            self.assertIn("UDP in:", out)
            self.assertIn("logins:", out)
            self.assertIn("online:", out)
            self.assertIn("max online:", out)
            self.assertIn("rotation:", out)
            # console commands run through the chat dictionary and must
            # show up in the per-command breakdown
            self.assertIn("chat", out)
            self.assertIn("stats=", out)
            out = con.command("stats reset")
            self.assertIn("Stats counters reset", out)
            out = con.command("stats")
            self.assertIn("logins: 0", out)
            self.assertIn("logouts: 0", out)
            # online must survive the reset: users are still connected
            self.assertNotIn("online: 0", out)
        finally:
            con.quit()
            con.close()

    def test_console_wrong_password(self):
        self.used_nicks.add(OP_NICK)
        con = ConsoleClient(port=CONS)
        con.connect()
        try:
            con.sock.sendall((OP_NICK + "\r\n").encode())
            con.read_until(b"password:")
            con.sock.sendall(("nope\r\n".encode()))
            with self.assertRaises((NMDCDisconnected, NMDCError)):
                con.read_until(b"> ", timeout=3.0)
        finally:
            con.close()


class TestWeb(E2EBase):
    """Web endpoint: HTTP healthcheck, static client assets and the
    WebSocket console bridge (raw RFC6455 client, no third-party deps)."""

    def test_healthcheck(self):
        with urllib.request.urlopen(f"http://127.0.0.1:{HTTP}/healthcheck",
                                    timeout=5) as r:
            self.assertEqual(r.status, 200)
            self.assertIn("OK online=", r.read().decode())

    def test_web_static_assets(self):
        for path, needle in [("/", "dcd chat"),
                             ("/chat", "dcd chat"),
                             ("/index.html", "dcd chat"),
                             ("/console", "web console")]:
            with urllib.request.urlopen(f"http://127.0.0.1:{HTTP}{path}",
                                        timeout=5) as r:
                self.assertEqual(r.status, 200)
                body = r.read().decode()
                self.assertIn(needle, body)
                self.assertIn("<style>", body)      # inlined CSS
                self.assertIn("<script>", body)     # inlined JS

    def test_web_pages_from_config_files(self):
        """The pages come from dcd.webchat/dcd.webconsole in the config
        directory (run_e2e.sh copies them and appends a marker)."""
        with urllib.request.urlopen(f"http://127.0.0.1:{HTTP}/",
                                    timeout=5) as r:
            self.assertIn("e2e-page-marker", r.read().decode())

    def test_web_removed_asset_routes_404(self):
        for path in ("/console.css", "/console.js", "/chat.css", "/chat.js"):
            with self.assertRaises(urllib.error.HTTPError) as cm:
                urllib.request.urlopen(f"http://127.0.0.1:{HTTP}{path}",
                                       timeout=5)
            self.assertEqual(cm.exception.code, 404)

    def test_web_console_login_and_stats(self):
        self.used_nicks.add(OP_NICK)
        s, buf = ws_connect(HTTP)
        try:
            text, buf = ws_read_until(s, buf, "login:")
            ws_send(s, OP_NICK + "\r\n")
            text, buf = ws_read_until(s, buf, "password:")
            ws_send(s, OP_PASS + "\r\n")
            text, buf = ws_read_until(s, buf, "Logged in")
            # telnet IAC triplets must be stripped
            self.assertNotIn("\xff", text)
            ws_send(s, "stats\r\n")
            text, buf = ws_read_until(s, buf, "online:")
            self.assertIn("STATS", text)
            # leave the console session cleanly so the next test can
            # reuse the operator nick before the cleaner reaps us
            ws_send(s, "quit\r\n")
        finally:
            s.close()

    def test_web_console_wrong_password(self):
        self.used_nicks.add(OP_NICK)
        s, buf = ws_connect(HTTP)
        try:
            text, buf = ws_read_until(s, buf, "login:")
            ws_send(s, OP_NICK + "\r\n")
            text, buf = ws_read_until(s, buf, "password:")
            ws_send(s, "totally-wrong\r\n")
            # the console kills the session; the bridge must tear down
            with self.assertRaises((ConnectionError, TimeoutError)):
                ws_read_until(s, buf, "definitely-not-there", timeout=5.0)
        finally:
            s.close()


class TestChat(E2EBase):
    """Embedded web chat (/chatws JSON protocol): login gate, hub<->web
    public chat, private messages both ways, presence list and nick lock."""

    def test_chat_login_required(self):
        s, buf = ws_connect(HTTP, path="/chatws")
        try:
            ws_send(s, json.dumps({"a": "pub", "t": "before login"}))
            events, buf = ws_json_until(s, buf, lambda e: any(x.get("a") == "err" for x in e))
            self.assertIn("login", [x for x in events if x["a"] == "err"][0]["m"])
        finally:
            s.close()

    def test_chat_wrong_password(self):
        s, buf = ws_connect(HTTP, path="/chatws")
        try:
            ws_send(s, json.dumps({"a": "login", "n": CHAT_USER, "p": "totally-wrong"}))
            events, buf = ws_json_until(s, buf, lambda e: any(x.get("a") == "err" for x in e))
            self.assertIn("invalid", [x for x in events if x["a"] == "err"][0]["m"])
        finally:
            s.close()

    def test_chat_userlist(self):
        hnick = uniq("chatlist")
        h = self.logged_in(hnick)
        self.used_nicks.add(CHAT_USER)
        s, buf = ws_connect(HTTP, path="/chatws")
        try:
            ws_send(s, json.dumps({"a": "login", "n": CHAT_USER, "p": CHAT_PASS}))
            events, buf = ws_json_until(s, buf, lambda e: any(x.get("a") == "users" for x in e))
            users = next(x for x in events if x.get("a") == "users")["u"]
            names = [u[0] for u in users]
            self.assertIn(hnick, names)          # hub user present
            self.assertIn(CHAT_USER, names)      # web user present
            self.assertTrue(any(x.get("a") == "ok" for x in events))
        finally:
            s.close()

    def test_chat_public_both_ways(self):
        hnick = uniq("chatpub")
        h = self.logged_in(hnick)
        self.used_nicks.add(CHAT_USER)
        s, buf = chat_ws_login()
        try:
            h.chat("hello from hub")
            events, buf = ws_json_until(
                s, buf, lambda e: any(x.get("a") == "pub" and x.get("n") == hnick
                                      and x.get("t") == "hello from hub" for x in e))
            ws_send(s, json.dumps({"a": "pub", "t": "hello from web"}))
            h.expect(lambda m: m.kind == "chat" and m.cmd == CHAT_USER
                     and m.arg == "hello from web")
            # sender-included echo back to the web session
            events, buf = ws_json_until(
                s, buf, lambda e: any(x.get("a") == "pub" and x.get("n") == CHAT_USER for x in e))
        finally:
            s.close()

    def test_chat_pm_both_ways(self):
        hnick = uniq("chatpm")
        h = self.logged_in(hnick)
        self.used_nicks.add(CHAT_USER)
        s, buf = chat_ws_login()
        try:
            h.pm(CHAT_USER, "hi to web")
            ws_json_until(s, buf, lambda e: any(x.get("a") == "pm" and x.get("n") == hnick
                                                and x.get("t") == "hi to web" for x in e))
            ws_send(s, json.dumps({"a": "pm", "n": hnick, "t": "hi to hub"}))
            h.expect(lambda m: m.kind == "dc" and m.cmd == "To:"
                     and f"From: {CHAT_USER}" in m.arg and "hi to hub" in m.arg)
        finally:
            s.close()

    def test_chat_nick_lock(self):
        self.used_nicks.add(CHAT_USER)
        s, buf = chat_ws_login()
        try:
            c = connect(CHAT_USER)
            self.clients.append(c)
            self.expect_rejected(c, "already used")
        finally:
            s.close()


class TestTLS(E2EBase):
    """Optional TLS: hub client port listen_port_ssl (encrypted NMDC) and
    the HTTPS web endpoint listen_port_https (self-signed test cert)."""

    def test_tls_client_banner_and_hubname(self):
        c = connect(tls=True)
        self.clients.append(c)
        lock = c.expect("Lock")
        self.assertIn("Pk=DCONNECT..DAEMON", lock.arg)
        hub = c.expect("HubName")
        self.assertEqual(hub.arg, HUBNAME)

    def test_tls_client_chat_roundtrip(self):
        # a TLS hub client and a plaintext one must be able to talk
        a = self.logged_in(uniq("tlsA"), tls=True)
        b = self.logged_in(uniq("plainB"))
        a.chat("secure hello")
        m = b.expect(lambda m: m.kind == "chat" and m.cmd == a.nick)
        self.assertIn("secure hello", m.arg)

    def test_tls_port_rejects_plaintext_bytes(self):
        s = socket.create_connection(("127.0.0.1", SSLP), timeout=5)
        try:
            s.sendall(b"$Hello plaintext|")
            s.settimeout(8)
            # the TLS handshake fails on non-TLS bytes: the hub answers with
            # a TLS alert and closes - never any NMDC protocol data
            data = b""
            while b"$" not in data:
                chunk = s.recv(100)
                if not chunk:
                    break
                data += chunk
            self.assertEqual(data[:1], b"\x15")   # TLS Alert record
        finally:
            s.close()

    def test_https_healthcheck(self):
        with urllib.request.urlopen(f"https://127.0.0.1:{HTTPS}/healthcheck",
                                    timeout=5, context=_unverified_tls()) as r:
            self.assertEqual(r.status, 200)
            self.assertIn("OK online=", r.read().decode())

    def test_https_pages(self):
        with urllib.request.urlopen(f"https://127.0.0.1:{HTTPS}/",
                                    timeout=5, context=_unverified_tls()) as r:
            self.assertEqual(r.status, 200)
            body = r.read().decode()
            self.assertIn("dcd chat", body)
            self.assertIn("e2e-page-marker", body)

    def test_https_console_ws_login(self):
        self.used_nicks.add(OP_NICK)
        s, buf = ws_connect(HTTPS, tls=True)
        try:
            text, buf = ws_read_until(s, buf, "login:")
            ws_send(s, OP_NICK + "\r\n")
            text, buf = ws_read_until(s, buf, "password:")
            ws_send(s, OP_PASS + "\r\n")
            text, buf = ws_read_until(s, buf, "Logged in")
            self.assertNotIn("\xff", text)
            ws_send(s, "quit\r\n")
        finally:
            s.close()


def main():
    global PORT, CONS, HTTP, SSLP, HTTPS
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=41111)
    ap.add_argument("--cons-port", type=int, default=41511)
    ap.add_argument("--http-port", type=int, default=41911)
    ap.add_argument("--ssl-port", type=int, default=42011)
    ap.add_argument("--https-port", type=int, default=42061)
    args, rest = ap.parse_known_args()
    PORT, CONS, HTTP, SSLP, HTTPS = (args.port, args.cons_port, args.http_port,
                                     args.ssl_port, args.https_port)

    unittest.main(verbosity=2, argv=[sys.argv[0]] + rest)


if __name__ == "__main__":
    main()
