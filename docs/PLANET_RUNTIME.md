# Planet Runtime

This document defines the intended runtime structure for spherical planets in
`grpcMMO`.

## Radius

Current code default:

- Mars MOLA reference radius: `3,396,190 m`

Useful scaled references:

- Mars at `1:100`: `33,961.9 m`
- Mars at `1:200`: `16,980.95 m`

The current authoritative world defaults to the same reference radius used by
the baked MOLA preview patch so the rendered terrain and replicated positions
share one sphere.
Scaling is treated as a separate world-design decision, not as an implicit
runtime assumption.

## Coordinate Rule

- authoritative world coordinates stay in planet-centered `double`
- local gravity points to the planet center
- local `up` is the normalized vector from center to surface point
- only the final camera-relative render path should reduce precision to `float`

## Chunk Structure

Use a cube-sphere quadtree.

Chunk key:

- `planet_id`
- `face`
- `lod`
- `tile_x`
- `tile_y`

Chunk contents:

- height samples
- geometric error
- min/max height
- bounding sphere
- optional cached render mesh
- optional collision mesh

## Terrain Sampling

The GeoTIFF is an offline source, not the runtime mesh format.

For each chunk vertex:

1. map `(face, u, v)` to a cube point
2. normalize to sphere direction
3. convert direction to lat/lon
4. sample the DEM
5. place the final point at `center + direction * (radius + height)`

## Streaming

The far distance should be horizon-driven, not based on a flat rectangle.

For radius `R` and camera altitude `h`:

- line-of-sight horizon distance: `sqrt(h * (2R + h))`
- surface arc distance: `R * acos(R / (R + h))`

Use that horizon distance plus a preload margin to decide which chunks remain
loaded. Use hysteresis so chunks do not churn at the boundary.

## Current Gap

The proto layer already supports spherical data:

- `Transform.position_m`
- `Transform.up_unit`
- `SurfaceAnchor.unit_normal`

The current client runtime now uses a local patch render frame for the baked
preview terrain, but it is not yet a full chunk-streamed planetary renderer:

- movement is still constrained to the currently loaded local patch
- rendering still uses a baked preview mesh rather than streamed cube-sphere chunks
- chunk streaming and camera-relative planet rendering are not implemented yet

The new shared planet headers provide the intended math foundation:

- `shared/include/grpcmmo/shared/planet/PlanetConstants.hpp`
- `shared/include/grpcmmo/shared/planet/PlanetMath.hpp`
- `shared/include/grpcmmo/shared/planet/CubeSphereAddress.hpp`
