# vTuber Combat Chess — Networked Multiplayer Design

**Status:** Active design (implementation in progress)  
**Author:** Lord Intellectual (implementation assisted by AI coding agents under direction)  
**Date:** 2026-08-06  
**Repo:** https://github.com/LordIntellectual/vtuber-combat-chess  
**Related:** [DESIGN.md](DESIGN.md) (product vision), prior audit (local only: AI OFF hotseat)  
**Protocol version:** `VCC1`  
**Default listen port:** `7777`

This document is the **single source of truth** for internet-capable multiplayer. It is written to survive session resets and context compaction: future implementers should be able to continue from this file alone.

---

## 1. Goal and success criteria

### 1.1 Goal

Enable **real networked multiplayer**: two players on different machines play one chess game over LAN or the internet, with move sync, turn ownership, and basic disconnect handling. This is **not** the existing local hotseat (same PC, AI OFF).

### 1.2 Success criteria (MVP → full)

| Level | Criteria |
|-------|----------|
| **MVP (Phase 1)** | Host listens on TCP; guest joins via `host:port`; host is authoritative; both see the same moves; AI forced off; works on LAN and over the internet when a route exists (port forward / public IP / private test relay). CLI host/join flags. |
| **Phase 2** | In-game Host/Join UI, status on HUD, side selection, reconnect within a grace window, desync recovery via state snapshot. |
| **Phase 3** | Optional **relay** path for NAT (no public port on host); matchmaking optional/out of scope for pre-alpha. |
| **Phase 4** | Hardening: fuzz protocol, automated two-process integration tests, Windows/Linux parity, docs for players. |

### 1.3 Explicit non-goals (near term)

- Ranked matchmaking, accounts, anti-cheat beyond host authority  
- Spectators, 3+ players, bughouse/variants  
- Web browser client  
- Full FIDE rule parity beyond what local `ChessGame` already enforces  
- Real-time physics / camera / theme sync (presentation is local)  
- Embedding private hosting hostnames or personal infrastructure domains in public repo artifacts

---

## 2. Current baseline (what exists today)

From codebase audit (2026-08-06):

| Capability | Status |
|------------|--------|
| Vs Stockfish | Yes — local process pipes (`StockfishConnector`) |
| Local hotseat | Yes — **A** toggles AI OFF → `BLACK_TURN` human |
| Network sockets / host / join | **No** — never implemented |
| `ConnectionException` | Misnomer — Stockfish IPC only |
| Design non-goal | `docs/DESIGN.md` listed “Full online multiplayer” as v1 non-goal |

Relevant state machine (`constants.hxx`):

```
USER_TURN → USER_MOVING → WAITING/BLACK_TURN → AI_TURN/BLACK_TURN → … → GAME_OVER
```

Local hotseat path: after white move, if `!aiEnabled` → `BLACK_TURN`. Network multiplayer **reuses** human black/white turns and **disables AI** while connected.

---

## 3. Technical requirements

### 3.1 Functional

1. **Host** can start a session and wait for one guest.  
2. **Guest** can join with `address:port`.  
3. Exactly **two players**, one side each (default: host = white, guest = black).  
4. Only the side-to-move’s controlling player may input a move.  
5. Illegal moves rejected on the **authority** (host); client never forces board state.  
6. Both clients animate captures/moves using existing FX paths.  
7. Disconnect surfaces a clear status; optional grace reconnect (Phase 2).  
8. Offline modes (AI / hotseat) remain unchanged when not in a net session.

### 3.2 Non-functional

| Concern | Target |
|---------|--------|
| Latency | Chess is turn-based; RTT of hundreds of ms is acceptable |
| Bandwidth | Negligible (text messages per move) |
| Platform | Linux + Windows 11 (same branches as product) |
| Dependencies | Prefer **no new third-party network libraries** for MVP (BSD sockets / Winsock) |
| C++ standard | C++11 (project already) |
| Security | Unauthenticated MVP; do not treat as secure; no secrets in repo |
| Privacy | No personal account IDs; no private hostnames in public docs/code |

