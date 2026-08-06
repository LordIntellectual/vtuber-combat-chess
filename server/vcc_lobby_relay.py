#!/usr/bin/env python3
"""
vTuber Combat Chess — lobby + TCP relay (Option A).

- HTTP JSON API on LOBBY_PORT (default 8080): list/create/join rooms
- TCP relay on RELAY_PORT (default 7777): host + guest both connect outbound;
  server pairs them and forwards VCC1 lines bidirectionally (no peer IPs exposed).

No third-party deps (stdlib only). Designed for a disposable VPS.
"""

from __future__ import annotations

import asyncio
import hashlib
import json
import logging
import os
import secrets
import time
from dataclasses import dataclass, field
from typing import Dict, Optional, Set, Tuple
from urllib.parse import parse_qs, urlparse

LOG = logging.getLogger("vcc")

LOBBY_HOST = os.environ.get("VCC_LOBBY_HOST", "0.0.0.0")
LOBBY_PORT = int(os.environ.get("VCC_LOBBY_PORT", "8080"))
RELAY_HOST = os.environ.get("VCC_RELAY_HOST", "0.0.0.0")
RELAY_PORT = int(os.environ.get("VCC_RELAY_PORT", "7777"))
ROOM_TTL_SEC = int(os.environ.get("VCC_ROOM_TTL", "3600"))
MAX_ROOMS = int(os.environ.get("VCC_MAX_ROOMS", "200"))


def _hash_password(password: str, salt: str) -> str:
    if not password:
        return ""
    raw = hashlib.sha256((salt + "\0" + password).encode("utf-8")).hexdigest()
    return raw


@dataclass
class Room:
    room_id: str
    name: str
    salt: str
    password_hash: str  # empty => open room
    created_at: float
    host_token: str
    guest_token: str
    host_present: bool = False
    guest_present: bool = False
    # relay sides: writer queues
    host_q: Optional[asyncio.Queue] = None
    guest_q: Optional[asyncio.Queue] = None
    host_writer: Optional[asyncio.StreamWriter] = None
    guest_writer: Optional[asyncio.StreamWriter] = None

    def has_password(self) -> bool:
        return bool(self.password_hash)

    def players(self) -> int:
        return int(self.host_present) + int(self.guest_present)

    def public_dict(self) -> dict:
        return {
            "id": self.room_id,
            "name": self.name,
            "players": self.players(),
            "max_players": 2,
            "has_password": self.has_password(),
            "full": self.players() >= 2,
            "created_at": int(self.created_at),
        }


class Lobby:
    def __init__(self) -> None:
        self.rooms: Dict[str, Room] = {}
        self.lock = asyncio.Lock()

    async def prune(self) -> None:
        now = time.time()
        async with self.lock:
            dead = [
                rid
                for rid, r in self.rooms.items()
                if (now - r.created_at > ROOM_TTL_SEC and r.players() == 0)
                or (now - r.created_at > ROOM_TTL_SEC * 3)
            ]
            for rid in dead:
                LOG.info("prune room %s", rid)
                del self.rooms[rid]

    async def list_rooms(self) -> list:
        await self.prune()
        async with self.lock:
            rooms = sorted(self.rooms.values(), key=lambda r: r.created_at, reverse=True)
            return [r.public_dict() for r in rooms if r.players() < 2 or True]

    async def create_room(self, name: str, password: str = "") -> dict:
        await self.prune()
        name = (name or "Room").strip()[:48] or "Room"
        # sanitize name for display / protocol
        name = "".join(c if c.isalnum() or c in " _-.'" else "_" for c in name)[:48]
        async with self.lock:
            if len(self.rooms) >= MAX_ROOMS:
                raise ValueError("server_full")
            room_id = secrets.token_hex(4)
            salt = secrets.token_hex(8)
            room = Room(
                room_id=room_id,
                name=name,
                salt=salt,
                password_hash=_hash_password(password, salt),
                created_at=time.time(),
                host_token=secrets.token_hex(16),
                guest_token=secrets.token_hex(16),
            )
            self.rooms[room_id] = room
            LOG.info("create room id=%s name=%r pw=%s", room_id, name, bool(password))
            return {
                "ok": True,
                "room": room.public_dict(),
                "role": "host",
                "token": room.host_token,
                "relay_port": RELAY_PORT,
            }

    async def join_room(self, room_id: str, password: str = "") -> dict:
        await self.prune()
        async with self.lock:
            room = self.rooms.get(room_id)
            if not room:
                raise ValueError("not_found")
            if room.password_hash:
                if _hash_password(password, room.salt) != room.password_hash:
                    raise ValueError("bad_password")
            if room.guest_present and room.host_present:
                raise ValueError("full")
            # Prefer guest seat; if host slot empty (reconnect), still issue guest token for join API
            role = "guest"
            token = room.guest_token
            return {
                "ok": True,
                "room": room.public_dict(),
                "role": role,
                "token": token,
                "relay_port": RELAY_PORT,
            }

    def find_by_token(self, token: str) -> Optional[Tuple[Room, str]]:
        for r in self.rooms.values():
            if token == r.host_token:
                return r, "host"
            if token == r.guest_token:
                return r, "guest"
        return None


