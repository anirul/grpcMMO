# Architecture

## Core Rule

Keep auth, gameplay, and persistence as separate concerns even when they run on the same machine.

That means:

- `auth_server` never owns realtime world simulation
- `game_server` never owns account login flows
- storage is accessed through an interface, not through direct SQLite calls scattered across the codebase

## Service Responsibilities

### `auth_server`

Owns:

- account login
- password or token verification
- session token issuance
- character list and character creation
- handing the client off to the target realm or game server

Does not own:

- zone simulation
- movement
- combat
- replication

### `game_server`

Owns:

- authoritative tick loop
- world and zone state
- movement, combat, NPC logic
- area-of-interest filtering
- replication and realtime session streams
- zone transfer coordination

Does not own:

- account authentication
- password handling
- direct storage implementation details

### `storage`

This is an abstraction layer, not a deployment decision.

Start with:

- in-process SQLite backend for local development

Be ready for:

- Postgres backend for larger environments
- optional `storage_server` if multiple services later need centralized persistence access

## Storage Recommendation

Do not force a dedicated DB service on day one.

Recommended path:

1. define repository-style interfaces in `storage/`
2. implement SQLite first
3. add Postgres as a second backend when needed
4. only add `storage_server` if operational separation becomes useful

That keeps the code portable without introducing unnecessary network hops early.

## Runtime Profiles

### Profile A: local

- `auth_server`
- `game_server`
- SQLite file on disk

Best for:

- fast iteration
- single developer testing
- initial vertical slices

### Profile B: shared db

- `auth_server`
- `game_server`
- Postgres

Best for:

- team integration
- multiple processes sharing durable state
- more realistic staging

### Profile C: service-backed persistence

- `auth_server`
- `game_server`
- `storage_server`
- Postgres

Best for:

- centralizing persistence policy
- isolating database credentials
- handling persistence from several runtime services later on

## Repo Boundaries

### `proto/`

Split contracts by concern:

- `auth/v1`
- `session/v1`
- `world/v1`
- `chat/v1`
- `combat/v1`
- `storage/v1`

### `shared/`

Low-level code safe for any binary:

- IDs
- config
- logging
- tick/time helpers

### `game/`

Pure gameplay logic that should compile without gRPC and without Frame.

### `storage/`

Repository interfaces and backend implementations.

### `client/`

Future Frame client. Keep raw Frame access behind a dedicated rendering boundary.

## First Vertical Slice

The first real slice should prove:

- login through `auth_server`
- token-based connect into `game_server`
- SQLite-backed account and character persistence
- one playable zone
- player spawn and movement
- entity replication to a Frame client

Do not start with inventory, quests, or distributed sharding.

