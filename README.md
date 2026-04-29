# grpcMMO

`grpcMMO` is the clean-start MMO workspace that follows `grpcMUD`.

It keeps the same broad stack:

- C++23
- CMake + vcpkg
- gRPC + Protobuf
- Frame for 3D rendering
- `grpcMMO-data` for raw and baked terrain assets
- authoritative server simulation

The repo is organized around three hard boundaries from day one:

- `auth_server`: login, accounts, session tokens, character selection
- `game_server`: realtime gameplay, simulation, replication, zoning
- `storage`: one abstraction that can talk to SQLite first and later move to Postgres or an optional `storage_server`

## Why This Shape

`grpcMUD` already proves:

- bidirectional gRPC gameplay works
- Frame can render the world
- an authoritative simulation loop is the right backbone

What changes here is separation. The MMO needs auth, gameplay, and persistence to evolve independently without turning into one large server binary.

## Deployment Profiles

### Local development

- `auth_server`
- `game_server`
- in-process SQLite storage backend

This is the default starting point.

### Shared database

- `auth_server`
- `game_server`
- Postgres backend via the same storage interface

This is the next step when SQLite stops being enough.

### Service-backed persistence

- `auth_server`
- `game_server`
- `storage_server`
- Postgres behind `storage_server`

This is optional and only needed if persistence access needs to be centralized or isolated.

## Repository Layout

```text
grpcMMO/
  cmake/
  docs/
  proto/
  shared/
  game/
  storage/
  client/
  tools/
  services/
    auth_server/
    game_server/
    storage_server/
  tests/
```

## Workspace Dependencies

The code repo uses a local `vcpkg` checkout under `external/vcpkg`, similar to `grpcMUD`.
It is intended to stay pinned to a known release tag instead of floating on `master`
and is currently aligned with `2026.03.18`.

The wider workspace is expected to live beside these sibling repositories:

```text
github/
  grpcMMO/
  grpcMMO-data/
```

- `external/vcpkg` is the local toolchain and manifest package manager used by the build presets.
- `external/frame` is the local Frame checkout used by the client-side build path.
- `grpcMMO-data` is a companion data repo for raw DEMs and baked terrain tiles.
- `grpcMMO` should compile even if the data repo is absent, but the final product should point at a valid data root.
- the installed dependency tree lives in `vcpkg_installed/` at the repo root so `cmake --preset ... --fresh` does not throw away the compiled packages

By default the build looks for:

- `external/vcpkg`
- `external/frame`
- `../grpcMMO-data`

Override them with the CMake cache variables:

- `GRPCMMO_FRAME_ROOT`
- `GRPCMMO_DATA_ROOT`

## Terrain Data Pipeline

The repo now includes a first terrain ingestion slice for Mars DEM data:

- download an official Mars source raster into `grpcMMO-data`
- bake a local patch into `ground_heights.f32`, `ground_preview.gltf`, and `patch.json`
- let the client render `ground_preview.gltf` automatically when present
- use `python tools/fetch_mars_source.py ...` for a portable download/bootstrap step

See [docs/TERRAIN_PIPELINE.md](docs/TERRAIN_PIPELINE.md).

## Planet Runtime

The spherical-world runtime foundation is now documented separately in
[docs/PLANET_RUNTIME.md](docs/PLANET_RUNTIME.md).

Current default radius in code:

- Mars MOLA reference radius: `3,396,190 m`

Reference scaled radius:

- Mars at `1:200`: `16,980.95 m`

Bootstrap the local toolchain once:

```powershell
.\external\vcpkg\bootstrap-vcpkg.bat
```

Or on Linux:

```sh
./external/vcpkg/bootstrap-vcpkg.sh
```

Then configure and build with the presets. The configure step already checks the expected import roots and can be forced to fail if `external/frame` or `../grpcMMO-data` are missing.
The project manifest pins the `vcpkg` baseline, and the local checkout should stay on the matching release tag.

## Current Scope

This workspace is a scaffold plus architecture plan, not a gameplay implementation yet.

Start in this order:

1. define the new protobuf split for auth, realtime session, world state, combat, chat, and storage DTOs
2. build a `game` simulation library that does not depend on gRPC or Frame
3. wire `auth_server` and `game_server` against a storage interface backed by SQLite
4. connect a Frame client that consumes replicated entity state instead of the old grid-view protocol

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/REPO_PLAN.md](docs/REPO_PLAN.md).

## Current Vertical Slice

The repository now includes a first minimal end-to-end slice:

- `grpcmmo_auth_server`
- `grpcmmo_game_server`
- `grpcmmo_client`

Defaults:

- auth server: `127.0.0.1:50050`
- game server: `127.0.0.1:50051`
- SQLite DB: `data/grpcmmo.sqlite3`
- demo login: `demo`
- demo password: `demo`

The auth server stores accounts, characters, and session grants in SQLite.
The game server validates session grants against the same SQLite file and runs a
minimal authoritative movement loop.

## Local Testing

Build:

```powershell
cmake --preset windows --fresh
cmake --build --preset windows-debug
```

Create a local account in the shared SQLite file:

```powershell
.\build\windows\tools\account_tool\Debug\grpcmmo_account_tool.exe --login_name=alice --password=alice123 --display_name="Alice"
```

Start the servers in two terminals:

```powershell
.\build\windows\services\auth_server\Debug\grpcmmo_auth_server.exe
```

```powershell
.\build\windows\services\game_server\Debug\grpcmmo_game_server.exe
```

Connect with the visual ground-slice client:

```powershell
.\build\windows\client\Debug\grpcmmo_client.exe --login_name=alice --password=alice123 --character_name=alice-probe --device=opengl
```

Controls:

- `W` / `S`: move forward and backward
- `A` / `D`: strafe
- `Q` / `E`: yaw left and right
- `P`: ping
- `Escape`: quit

For an automated smoke test without keyboard input:

```powershell
.\build\windows\client\Debug\grpcmmo_client.exe --login_name=demo --password=demo --character_name=explorer --device=opengl --auto_move_seconds=5 --auto_exit_seconds=12
```

Current client state:

- renders a simple placeholder surface using Frame assets
- follows the authoritative player position with a third-person camera
- interpolates replicated state from `game_server`
- actor marker meshes are not enabled yet; the camera movement path is working first

For quick smoke tests, the seeded fallback account is still:

```text
login: demo
password: demo
```