LOBBY = Lobby()


# ---------------- HTTP lobby -----------------

def _http_response(status: int, body: dict, extra_headers: Optional[list] = None) -> bytes:
    data = json.dumps(body).encode("utf-8")
    reason = {
        200: "OK",
        400: "Bad Request",
        404: "Not Found",
        405: "Method Not Allowed",
        500: "Internal Server Error",
    }.get(status, "OK")
    headers = [
        f"HTTP/1.1 {status} {reason}",
        "Content-Type: application/json",
        f"Content-Length: {len(data)}",
        "Access-Control-Allow-Origin: *",
        "Access-Control-Allow-Methods: GET, POST, OPTIONS",
        "Access-Control-Allow-Headers: Content-Type",
        "Connection: close",
    ]
    if extra_headers:
        headers.extend(extra_headers)
    return ("\r\n".join(headers) + "\r\n\r\n").encode("utf-8") + data


async def handle_http(
    reader: asyncio.StreamReader, writer: asyncio.StreamWriter
) -> None:
    peer = writer.get_extra_info("peername")
    try:
        raw = await asyncio.wait_for(reader.readuntil(b"\r\n\r\n"), timeout=10)
    except Exception:
        writer.close()
        await writer.wait_closed()
        return

    try:
        head = raw.decode("utf-8", errors="replace")
        lines = head.split("\r\n")
        req = lines[0].split()
        if len(req) < 2:
            writer.write(_http_response(400, {"ok": False, "error": "bad_request"}))
            await writer.drain()
            return
        method, path = req[0], req[1]
        headers = {}
        for line in lines[1:]:
            if ":" in line:
                k, v = line.split(":", 1)
                headers[k.strip().lower()] = v.strip()

        body = b""
        cl = int(headers.get("content-length", "0") or "0")
        if cl > 0:
            body = await asyncio.wait_for(reader.readexactly(cl), timeout=10)

        if method == "OPTIONS":
            writer.write(_http_response(200, {"ok": True}))
            await writer.drain()
            return

        parsed = urlparse(path)
        route = parsed.path.rstrip("/") or "/"

        if method == "GET" and route in ("/", "/health"):
            writer.write(
                _http_response(
                    200,
                    {
                        "ok": True,
                        "service": "vcc-lobby-relay",
                        "version": 1,
                        "relay_port": RELAY_PORT,
                    },
                )
            )
        elif method == "GET" and route == "/rooms":
            rooms = await LOBBY.list_rooms()
            writer.write(_http_response(200, {"ok": True, "rooms": rooms}))
        elif method == "POST" and route == "/rooms":
            try:
                payload = json.loads(body.decode("utf-8") or "{}")
            except json.JSONDecodeError:
                writer.write(_http_response(400, {"ok": False, "error": "bad_json"}))
                await writer.drain()
                return
            try:
                result = await LOBBY.create_room(
                    payload.get("name", "Room"),
                    payload.get("password", "") or "",
                )
                writer.write(_http_response(200, result))
            except ValueError as e:
                writer.write(_http_response(400, {"ok": False, "error": str(e)}))
        elif method == "POST" and route.startswith("/rooms/") and route.endswith("/join"):
            room_id = route[len("/rooms/") : -len("/join")]
            try:
                payload = json.loads(body.decode("utf-8") or "{}")
            except json.JSONDecodeError:
                writer.write(_http_response(400, {"ok": False, "error": "bad_json"}))
                await writer.drain()
                return
            try:
                result = await LOBBY.join_room(room_id, payload.get("password", "") or "")
                writer.write(_http_response(200, result))
            except ValueError as e:
                code = 404 if str(e) == "not_found" else 400
                writer.write(_http_response(code, {"ok": False, "error": str(e)}))
        else:
            writer.write(_http_response(404, {"ok": False, "error": "not_found"}))

        await writer.drain()
    except Exception:
        LOG.exception("http error peer=%s", peer)
        try:
            writer.write(_http_response(500, {"ok": False, "error": "internal"}))
            await writer.drain()
        except Exception:
            pass
    finally:
        try:
            writer.close()
            await writer.wait_closed()
        except Exception:
            pass


# ---------------- TCP relay -----------------

