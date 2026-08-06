#!/usr/bin/env python3
"""Headless VCC1 protocol smoke test (no OpenGL).

Usage:
  # Terminal A (game host with display):
  #   ./run.sh --host 7777
  # Terminal B:
  #   python3 tools/test_vcc1_protocol.py 127.0.0.1 7777

Or self-test encode/decode only:
  python3 tools/test_vcc1_protocol.py --self
"""
from __future__ import annotations

import socket
import sys
import time


def encode(msg_type: str, **kv: str) -> bytes:
    parts = ["VCC1", msg_type] + [f"{k}={v}" for k, v in kv.items()]
    return (" ".join(parts) + "\n").encode("utf-8")


def decode_line(line: str) -> tuple[str, dict[str, str]]:
    line = line.strip()
    toks = line.split()
    if len(toks) < 2 or toks[0] != "VCC1":
        raise ValueError(f"bad line: {line!r}")
    kv = {}
    for t in toks[2:]:
        if "=" in t:
            k, v = t.split("=", 1)
            kv[k] = v
    return toks[1], kv


def self_test() -> None:
    line = encode("HELLO", proto="1", name="Test").decode()
    t, kv = decode_line(line)
    assert t == "HELLO" and kv["proto"] == "1" and kv["name"] == "Test"
    print("self-test OK:", line.strip())


def join_host(host: str, port: int) -> None:
    s = socket.create_connection((host, port), timeout=5)
    s.settimeout(5.0)
    buf = b""
    s.sendall(encode("HELLO", proto="1", name="PyGuest"))
    deadline = time.time() + 5
    got_welcome = False
    got_start = False
    while time.time() < deadline:
        try:
            chunk = s.recv(4096)
        except socket.timeout:
            continue
        if not chunk:
            break
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode("utf-8", errors="replace")
            print("<<", text)
            t, kv = decode_line(text)
            if t == "WELCOME":
                got_welcome = True
                assert kv.get("you") == "black"
            elif t == "START":
                got_start = True
            elif t == "PING":
                s.sendall(encode("PONG", t=kv.get("t", "0")))
    s.sendall(encode("GOODBYE", reason="test_done"))
    s.close()
    if not got_welcome:
        print("FAIL: no WELCOME", file=sys.stderr)
        sys.exit(1)
    print("PASS: handshake (WELCOME%s)" % (" + START" if got_start else " only"))


def main() -> None:
    if len(sys.argv) == 2 and sys.argv[1] == "--self":
        self_test()
        return
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 7777
    join_host(host, port)


if __name__ == "__main__":
    main()
