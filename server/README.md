# vTuber Combat Chess — multiplayer server (Option A)

Lobby + TCP relay for online rooms. Players never exchange IPs.

## Ports

| Port | Service |
|------|---------|
| **8080** | HTTP JSON lobby (`/rooms`, create, join) |
| **7777** | TCP relay (both clients connect outbound) |

## API

```http
GET  /health
GET  /rooms
POST /rooms          {"name":"Stream Match","password":"optional"}
POST /rooms/{id}/join {"password":"optional"}
```

Create/join responses include `token`, `role` (`host`|`guest`), `relay_port`.

## Relay handshake

Client connects to relay TCP and sends one line:

```text
VCCREL1 HELLO token=<token> role=host
```

Server replies:

```text
VCCREL1 WELCOME room=... role=host name=...
VCCREL1 PAIRED
```

Then the socket carries normal **VCC1** game lines (bidirectional forward).

## Deploy (VPS)

```bash
sudo mkdir -p /opt/vcc-multiplayer
sudo cp vcc_lobby_relay.py /opt/vcc-multiplayer/
sudo cp vcc-multiplayer.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now vcc-multiplayer
curl -s http://127.0.0.1:8080/health
```

TLS / domain (`vtuberchess.me`) can sit in front via Caddy/nginx later; clients may use raw IP:8080 / IP:7777 until DNS is ready.

## Local test

```bash
python3 vcc_lobby_relay.py
curl -s http://127.0.0.1:8080/rooms
```