async def _pipe(
    reader: asyncio.StreamReader,
    other_writer: asyncio.StreamWriter,
    label: str,
) -> None:
    try:
        while True:
            data = await reader.read(4096)
            if not data:
                break
            other_writer.write(data)
            await other_writer.drain()
    except Exception as e:
        LOG.info("pipe %s end: %s", label, e)


async def handle_relay(
    reader: asyncio.StreamReader, writer: asyncio.StreamWriter
) -> None:
    peer = writer.get_extra_info("peername")
    LOG.info("relay connect from %s", peer)
    # First line: VCCREL1 HELLO token=... role=host|guest
    try:
        line = await asyncio.wait_for(reader.readline(), timeout=15)
    except Exception:
        writer.close()
        await writer.wait_closed()
        return

    text = line.decode("utf-8", errors="replace").strip()
    parts = text.split()
    token = ""
    role_claim = ""
    if len(parts) >= 2 and parts[0] == "VCCREL1" and parts[1] == "HELLO":
        for p in parts[2:]:
            if p.startswith("token="):
                token = p[6:]
            elif p.startswith("role="):
                role_claim = p[5:]
    if not token:
        writer.write(b"VCCREL1 ERROR code=bad_hello\n")
        await writer.drain()
        writer.close()
        await writer.wait_closed()
        return

    found = LOBBY.find_by_token(token)
    if not found:
        writer.write(b"VCCREL1 ERROR code=bad_token\n")
        await writer.drain()
        writer.close()
        await writer.wait_closed()
        return

    room, role = found
    if role_claim and role_claim != role:
        writer.write(b"VCCREL1 ERROR code=role_mismatch\n")
        await writer.drain()
        writer.close()
        await writer.wait_closed()
        return

    async with LOBBY.lock:
        if role == "host":
            if room.host_writer and not room.host_writer.is_closing():
                try:
                    room.host_writer.close()
                except Exception:
                    pass
            room.host_writer = writer
            room.host_present = True
        else:
            if room.guest_writer and not room.guest_writer.is_closing():
                try:
                    room.guest_writer.close()
                except Exception:
                    pass
            room.guest_writer = writer
            room.guest_present = True

    writer.write(
        f"VCCREL1 WELCOME room={room.room_id} role={role} name={room.name.replace(' ', '_')}\n".encode()
    )
    await writer.drain()
    LOG.info("relay bound room=%s role=%s peer=%s", room.room_id, role, peer)

    # Wait until peer is present, then bidirectional pipe
    try:
        for _ in range(600):  # ~60s
            other = room.guest_writer if role == "host" else room.host_writer
            if other is not None and not other.is_closing():
                break
            await asyncio.sleep(0.1)
        else:
            writer.write(b"VCCREL1 ERROR code=peer_timeout\n")
            await writer.drain()
            return

        other = room.guest_writer if role == "host" else room.host_writer
        # Notify both sides that pairing is ready
        try:
            writer.write(b"VCCREL1 PAIRED\n")
            await writer.drain()
            other.write(b"VCCREL1 PAIRED\n")
            await other.drain()
        except Exception:
            pass

        # After PAIRED, remaining bytes are raw VCC1 (and any leftover)
        # We need to also read from `other` — spawn two pipes using separate
        # readers. StreamReader is bound 1:1; for the reverse direction the
        # peer's handler runs the other pipe. So each connection only forwards
        # local reads → other writer.
        await _pipe(reader, other, f"{room.room_id}:{role}->peer")
    finally:
        async with LOBBY.lock:
            if role == "host":
                room.host_present = False
                room.host_writer = None
            else:
                room.guest_present = False
                room.guest_writer = None
            # If both gone, drop room
            if not room.host_present and not room.guest_present:
                LOBBY.rooms.pop(room.room_id, None)
                LOG.info("room closed %s", room.room_id)
            else:
                # Notify remaining peer
                rem = room.host_writer or room.guest_writer
                if rem and not rem.is_closing():
                    try:
                        rem.write(b"VCCREL1 PEER_LEFT\n")
                        await rem.drain()
                    except Exception:
                        pass
        try:
            writer.close()
            await writer.wait_closed()
        except Exception:
            pass
        LOG.info("relay disconnect room=%s role=%s", room.room_id, role)


async def main() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
    )
    lobby_srv = await asyncio.start_server(handle_http, LOBBY_HOST, LOBBY_PORT)
    relay_srv = await asyncio.start_server(handle_relay, RELAY_HOST, RELAY_PORT)
    LOG.info("lobby HTTP on %s:%s", LOBBY_HOST, LOBBY_PORT)
    LOG.info("relay TCP on %s:%s", RELAY_HOST, RELAY_PORT)
    async with lobby_srv, relay_srv:
        await asyncio.gather(lobby_srv.serve_forever(), relay_srv.serve_forever())


if __name__ == "__main__":
    asyncio.run(main())
