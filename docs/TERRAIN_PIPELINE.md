# Terrain Pipeline

`grpcMMO` now has a first terrain ingestion path aimed at the sibling
`grpcMMO-data` repository.

The current slice is intentionally local and practical:

- download an official Mars global DEM into `grpcMMO-data/sources/mars/`
- bake a small local patch into `grpcMMO-data/tiles/mars/patch-000/`
- emit three outputs:
  - `ground_heights.f32`
  - `ground_preview.gltf`
  - `patch.json`
- let the client render `ground_preview.gltf` automatically when it exists

## Source Datasets

The default source for the first bake path is the official USGS/Astrogeology
`Mars MGS MOLA DEM 463m` product because it is global, public, and materially
smaller than the blended 200 m raster.

Higher-detail follow-up:

- `Mars MGS MOLA - MEX HRSC Blended DEM Global 200m`

## Download

From the `grpcMMO` repo root:

```bash
python3 tools/fetch_mars_source.py --dataset mola-463m
```

Or, for the larger blend raster:

```bash
python3 tools/fetch_mars_source.py --dataset hrsc-mola-200m
```

The Python script uses only the standard library, works on Windows and Linux,
downloads into `../grpcMMO-data/sources/mars/`, and writes a
sidecar `.sha256` file.

On Windows, use `python` if that is how Python is installed on your machine.

## Build The Baker

Windows:

```powershell
cmake --preset windows
cmake --build --preset windows-debug --target grpcmmo_terrain_tool
```

Linux:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug --target grpcmmo_terrain_tool
```

## Inspect A Raster

Windows:

```powershell
.\build\windows\tools\terrain_tool\Debug\grpcmmo_terrain_tool.exe --inspect_only
```

Linux:

```bash
./build/linux-debug/tools/terrain_tool/grpcmmo_terrain_tool --inspect_only
```

By default the tool looks for:

- `../grpcMMO-data/sources/mars/mars_mgs_mola_dem_463m.tif`

## Bake A Local Patch

This example bakes a 1-degree by 1-degree patch centered on `(0, 0)`:

Windows:

```powershell
.\build\windows\tools\terrain_tool\Debug\grpcmmo_terrain_tool.exe `
  --center_lat_deg=0 `
  --center_lon_deg=0 `
  --lat_span_deg=1 `
  --lon_span_deg=1 `
  --output_rows=257 `
  --output_cols=257
```

Linux:

```bash
./build/linux-debug/tools/terrain_tool/grpcmmo_terrain_tool \
  --center_lat_deg=0 \
  --center_lon_deg=0 \
  --lat_span_deg=1 \
  --lon_span_deg=1 \
  --output_rows=257 \
  --output_cols=257
```

Outputs land in:

- `../grpcMMO-data/tiles/mars/patch-000/ground_heights.f32`
- `../grpcMMO-data/tiles/mars/patch-000/ground_preview.gltf`
- `../grpcMMO-data/tiles/mars/patch-000/patch.json`

`ground_heights.f32` stores float32 little-endian relative heights in row-major
order. Heights are normalized around the patch center sample so the preview mesh
can sit at the render origin.

## Client Integration

When `ground_preview.gltf` exists, the client copies it into `asset/model/`
at startup and uses it as the ground mesh instead of the placeholder cube.
If no baked preview is present, the old placeholder ground remains in place.

## What This Does Not Do Yet

- no runtime LOD
- no cube-sphere patch layout yet
- no collision/physics terrain representation
- no server-side terrain queries yet
- no direct GeoTIFF rendering in the client

This slice is only the first stable bridge from official planetary DEMs to a
runtime-friendly local patch artifact.
