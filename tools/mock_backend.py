"""Local mock of the chess bot backend (application protocol v2).

Lets you test engine_ws without touching the public server. It checks the
Bearer key, the register packet, the received -> pong -> move order and
request IDs, and verifies every returned FEN is a legal successor.

Setup (once):   pip install websockets python-chess
Run:            python tools/mock_backend.py
Then:           engine_ws            (defaults to ws://127.0.0.1:3000/ws/bot)

By default the mock accepts any "Authorization: Bearer <something>" header
(so your real auth_key.txt works here too). Set MOCK_KEY to require an exact key.
"""

import asyncio
import os
import struct

import chess
import websockets

HOST, PORT = "127.0.0.1", 3000
KEY = os.environ.get("MOCK_KEY")  # None = accept any non-empty Bearer key

# (mode, FEN) requests sent to the bot in order: castling, en passant,
# promotion and counter edge cases.
POSITIONS = [
    ("fast", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"),
    ("deep", "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 3 10"),
    ("fast", "4k3/8/8/3Pp3/8/8/8/4K3 w - e6 0 2"),
    ("deep", "4k3/1P6/8/8/8/8/8/4K3 w - - 5 40"),
]


def legal_successors(fen):
    board = chess.Board(fen)
    out = set()
    for move in board.legal_moves:
        nxt = board.copy()
        nxt.push(move)
        out.add(nxt.fen(en_passant="legal"))
        out.add(nxt.fen(en_passant="fen"))
    return out


def predict_packet(request_id, mode, fen):
    m, f = mode.encode(), fen.encode()
    return bytes([0x10]) + struct.pack("<Q", request_id) + bytes([len(m)]) + m + struct.pack("<H", len(f)) + f


async def expect(ws, opcode, request_id, size=None):
    data = await asyncio.wait_for(ws.recv(), 30)
    assert isinstance(data, bytes), f"expected binary, got text: {data!r}"
    assert data[0] == opcode, f"expected opcode {opcode:#04x}, got {data[0]:#04x}"
    got = struct.unpack("<Q", data[1:9])[0]
    assert got == request_id, f"wrong request id {got}, expected {request_id}"
    if size is not None:
        assert len(data) == size, f"packet size {len(data)}, expected {size}"
    return data


async def handler(ws):
    print(f"[mock] bot connected from {ws.remote_address}")
    reg = await asyncio.wait_for(ws.recv(), 5)
    assert isinstance(reg, bytes) and reg[:2] == b"\x01\x02", f"bad register: {reg!r}"
    pos = 2
    ident = reg[pos + 1 : pos + 1 + reg[pos]].decode(); pos += 1 + reg[pos]
    name = reg[pos + 1 : pos + 1 + reg[pos]].decode(); pos += 1 + reg[pos]
    modes = []
    for _ in range(reg[pos]):
        pos += 1
        modes.append(reg[pos + 1 : pos + 1 + reg[pos]].decode())
        pos += reg[pos]
    print(f"[mock] registered identity={ident!r} name={name!r} modes={modes}")
    await ws.send(bytes([0x81, 0x02]))

    for i, (mode, fen) in enumerate(POSITIONS):
        if mode not in modes:
            mode = modes[0]
        request_id = (1 << 40) + i  # exercise the full 64-bit range
        await ws.send(predict_packet(request_id, mode, fen))
        await expect(ws, 0x11, request_id, 17)
        await expect(ws, 0x12, request_id, 9)
        move = await expect(ws, 0x13, request_id)
        n = struct.unpack("<H", move[9:11])[0]
        assert len(move) == 11 + n, "move packet length mismatch"
        result = move[11:].decode()
        ok = result in legal_successors(fen)
        print(f"[mock] {'OK  ' if ok else 'FAIL'} {mode:5} {fen}\n             -> {result}")
        if not ok:
            reason = b"FEN is not exactly one legal successor"
            await ws.send(bytes([0xFF]) + struct.pack("<H", len(reason)) + reason)
            await ws.close()
            return
        await ws.send(bytes([0x14]) + struct.pack("<Q", request_id))

    print("[mock] all positions answered correctly; idling (Ctrl+C to quit)")
    await ws.wait_closed()


async def process_request(connection, request):
    auth = request.headers.get("Authorization") or ""
    ok = auth == f"Bearer {KEY}" if KEY else (auth.startswith("Bearer ") and len(auth) > 7)
    if not ok:
        print("[mock] rejected upgrade: missing or wrong Bearer key -> HTTP 401")
        return connection.respond(401, "unauthorized\n")
    return None


async def main():
    async with websockets.serve(handler, HOST, PORT, process_request=process_request):
        print(f"[mock] listening on ws://{HOST}:{PORT}/ws/bot  (key: {'any Bearer key' if not KEY else 'exact match required'})")
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
