# LOOM Sandbox: Flappy Bird

A small Flappy Bird clone built entirely on [LOOM](https://github.com/dev-siavash/LOOM), the C++ 2D game framework with a textile metaphor.

The sandbox is LOOM's testbed. Every new framework feature gets exercised here, so it doubles as a reference for how LOOM's API reads in a real game. The game code in `main.cpp` makes no direct SDL or entt calls, only LOOM.

## How to play

- Click the start screen to begin.
- Click to flap.
- Fly between the pipes to score. Hitting a pipe or the ground ends the run.
- Click the game-over screen to return to the start screen.

## What it demonstrates

| LOOM feature | Where it shows up |
| --- | --- |
| Tows and Fibers | Background, base and score label are built with `CreateTow()` and `PlyFiber<...>()` |
| Custom Fibers and tags | `PlayerTag`, `PipeTag`, `ScorePoint`, `StartPromptTag`, `GameOverPromptTag`, `Timer`, `Score` |
| Prefabs | `StartPrompt`, `GameOverPrompt`, `Player`, `UpperPipe`, `LowerPipe`, registered with `PreFabricate(...).Register()` and spawned with `SpawnPrefab()` |
| Bound shuttles | `AddBoundShuttle<PlayerTag>(...)` and friends: each shuttle only receives the Tows it cares about |
| Sheds and ordering | Shuttles spread across `Update` (orders 0-4) and `PostUpdate` (orders 0-1) |
| Adding and removing shuttles at runtime | `PipeGeneratorShuttle` is added when a run starts and removed on reset |
| Services | `InputState`, `MusicPlayer` and `CollisionEvent` through `GetService<T>()` |
| Physics | `PhysicsBody` on the bird, a global gravity scale, collision pairs for the lose condition |
| Rendering | Sorting order, scaling, rotation (bird tilt), a flipped sprite for the upper pipe |
| Text | The live score label |
| Audio | Looping music plus flap, point and hit sound effects through `AudioPlayer` |
| UI | Full-screen `Clickable` prompts for the start and game-over screens |

## How the game is structured

There is no state machine. Game state is simply which Tows exist, and shuttles bound to a tag only run while a matching Tow is present.

| Shuttle | Shed / order | Bound to | What it does |
| --- | --- | --- | --- |
| `StartPromptShuttle` | Update / 0 | `StartPromptTag` | On click, removes the prompt, spawns the player and registers the pipe generator |
| `GameOverPromptShuttle` | Update / 0 | `GameOverPromptTag` | On click, resets the score and brings back the start prompt |
| `PipeGeneratorShuttle` | Update / 1 | none (added at runtime) | Spawns an upper and lower pipe prefab at a random height on a timer |
| `PipeScrollerShuttle` | Update / 2 | `PipeTag` | Scrolls pipes left and removes them off-screen |
| `PlayerControllerShuttle` | Update / 3 | `PlayerTag` | Flaps on click and tilts the bird with its velocity |
| `ScoreDetectionShuttle` | Update / 4 | `ScorePoint` | Awards a point when a pipe pair passes the bird, then severs the tag so it counts once |
| `ScoreTextUpdateShuttle` | PostUpdate / 0 | `Score`, `Text` | Writes the score into the label |
| `LoseConditionShuttle` | PostUpdate / 1 | `PlayerTag` | Reads collision pairs, plays the hit sound and resets the run |

## Building

### Requirements

- Windows with Visual Studio 2022 (MSVC v143, "Desktop development with C++")
- C++20
- [entt](https://github.com/skypjack/entt), header-only: clone it anywhere and point the include path at its `src` folder
- The [LOOM](https://github.com/dev-siavash/LOOM) repository
- SDL2, SDL2_image, SDL2_mixer and SDL2_ttf, restored automatically by NuGet from `packages.config`

### Expected layout

`Sandbox.vcxproj` references the LOOM library project and the NuGet `packages` folder relative to itself, so the folders should sit side by side:

```
<solution root>/
├─ LOOM/        LOOM library project (LOOM.vcxproj and include/)
├─ Sandbox/     this project
└─ packages/    NuGet packages (created on restore)
```

### Steps

1. Open the solution in Visual Studio and let NuGet restore the SDL packages.
2. Open the Sandbox project properties, go to **C/C++ > General > Additional Include Directories** and set the two machine-specific entries to your local entt `src` folder and your LOOM library folder (the one that contains `include/`).
3. Select the **Debug | x64** configuration and build.

### Running

Press F5 from Visual Studio. Assets are loaded through relative paths (`./assets/...`), so the working directory must be the project folder, which is Visual Studio's default. To run the `.exe` on its own, copy the `assets/` folder next to it.

### Known limitations

- **Debug | x64 is the only configured build.** The C++20 setting and include paths are only set for that configuration, so Release and Win32 won't build as-is.
- The include paths are absolute paths from the author's machine and need editing on any other computer.
- A CMake build is planned, which will replace the manual setup above.

## Tuning

The constants at the top of `main.cpp` control how the game feels:

- `PIPE_SPAWN_INTERVAL`: seconds between pipe pairs
- `PIPE_SCROLL_SPEED`: how fast pipes move left
- `PIPE_GAP`: distance between the centres of the upper and lower pipe
- `BIRD_JUMP_FORCE`: strength of each flap
- `BIRD_TILT_SENSITIVITY`, `BIRD_TILT_SMOOTHING`, `BIRD_MAX_UP_TILT`, `BIRD_MAX_DOWN_TILT`: bird rotation behaviour
- `loom.Settings().globalGravityScale`: gravity multiplier (set in `main()`)

## Assets and credits

The `assets/` folder holds the sprites, sound effects, music and font. It also contains extras the game doesn't use yet, such as other bird colour, and a daytime background.

<!-- TODO: add the source and licence for the sprites, audio and font. -->

## Related

- [LOOM](https://github.com/dev-siavash/LOOM): the framework this sandbox is built on