### 3.3 Player-facing constraints

- Streamer-friendly: host can announce “join me at IP:port” on stream.  
- Portable Windows zip remains double-clickable; net mode via CLI and/or keys.  
- Firewall: host must allow inbound TCP on the listen port (document for users).

---

## 4. Architecture decision

### 4.1 Chosen model: **Host-authoritative TCP (in-process)**

```
┌─────────────────────┐         TCP          ┌─────────────────────┐
│  Host process       │◄────────────────────►│  Guest process      │
│  • Listen socket    │   VCC1 line protocol │  • Connect socket   │
│  • ChessGame (auth) │                      │  • ChessGame (view) │
│  • NetSession HOST  │                      │  • NetSession CLIENT│
│  • White (default)  │                      │  • Black (default)  │
└─────────────────────┘                      └─────────────────────┘
```

**Why this model**

| Option | Pros | Cons | Decision |
|--------|------|------|----------|
| **Host-authoritative TCP** | Simple; one rules engine; easy desync repair; no extra server binary | Host must be reachable; NAT pain | **MVP choice** |
| Dedicated game server | Fair authority; easier NAT if server public | Ops cost; extra binary/service | Later if needed |
| Lockstep both peers | Symmetric | Desync hell; no single truth | Rejected |
| WebRTC / WebSocket | Browser-friendly; some NAT help | Heavy deps; overkill for desktop MVP | Deferred |
| Pure UDP | Lower latency | Need reliability layer for chess | Not needed |

**In-process host** (not a separate `vcc-server` binary for MVP): the host player’s game instance listens. Keeps deployment identical to single-player.

### 4.2 Optional Phase 3: TCP relay

For internet play when the host has no public port:

```
Host ──► Relay ◄── Guest
         (forwards byte stream; may not interpret VCC1)
```

- Relay is a **dumb TCP bridge** or thin room broker.  
- Host and guest still run the same VCC1 protocol; relay does not become chess authority unless explicitly redesigned.  
- **Configuration:** address/port via environment variable or local config file only (e.g. `VCC_RELAY=host:port`).  
- **STRICT:** never commit private hosting domains, personal infra hostnames, or internal LAN docs that identify private services into the public GitHub tree. Internal test hosts stay on the workstation / private notes only.

### 4.3 Module layout (code)

```
upstream/src/Network/
  TcpSocket.hxx / .cxx      # cross-platform TCP (listen, accept, connect, send, recv non-blocking poll)
  NetProtocol.hxx / .cxx    # encode/decode VCC1 messages
  NetSession.hxx / .cxx     # host/client state machine, queues inbound/outbound
```

Integration points:

| File | Role |
|------|------|
| `ChessGame` | Apply remote UCI moves; query side-to-move; optional “local side only” input gate |
| `NightfireChessArena.cxx` | CLI `--host` / `--join`; poll `NetSession` each frame; gate clicks; HUD status |
| `Hud` | Show net status line (Phase 2 polish) |
| `CMakeLists.txt` | Add Network sources; Windows link `ws2_32` |
| `docs/DESIGN.md` | Remove multiplayer from non-goals; link this doc |

---

## 5. Networking model details

### 5.1 Transport

- **Protocol:** TCP, IPv4 (IPv6 optional later).  
- **Framing:** newline-delimited text lines (`\n`). Max line length 4 KiB.  
- **Encoding:** UTF-8 ASCII subset for tokens.  
- **Non-blocking:** sockets set non-blocking; game loop `poll()` / `select()` with 0 timeout each frame so OpenGL never stalls.  
- **Keepalive:** application `PING` / `PONG` every 10s; TCP keepalive optional.

### 5.2 Port and bind

| Item | Value |
|------|--------|
| Default port | `7777` |
| Bind address | `0.0.0.0` (all interfaces) for host |
| Override | `--host [port]`, `--join host:port`, env `VCC_PORT` |

### 5.3 Connection lifecycle

