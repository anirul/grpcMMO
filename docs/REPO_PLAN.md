# Repo Plan

## Phase 0: bootstrap

- keep the repo outside `grpcMUD`
- wire the top-level CMake structure
- keep auth, gameplay, and storage physically separated
- lock in the storage abstraction before gameplay code starts depending on SQLite directly

## Phase 1: contracts

Create protobuf packages for:

- auth
- realtime session
- world replication
- combat
- chat
- storage DTOs

Keep login and realtime gameplay as separate APIs.

## Phase 2: storage backends

Implement:

- `SQLiteStorageBackend`

Design for later:

- `PostgresStorageBackend`
- `storage_server` facade over the same repository interfaces

The important point is that the game code should depend on the interface, not the concrete backend.

## Phase 3: auth server

Build:

- account login
- character creation
- session token creation
- handoff contract for connecting to a game server

## Phase 4: game server

Build:

- authoritative tick loop
- one zone
- player spawn
- movement
- replication snapshots and deltas
- reconnect-safe session ownership

## Phase 5: Frame client

Build:

- a new client protocol adapter for the MMO messages
- scene/entity replication cache
- incremental scene updates
- login and character select UI

Avoid reusing the old `grpcMUD` local-grid protocol directly. The MMO should replicate entities and state deltas instead.

## Phase 6: scale-up options

When needed:

- swap SQLite for Postgres
- add `storage_server`
- add more zones
- add cross-zone transfers
- add soak testing and replay capture

## Migration Notes From `grpcMUD`

Port concepts:

- authoritative simulation
- gRPC-based messaging
- Frame rendering integration

Do not port directly:

- one giant world-state object
- one giant client application class
- world authoring mixed with live persistence

