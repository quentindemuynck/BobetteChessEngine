# BobetteChessEngine

A chess engine: takes a position as a FEN string, calculates the best move, and
plays it on the chess backend over a WebSocket (application protocol v2).

> Status: the bot connection (`engine_ws`) is complete. The engine itself is a
> placeholder that plays the **first legal move**; `evaluate()` and the real
> search are still TODO.

## Building in Visual Studio

Requirements: Visual Studio 2022 with the **Desktop development with C++**
workload (includes MSVC, CMake and Ninja), and Git (dependencies are fetched at
configure time).

1. **File → Open → Folder…** and select this folder (the one containing
   `CMakeLists.txt`). Visual Studio detects `CMakePresets.json` automatically.
2. Pick a configuration in the toolbar dropdown: **Debug (x64)** or **Release (x64)**.
   The first configure downloads the dependencies (mbedTLS takes a minute) —
   watch the Output window.
3. **Build → Build All** (Ctrl+Shift+B).
4. Tests: **Test → Test Explorer**, or **Test → Run CTests for BobetteChessEngine**.

Build output goes to `out/build/<preset>/`.

## Running the bot

### The auth key

Put the key in a file called **`auth_key.txt` in the project root** (next to
`CMakeLists.txt`). The file contains only the key; surrounding spaces and
newlines are trimmed. `auth_key.txt` is in `.gitignore`, so it never gets committed.

`engine_ws` finds it automatically: it searches the working directory and its
parents, then the executable's folder and its parents. No environment variable
or IDE setting is needed. The log shows which file was used, never the key.

Alternatives: the `BOBETTE_AUTH_KEY` environment variable, or
`BOBETTE_AUTH_KEY_FILE` / `--key-file` pointing at a file somewhere else. There
is no `--key` flag on purpose (it would show up in shell history and process lists).

### From JetBrains Rider

1. Open the folder; Rider loads `CMakeLists.txt` and the presets. If it can't
   find `cl.exe`, set the toolchain to **Visual Studio** under
   **Settings → Build, Execution, Deployment → Toolchains**.
2. **Run → Edit Configurations… → + → CMake Application**
   - Name: `engine_ws public` · Target: `engine_ws` · Executable: `engine_ws`
   - Program arguments: `--public`
3. Add a second one with empty program arguments for the local mock.
4. Pick the configuration in the toolbar dropdown, then **Run** (Shift+F10, or
   Ctrl+F5 with the Visual Studio keymap) or the green ▶ button.

### From Visual Studio

`.vs/launch.vs.json` defines two entries for the "Select Startup Item" dropdown
(public server with `--public`, and the local mock). Pick one and press Ctrl+F5.

### From a terminal

```powershell
cd out\build\release\ws_bridge
.\engine_ws.exe --public      # finds auth_key.txt in the project root
.\engine_ws.exe --help
```

| Option / env var | Default |
|---|---|
| `--public` / `--url <url>` / `BOBETTE_URL` | `ws://127.0.0.1:3000/ws/bot` |
| `--key-file <path>` / `BOBETTE_AUTH_KEY_FILE` / `BOBETTE_AUTH_KEY` | — (required for `wss://`) |
| `--identity <id>` / `BOBETTE_IDENTITY` | `bobette` (keep it stable: games resume on reconnect) |
| `--name <name>` / `BOBETTE_NAME` | `Bobette` |
| `--modes <a,b>` / `BOBETTE_MODES` | `fast,deep` |
| `--ca-file <pem>` / `BOBETTE_CA_FILE` | Windows certificate store |

### Reading the log

Each line is tagged with the stage it belongs to:

| Tag | Meaning |
|---|---|
| `[tcp/tls]` | DNS, TCP or TLS handshake / certificate problem |
| `[auth]` | HTTP 401: wrong or missing key — the bot exits (code 3) instead of retrying |
| `[upgrade]` | WebSocket upgrade result (HTTP 101 = good) |
| `[protocol]` | register / registered / predict / accepted |
| `[engine]` | the move the engine chose and how long it took |
| `[server-error]` | an `error` packet from the backend |

Example of a healthy session:

```
[upgrade] HTTP 101, WebSocket open
[protocol] sent register (v2)
[protocol] registered as 'bobette', waiting for predictions
[protocol] predict #12 mode=fast fen=rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
[engine] move #12 -> rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b KQkq - 0 1 (0 ms)
[protocol] accepted #12
```

## Testing locally with the mock backend

`tools/mock_backend.py` speaks the same protocol as the real server, checks
the packet order and IDs, and verifies every returned FEN with python-chess.

```powershell
pip install websockets python-chess
python tools\mock_backend.py
```

Then run `engine_ws` **without** `--public` (it defaults to the local mock's
address). The mock accepts any Bearer key, so your `auth_key.txt` works for both,
and prints `OK` / `FAIL` per position.

## Build configurations

| Preset    | Flags                                                        |
|-----------|--------------------------------------------------------------|
| `debug`   | `/W4` on project code, optional `/fsanitize=address`         |
| `release` | `/O2`, link-time optimization (`/GL` + `/LTCG`)              |

ASan is off by default: set the cache variable `ENGINE_ENABLE_ASAN=ON`
(**Project → CMake Settings / Edit Cache**) and reconfigure.

Warning flags (`/W4 /permissive-`) are applied per target to our own code only.

## Dependencies

Fetched with CMake `FetchContent`, all pinned:

| Library | Pin | Used by |
|---------|-----|---------|
| [Disservin/chess-library](https://github.com/Disservin/chess-library) | commit `4302664` | `engine_core` (board, move generation) |
| [IXWebSocket](https://github.com/machinezone/IXWebSocket) | `v12.1.0` (TLS on, zlib off) | `engine_ws` only |
| [mbedTLS](https://github.com/Mbed-TLS/mbedtls) | `v3.6.7` | TLS backend for IXWebSocket |
| [doctest](https://github.com/doctest/doctest) | `v2.5.3` | `engine_tests` |

## Project layout

```
CMakeLists.txt          top level: options, flags, FetchContent, add_subdirectory
CMakePresets.json       debug / release presets for Visual Studio
engine/                 engine_core (static library) — no networking
  include/engine/
    search.hpp          findBestMove(const chess::Board&, const TimeLimit&)
    evaluate.hpp        evaluate(const chess::Board&)
    time_manager.hpp    TimeLimit struct + allocateTime()
uci/                    engine_uci — UCI protocol over stdin/stdout (stub)
ws_bridge/
  protocol.*            binary protocol v2 encode/decode           ┐ bot_protocol
  prediction.*          FEN + mode -> engine -> resulting FEN      ┘ (no sockets)
  config.*              URL / key / identity / modes from env + flags
  bot_client.*          IXWebSocket client: register, answer, reconnect
  main.cpp              engine_ws entry point
tests/                  engine_tests — doctest (protocol fixtures from the spec,
                        castling / en passant / promotion FEN checks)
tools/mock_backend.py   local fake backend for end-to-end testing
```

Modes are interpreted in `ws_bridge/prediction.cpp` (`timeLimitForMode`):
`fast` = 200 ms, `deep` = 3 s, anything else = 1 s.
