# Space Explorer (Unreal Engine 5, C++)

Open-world procedural galaxy adventure: fly between 2,000 generated star systems, land on planets, mine and scan, fight or befriend creatures and settlers, trade, research, build bases and upgrade your ship. All 14 build phases are implemented in code; see *Status* below for what is placeholder art.

## Requirements
- Unreal Engine 5.4+ (set `EngineAssociation` in `SpaceExplorer.uproject` for other 5.x)
- Visual Studio 2022 with "Game development with C++" (Windows) or Xcode (macOS)
- Plugin `ProceduralMeshComponent` (ships with the engine; enabled in the .uproject)

## Setup
1. Right-click `SpaceExplorer.uproject` > *Generate Visual Studio project files*.
2. Open the `.sln`, set *Development Editor*, build.
3. Open the project, create an **empty** level, save it as `Content/Maps/MainMap`. Do not add a floor, lights or a Player Start: the game mode builds the planet, sun, sky and your ship.
4. Press Play. Choose *New Game*.

## Build / package
Editor: *Platforms > Windows > Package Project*. CLI: `RunUAT BuildCookRun -project=SpaceExplorer.uproject -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive`.

## Your first ten minutes
1. You start on Haven. Walk to the settlement (about 160 m away, look for the beacon pole) and press **F** on a settler.
2. Accept a mission, then hold **left mouse** on a coloured boulder to mine it. Press **T** to scan things for research.
3. **Tab** > Crafting: turn iron ore into plates, carbon + plates into fuel cells.
4. **Tab** > Research: spend research points on ship and survival upgrades.
5. **B** to build a Habitat (breathable air + healing) from iron plates and silicon.
6. Board your ship (**F**), hold **Space** to climb out of the atmosphere, open the map (**M**), pick a world and *Go*. It costs fuel.

## Architecture
| Folder | Contents |
|---|---|
| `Core` | game mode, player controller (UI state + actions + console), interaction/scan interfaces, visual helpers |
| `Data` | items, recipes, tech tree, structures, factions, price model (code-defined, no data assets) |
| `Progression` | credits, research, XP, reputation, discoveries, missions, settings; one saved struct |
| `Galaxy` | deterministic galaxy: star systems, planets, moons from a seed; travel |
| `World` | terrain chunks and streamer, populator (ore, ruins, settlements, creatures, foliage), environment (sun, sky, fog, ocean, weather), base structures |
| `Player`, `Vitals`, `Inventory` | character, health/oxygen/energy, stack inventory |
| `Ship` | 6-DOF ship with fuel, shields, hull, cargo, upgrades, cameras |
| `Creatures`, `NPC` | animal AI and settler AI (shared `ASEWalker` base) |
| `SaveSystem` | versioned slots, autosave, planet-crossing loads, travel snapshots |
| `UI` | canvas HUD and all menu screens |
| `Tests` | automation tests |

Extending: add items/recipes/tech/structures in `Data/SEGameData.cpp`; add creature archetypes in `SECreatureGen::Generate`; add biomes in `ESEBiome` plus `SETerrain::Height/Color`.

## Tests
Editor > *Tools > Session Frontend > Automation*, filter `SpaceExplorer`. Covers galaxy determinism, terrain seams, inventory, vitals, missions and species generation.

## Status and honest limitations
- **Not compiled yet.** This was written without access to the Unreal compiler. Expect a handful of compile errors on first build (typically a missing include or a changed API name). Send them back and they are quick fixes.
- Art is procedural primitives (cubes, spheres, cones). Terrain uses an engine debug vertex-colour material with lighting baked into vertex colours. There is no audio and no animation. Swap in meshes, materials and sounds when you have assets.
- Terrain is a streamed heightfield, so **there are no caves**; ruins and settlements provide points of interest instead.
- Planet-to-planet travel is a hyperdrive jump with a level reload, not a seamless atmosphere-to-orbit transition.
- Space stations, asteroid-field gameplay and a full creature encyclopedia UI beyond the Codex list are not implemented.
- UI is a canvas HUD with mouse-clickable buttons, not UMG.
