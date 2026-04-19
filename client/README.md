# Client

This directory now contains the first Frame-backed MMO client slice.

The current `grpcmmo_client` target:

- logs in through `auth_server`
- opens a bidirectional `game_server` session
- keeps the server authoritative for movement
- interpolates replicated state locally
- renders a simple placeholder ground scene and follows the controlled entity with a third-person camera

If `../grpcMMO-data/tiles/mars/patch-000/ground_preview.gltf` exists, the client
will copy that baked preview mesh into its local model assets and use it
instead of the placeholder ground cube.

For workspace development, `grpcMMO` expects the following repositories by default:

- `external/frame`
- `../grpcMMO-data`

Current limitation:

- placeholder actor meshes are not active yet because the first stable path is the ground scene plus authoritative camera motion