```
HOST:  Idle → Listening → Handshaking → InGame → Closing
CLIENT: Idle → Connecting → Handshaking → InGame → Closing
```

1. Host binds and listens.  
2. Guest connects.  
3. Guest sends `HELLO`.  
4. Host replies `WELCOME` (assigns sides, session id, protocol check).  
5. Either side may send `READY`; when both ready (or auto-ready on WELCOME), host sends `START`.  
6. Moves flow; host may send `STATE` snapshots.  
7. `GOODBYE` or socket error → session ends; return to offline mode or wait for reconnect (Phase 2).

---

## 6. Protocol specification (VCC1)

### 6.1 Line format

```
VCC1 <TYPE> [key=value]...
```

- First token: magic `VCC1`.  
- Second token: message type (uppercase).  
- Remaining: space-separated `key=value` pairs. Values must not contain spaces; use `_` or percent-encoding if needed.  
- Unknown keys: ignore (forward compatible).  
- Unknown types: ignore or reply `ERROR code=unknown_type` (host may disconnect abusive peers later).

### 6.2 Message catalog

| Type | Direction | Required keys | Purpose |
|------|-----------|---------------|---------|
| `HELLO` | C→H | `proto=1`, `name=<display>` | Guest identifies |
| `WELCOME` | H→C | `proto=1`, `session=<id>`, `you=black\|white`, `host=white\|black` | Accept + assign sides |
| `READY` | either | (optional `name=`) | Player ready |
| `START` | H→C | `fen=<fen>`, `turn=w\|b`, `ply=0` | Begin game (opening or resume) |
| `MOVE_REQ` | C→H | `uci=<uci>`, `ply=<n>` | Client proposes a move |
| `MOVE` | H→C (and host applies locally first) | `uci=<uci>`, `ply=<n>`, `by=w\|b` | Authoritative move applied |
| `REJECT` | H→C | `uci=…`, `reason=…` | Illegal / out of turn |
| `STATE` | H→C | `fen=…`, `turn=w\|b`, `ply=<n>`, `end=none\|checkmate\|forfeit`, `winner=w\|b\|-` | Full sync / recovery |
| `PING` | either | `t=<ms>` | Keepalive |
| `PONG` | either | `t=<ms>` | Keepalive reply |
| `CHAT` | either | `text=…` | Optional; Phase 2 |
| `GOODBYE` | either | `reason=…` | Clean close |
| `ERROR` | either | `code=…`, `msg=…` | Protocol / fatal error |

### 6.3 UCI move strings

Same as local engine: `e2e4`, promotion e.g. `e7e8q`. No multi-move batches in MVP.

### 6.4 FEN

Host’s `boardToFen(whiteToMove)` is source of truth. Known limitation: local GUI still omits full castling/EP rights in FEN (existing design). Network inherits that; do not invent rights the rules engine does not track.

### 6.5 Example session

```
# Guest connects
C→H  VCC1 HELLO proto=1 name=Guest
H→C  VCC1 WELCOME proto=1 session=a1b2 you=black host=white
H→C  VCC1 START fen=rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1 turn=w ply=0
# Host plays e2e4
H→C  VCC1 MOVE uci=e2e4 ply=1 by=w
# Guest plays e7e5
C→H  VCC1 MOVE_REQ uci=e7e5 ply=2
H→C  VCC1 MOVE uci=e7e5 ply=2 by=b
# Periodic
H→C  VCC1 STATE fen=… turn=w ply=2 end=none winner=-
```

### 6.6 Versioning

- Magic prefix `VCC1` is major protocol family.  
- `proto=1` field allows minor negotiation.  
- Breaking changes → `VCC2` and dual-read period if needed.

---

## 7. Lobby / host / join flow

### 7.1 Phase 1 (CLI — ship first)

```bash
# Host (default port 7777)
./VTuberCombatChess --host
./VTuberCombatChess --host 9000

# Guest
./VTuberCombatChess --join 192.168.1.10:7777
./VTuberCombatChess --join example.invalid:7777
```

