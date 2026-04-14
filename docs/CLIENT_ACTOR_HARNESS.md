# Client Actor Harness

The client now follows an Unreal-style runtime split instead of putting
everything directly in `Client`, `Scene`, and a single pawn.

## Runtime Layers

- `Client`: app bootstrap, auth/session connection, network polling, move send cadence
- `ClientWorld`: client-side world runtime, actor registry, local world bootstrap, camera routing
- `FrameSceneBridge`: the only class that talks directly to Frame scene/node handles
- `CameraDirector`: owns named camera actors and selects the active camera
- `PlayerController`: local input collection, camera orbit input, local movement drive

## Actor Hierarchy

```text
Actor
  WorldActor
    PlanetActor
    StaticPropActor
    InteractivePropActor
  Pawn
    Character
      PlayerCharacter
      NpcCharacter
  CameraActor
```

## Component Base

The initial harness includes a root scene component on every actor:

```text
ActorComponent
  SceneComponent
```

That is intentionally small for now. It gives the client one place to grow
mesh, animation, interaction, movement, and replication components later.

## Current Rendering Scope

The visual slice is still intentionally narrow:

- one local planet/ground representation
- one controlled character mesh
- one active gameplay follow camera

The important change is structural: the code now has dedicated places for
planets, static props, interactive world actors, characters, controllers, and
multiple cameras without keeping all of that logic in `Client`.
