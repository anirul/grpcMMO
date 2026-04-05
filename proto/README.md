# Proto Layout

This directory is intentionally split by concern from the start:

- `auth/v1`
- `session/v1`
- `world/v1`
- `chat/v1`
- `combat/v1`
- `storage/v1`

The MMO should not collapse auth, realtime gameplay, and persistence transport into one proto surface.

## Current Contract Shape

- `auth/v1/auth.proto`
  - account login
  - character list / create
  - session grant handoff to the realtime server
- `session/v1/session.proto`
  - authoritative gameplay stream
  - client input frames
  - sparse replication batches
  - chat/combat/system events pushed over the same session
- `world/v1/replication.proto`
  - shared spatial and replication DTOs
  - planet-centered coordinates plus surface anchors for spherical worlds
- `chat/v1/chat.proto`
  - chat send and replicated chat envelopes
- `combat/v1/combat.proto`
  - replicated combat events
- `storage/v1/storage.proto`
  - persistence DTOs shared by storage backends and any future `storage_server`

## Networking Assumptions

- The server is authoritative.
- The client sends intent, never authoritative transforms.
- The client interpolates from sparse authoritative snapshots.
- The server only sends entities that changed in a snapshot plus removals.
- Snapshot batches carry `last_processed_input_sequence` so the controlled player can reconcile local input against server state.