Behavior:

1. Parse argv before GLFW init where possible (fail fast on bad args).  
2. Host: after game systems start, begin listen; HUD/event: `Hosting on :7777`.  
3. Guest: connect with timeout (~5s); on failure show error and continue offline or exit (prefer offline + message).  
4. Force `aiEnabled = false` for duration of session.  
5. Assign local controllable side from WELCOME.

Env overrides (optional):

| Env | Meaning |
|-----|---------|
| `VCC_PORT` | Default port if not on CLI |
| `VCC_JOIN` | Implicit join target if no `--join` |
| `VCC_RELAY` | Phase 3 relay endpoint (private config only) |

### 7.2 Phase 2 (in-game UI)

- Keys (proposal): **N** multiplayer panel; **O** host; join field for IP.  
- Settings page “Network” with port + last join address (local file under user config, not repo).  
- HUD: `NET HOST | waiting`, `NET | connected as Black`, RTT from PING.

### 7.3 Side assignment

| Mode | Host side | Guest side |
|------|-----------|------------|
| Default | White | Black |
| Future | Negotiated in HELLO/WELCOME (`want=black`) | — |

Streamer convenience: host is usually white so the streamer opens; can flip later without protocol break (`you=` / `host=` already in WELCOME).

---

## 8. Data sync and authority

### 8.1 Authority rules

1. **Host `ChessGame` is authoritative** for legality, turn, and end conditions.  
2. Guest `ChessGame` is a **presentation replica**: applies only `MOVE` / `START` / `STATE` from host.  
3. Guest **never** runs Stockfish for the opponent during a net game.  
4. Host **never** accepts guest input that is not a well-formed `MOVE_REQ` for guest’s side on guest’s turn.

### 8.2 Host local input

- When host’s side to move: existing click → `perform()` path.  
- When a local move **commits** (transition to `*_MOVING` with UCI in `lastUserMove`): enqueue `MOVE` to guest.  
- Prefer sending `MOVE` when the move is **accepted** (start of animation), so both animate together; end-of-animation also acceptable if both use same duration (~1s existing animation).

### 8.3 Guest local input

- On click that would move: build UCI; send `MOVE_REQ`; **do not** apply locally until `MOVE` returns.  
- Optional: optimistic highlight only (Phase 2).  
- On `REJECT`: flash illegal SFX; clear selection.

### 8.4 Applying a remote `MOVE`

Add `ChessGame::tryApplyUciMove(const std::string& uci, bool asWhite)` (or equivalent):

- Parse UCI → start/end/(promo).  
- Verify piece side and pseudo-legal/legal via existing matrices or shared validation.  
- Set `moving*` fields and enter `USER_MOVING` or `BLACK_MOVING` (not AI path) so FX/events fire identically.  
- On host, validation failure → `REJECT` and no state change.

Refactor opportunity: extract the “begin move animation from squares” block shared by user click path and AI path into one helper used by network too.

### 8.5 Desync recovery

- Host sends `STATE` every N moves (e.g. each move) and on guest request (`STATE_REQ` future).  
- Guest if local FEN ≠ host FEN: hard-apply board from FEN (helper `loadFromFen` — may be Phase 2 if costly); MVP can reset board from STATE piece placement.  
- `ply` counters must match on `MOVE_REQ`; stale ply → reject.

### 8.6 Presentation not synced

Themes, piece sets, camera, audio, FX intensity remain **local**. Chess truth only.

### 8.7 Reset / resign

| Action | MVP |
|--------|-----|
| Host **R** reset | Host resets; send `START` with opening FEN |
| Guest **R** | Ignore or send request (host-only reset in MVP) |
| Resign | Phase 2: `RESIGN by=w\|b` → GAME_OVER |

---

## 9. ChessGame / input gating

### 9.1 New concepts

```cpp
enum NetRole { NET_NONE, NET_HOST, NET_CLIENT };
// localSide: +1 white (USER), -1 black (AI constant side)
```

