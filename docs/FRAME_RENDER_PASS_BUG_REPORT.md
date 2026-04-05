# Frame Bug Report: `PRE_RENDER_TIME` vs `SCENE_RENDER_TIME` Semantics

## Summary

`Frame` currently infers any level with non-skybox meshes as a raytracing scene and then assigns the internal raytracing pipeline so that:

- `SCENE_RENDER_TIME` is the fullscreen raytrace pass
- `PRE_RENDER_TIME` is the geometry source/preprocess pass

This conflicts with the expected public meaning of the render-time enums:

- `SCENE_RENDER_TIME` should be the normal per-frame scene rendering pass
- `PRE_RENDER_TIME` should be reserved for preprocessing / precomputation / static pre-pass work

For client code such as `grpcMMO`, this makes moving scene meshes behave backwards from the API name. To make ordinary scene geometry visible in the inferred raytracing preset, the meshes must be placed in `PRE_RENDER_TIME`, which is semantically misleading and makes scene authoring/debugging very hard.

## Expected

- Scene geometry such as player meshes, terrain meshes, and other world objects should render in `SCENE_RENDER_TIME`.
- `PRE_RENDER_TIME` should be optional and only used for preprocessing or auxiliary passes.
- A level author should not have to place ordinary moving scene meshes in `PRE_RENDER_TIME` just to make them appear in the world.

## Actual

The current internal preset logic maps the inferred raytracing scene like this:

- `SCENE_RENDER_TIME -> RayTraceProgram`
- `PRE_RENDER_TIME -> RayTraceProgram + RayTracePreprocessProgram`

Relevant engine code:

- [external/frame/frame/json/level_data.cpp](C:/Users/frede/github/grpcMMO/external/frame/frame/json/level_data.cpp#L357)
- [external/frame/frame/json/level_data.cpp](C:/Users/frede/github/grpcMMO/external/frame/frame/json/level_data.cpp#L359)

Any non-skybox mesh scene is automatically inferred as `SupportedScenePreset::Raytrace`:

- [external/frame/frame/json/level_data.cpp](C:/Users/frede/github/grpcMMO/external/frame/frame/json/level_data.cpp#L413)

The OpenGL scene parser also treats `SCENE_RENDER_TIME` with a raytracing program as `RayTraceMaterial` rather than ordinary scene geometry:

- [external/frame/frame/opengl/json/parse_scene_tree.cpp](C:/Users/frede/github/grpcMMO/external/frame/frame/opengl/json/parse_scene_tree.cpp#L316)
- [external/frame/frame/opengl/json/parse_scene_tree.cpp](C:/Users/frede/github/grpcMMO/external/frame/frame/opengl/json/parse_scene_tree.cpp#L319)

The OpenGL renderer then gives `SCENE_RENDER_TIME` special raytracing treatment and can override the model matrix with identity for world-space buffers:

- [external/frame/frame/opengl/renderer.cpp](C:/Users/frede/github/grpcMMO/external/frame/frame/opengl/renderer.cpp#L725)
- [external/frame/frame/opengl/renderer.cpp](C:/Users/frede/github/grpcMMO/external/frame/frame/opengl/renderer.cpp#L729)

## Repro

1. Create a level with ordinary world meshes: player cube, ground plane, landmarks.
2. Mark those meshes as `SCENE_RENDER_TIME`, which is the natural expectation for a normal scene.
3. Load the level through the current `Frame` auto-inferred preset path.
4. Observe that the scene is not treated as a normal scene-render pass. Internally, it is pushed into the raytracing scene path where `SCENE_RENDER_TIME` is the fullscreen raytrace pass and geometry is expected in `PRE_RENDER_TIME`.

## Impact

- Public enum names do not match the effective behavior in the inferred raytracing preset.
- Consumer projects are pushed toward writing incorrect-looking scene descriptions.
- Debugging moving objects becomes confusing because the natural enum assignment is not the working one.
- The current behavior makes it hard to build a normal third-person client on top of `Frame` without knowing internal preset rules.

## Suggested Fix

One of these should be done:

1. Introduce a distinct non-raytracing scene preset where ordinary meshes render in `SCENE_RENDER_TIME`.
2. Stop auto-inferring every non-skybox mesh scene as `SupportedScenePreset::Raytrace`.
3. Let the level explicitly choose its render-pass program mapping instead of hardwiring it through the inferred preset.
4. Reserve `PRE_RENDER_TIME` for true preprocessing only, and keep ordinary scene meshes in `SCENE_RENDER_TIME`.

## Notes

This report is about the public semantic mismatch, not just one `grpcMMO` client bug. The current engine behavior may be internally consistent for the shared raytracing pipeline, but the enum/API meaning exposed to scene authors is misleading enough that it causes real integration mistakes.
