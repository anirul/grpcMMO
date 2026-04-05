# Proto Interface

This document describes the first shared client/server contract for `grpcMMO`.

## Core Model

- The server is authoritative.
- The client never sends world transforms as truth.
- The client sends intent as input frames.
- The server sends sparse authoritative replication batches.
- The client interpolates between server samples and can reconcile the controlled player using `last_processed_input_sequence`.

## Why This Shape

The old `grpcMUD` stream mixed all gameplay, view data, and chat into one flat surface.
That was acceptable for a small project but too rigid for an MMO.

The new split keeps:

- auth and handoff in `auth/v1`
- realtime gameplay in `session/v1`
- reusable spatial/replication DTOs in `world/v1`
- chat and combat as imported event packages

## Client To Server

The realtime client stream starts with `BeginSession`, then sends `InputFrame` messages.

`InputFrame` contains:

- client time
- monotonic input sequence
- movement intent
- action presses

This lets the server:

- process input in order
- remain authoritative
- acknowledge the latest processed input sequence in each replication batch

## Server To Client

The server sends:

- `SessionReady`
- `ReplicationBatch`
- `ChatEnvelope`
- `CombatEvent`
- `SystemNotice`

`ReplicationBatch` is sparse:

- only changed entities are included in `entities`
- removed entities are sent in `removed_entity_ids`
- each entity update carries enough transform data for interpolation

## Interpolation

Interpolation is driven by:

- `sample_time_ms`
- `sample_tick`
- `transform`
- `interpolation_mode`

The server can choose:

- `SNAP`
- `LINEAR`
- `VELOCITY`
- `HERMITE`

The initial implementation can treat `LINEAR` and `VELOCITY` as the practical defaults.

## Planetary World Support

The replication DTOs are already shaped for a spherical world:

- `Transform.position_m` is planet-centered 3D
- `Transform.up_unit` gives the local up direction
- `SurfaceAnchor` carries `planet_id`, `zone_id`, `patch_id`, and `unit_normal`

That allows:

- spherical AOI logic
- patch or cube-sphere zoning later
- real-planet terrain sources such as Mars