- `canLocalPlayerMove()` — true if `NET_NONE` (legacy) or (net active and side-to-move == local side).  
- `setNewSelectedPiecePosition` early-out if `!canLocalPlayerMove()` except for pure selection UX of own pieces.  
- Actually: only allow selecting/moving own pieces when it is local side’s turn.

### 9.2 AI interaction

On entering net session: `setAiEnabled(false)`.  
On leaving: restore previous preference (store `aiEnabledBeforeNet`).

### 9.3 End of game

Host detects checkmate/forfeit as today → send `STATE end=checkmate winner=…` → both show victory UI.

---

## 10. Threading model

**Single-threaded game loop** (MVP):

```
each frame:
  net.pump()           // non-blocking read/write, accept
  process inbound msgs // may call tryApplyUciMove
  handle input
  game.perform()
  if host && move just started: net.send MOVE
  render
```

No network thread in MVP (avoids locking `ChessGame`). If connect() blocks, use non-blocking connect + timeout state in `NetSession`.

---

## 11. Security and abuse (pragmatic pre-alpha)

| Risk | Mitigation |
|------|------------|
| Anyone who can reach port can join | Single-slot accept; optional join code later |
| Protocol spam | Max line length; rate-limit messages; disconnect |
| Malicious UCI | Host validates; never trust client board |
| No TLS | Accept for LAN/trusted; document cleartext |
| Secrets in repo | Forbidden — no private hostnames, tokens, or personal infra in git |

This is **not** a secure competitive environment.

---

## 12. Testing approach

### 12.1 Unit

- Encode/decode each message type (round-trip).  
- Reject truncated / oversized / wrong magic lines.  
- UCI parse edge cases.

### 12.2 Integration (manual + scripted)

1. Two processes on localhost: `--host` and `--join 127.0.0.1:7777`.  
2. Host plays e2e4; guest sees animation and black to move.  
3. Guest plays; host sees.  
4. Illegal guest move → REJECT.  
5. Kill guest → host shows disconnected.  
6. Kill host → guest shows disconnected.  
7. Full game to checkmate if feasible.

### 12.3 Automated (Phase 4)

- Headless or minimal `NetSession` loop without OpenGL for CI.  
- Optional: `tests/network/test_protocol.cxx` linked without full game.

### 12.4 Cross-platform

- Linux host ↔ Linux guest (workstation).  
- Windows guest ↔ Linux host when available.  
- Document Windows Firewall prompt on first host.

### 12.5 Internet path

- LAN first.  
- Then port-forward test.  
- Private relay only via local env; never document private domains in public README.

---

## 13. Risks and mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| NAT / no inbound port | Cannot host on internet | Phase 3 relay; guide for port forward |
| Desync from animation timing | Visual mismatch | Authority STATE; apply MOVE at same lifecycle point |
| Incomplete chess rules | Bad rejects / accepts | Same as single-player; fix rules once for all modes |
| Main-thread connect stall | Hitches | Non-blocking connect |
| Windows socket init | Host fails silently | `WSAStartup` once; clear logs `[VCC-NET]` |
| Scope creep (matchmaking) | Delays MVP | Phased plan; this doc gates work |
| Context loss across agents | Rework | This document + early commits |

---

## 14. Phased implementation plan

### Phase 0 — Design anchor (this document)

- [x] Write `docs/MULTIPLAYER_DESIGN.md`  
- [ ] Commit early to repo  
- [ ] Link from `docs/DESIGN.md`; remove multiplayer from non-goals  

### Phase 1 — MVP transport + host/join (current build target)

1. `Network/TcpSocket` — cross-platform non-blocking TCP.  
2. `Network/NetProtocol` — VCC1 encode/decode.  
3. `Network/NetSession` — host listen, client connect, queues, PING.  
4. `ChessGame::tryApplyUciMove` (+ any shared begin-move helper).  
5. CLI `--host [port]` / `--join host:port` in `NightfireChessArena.cxx`.  
6. Frame pump + input gating + AI off.  
7. CMake: sources + `ws2_32` on Windows.  
8. Manual localhost two-process test.  
9. Log tags: `[VCC-NET]` for stream/debug stdout.

