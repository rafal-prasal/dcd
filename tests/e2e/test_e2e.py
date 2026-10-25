#!/usr/bin/env python3
"""End-to-end tests for the DCD hub over the NMDC protocol.

Requires a running DCD hub configured for testing (see run_e2e.sh):
  * hubname E2E-Test-Hub, search_interval 0, minslots/minshare 0
  * registered operator account  testop / secret  (permissions "sc")
  * console access allowed from 127.0.0.1

Usage: python3 test_e2e.py --port 41111 --cons-port 41511
"""

import argparse
import itertools
import os
import sys
import time
import unittest

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

_counter = itertools.count(1)


def uniq(base):
    return f"{base}{next(_counter):03d}"


def connect(nick=None):
    c = NMDCClient(port=PORT, timeout=5.0)
    c.connect()
    if nick:
        c.handshake()
        c.validate_nick(nick)
    return c


def login(nick, password=None):
    c = NMDCClient(port=PORT, timeout=5.0)
    c.connect()
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

    def logged_in(self, nick, password=None):
        self.used_nicks.add(nick)
        c = login(nick, password)
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
            self.assertIn("1.1.1", out)
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


def main():
    global PORT, CONS
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=41111)
    ap.add_argument("--cons-port", type=int, default=41511)
    args, rest = ap.parse_known_args()
    PORT, CONS = args.port, args.cons_port

    unittest.main(verbosity=2, argv=[sys.argv[0]] + rest)


if __name__ == "__main__":
    main()