**Exit criteria:** Two machines (or two local processes) complete legal moves both directions over TCP.

### Phase 2 — UX and robustness

- HUD net status; multiplayer help lines.  
- Join address persistence (local only).  
- Reconnect grace (same session id, 30–60s).  
- `STATE` hard resync / `loadFromFen`.  
- Host-only reset broadcast; resign.  
- Reject reasons surfaced on HUD.

### Phase 3 — NAT relay

- Dumb relay tool or documented external bridge.  
- Client path: connect to relay with room token; host registers room.  
- Config via env/local file only; **no private domains in public tree**.

### Phase 4 — Harden and ship

- Protocol tests in CI.  
- README short “Multiplayer (experimental)” section.  
- Windows firewall note.  
- Consider separate `vcc-relay` binary if relay stays in-tree (generic, no infra IDs).

---

## 15. Implementation notes for agents

1. **Always re-read this file** after compaction before coding.  
2. Prefer small commits: (1) design, (2) socket+protocol, (3) session, (4) game wire-up.  
3. Do not break offline AI/hotseat.  
4. Do not add heavy deps (Boost.Asio, etc.) without updating this design.  
5. Do not put private hostnames, tokens, or personal infrastructure in any file that may be pushed to GitHub.  
6. Identity isolation: no personal account identifiers in public surfaces.  
7. Match existing style: C++11, `.hxx`/`.cxx`, `[VCC]` log prefix family.  
8. Branch: implement on current product branch or `feature/multiplayer`; keep Windows + Linux buildable.

### 15.1 Suggested commit series

```
docs: add networked multiplayer design (VCC1 host-authoritative TCP)
net: add TcpSocket + VCC1 protocol codec
net: add NetSession host/client state machine
game: apply UCI moves for network authority path
app: wire --host/--join and per-frame net pump
```

### 15.2 Log / stream events

```
[VCC-NET] listening on 0.0.0.0:7777
[VCC-NET] client connected from …
[VCC-NET] session start host=white you=black
[VCC-NET] MOVE e2e4 ply=1
[VCC-NET] disconnect reason=…
```

---

## 16. Open questions (resolved defaults)

| Question | Default until product owner overrides |
|----------|----------------------------------------|
| Host color | White |
| Max peers | 1 guest |
| Guest mid-game join | Not allowed (fresh START only) |
| Chat | Deferred |
| Encryption | None in MVP |
| IPv6 | Deferred |
| Spectators | Deferred |

---

## 17. Changelog (design doc)

| Date | Change |
|------|--------|
| 2026-08-06 | Initial full design: host-authoritative TCP VCC1, phases 0–4, protocol, testing, risks |

---

## 18. Appendix A — Mapping to existing types

| Concept | Existing code |
|---------|----------------|
| White pieces | `piece > 0` (`USER = 1`) |
| Black pieces | `piece < 0` (`AI = -1` constant name is historical) |
| White turn | `USER_TURN` / `USER_MOVING` |
| Black human turn | `BLACK_TURN` / `BLACK_MOVING` |
| UCI last move | `ChessGame::getLastUserMove()` |
| FEN | `boardToFen(bool whiteToMove)` (private today — may friend/expose) |
| Illegal move UX | `GameException` + SFX `sfx_illegal` |

## 19. Appendix B — CLI reference (MVP)

```
VTuberCombatChess [--host [PORT]] [--join HOST:PORT] [--port PORT]
```

| Flag | Meaning |
|------|---------|
| `--host [PORT]` | Listen for one guest (default port 7777) |
| `--join HOST:PORT` | Connect as guest |
| `--port PORT` | Default port for `--host` if port omitted |

Mutually exclusive: `--host` and `--join`.

---

**End of design document.** Implementation should tick Phase 0 checkboxes, then execute Phase 1 until MVP exit criteria are met.
