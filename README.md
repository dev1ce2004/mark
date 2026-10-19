# Arena Shooter

A fast-paced, arcade-style top-down arena shooter written in **C** using **SDL3**. Survive as long as you can against endless waves of enemies, rack up kills, and climb the local leaderboard.

![Gameplay](docs/gameplay.gif)

## Tech Stack

- **Language:** C
- **Library:** SDL3
- **IDE / Platform:** Visual Studio (Windows)

## Features

- **Main menu** and **game over screen**
- **HUD** showing health, ammo, reload status (currently 18-round magazine / infinite reserve) and kill count
- **Multiple enemy types:**
  - **Grunt**: standard melee enemy
  - **Runner**: small and fast
  - **Tank**: slow, high health
  - **Shooter**: ranged enemy that fires projectiles
- **Core mechanics:** shooting, dashing and health regeneration
- **Visual feedback:** floating damage numbers and a dash cooldown indicator
- **Arcade-style leaderboard** with persistent score saving

## Controls

| Key | Action |
|-----|--------|
| `W` `A` `S` `D` | Move |
| `LMB` / `Ctrl` | Shoot |
| `Space` | Dash |
| `R` | Reload |
| `Enter` | Start / Return |

## Building and Running

The project is built and run from Visual Studio. SDL3 must be linked manually:

1. Download the SDL3 development libraries for Visual Studio from the [SDL releases page](https://github.com/libsdl-org/SDL/releases).
2. Right-click the project and select **Properties**.
3. **C/C++ → General → Additional Include Directories**: add `path/to/SDL3/include`
4. **Linker → General → Additional Library Directories**: add `path/to/SDL3/lib/x64`
5. **Linker → Input → Additional Dependencies**: add `SDL3.lib`
6. Copy `SDL3.dll` from `path/to/SDL3/lib/x64` into the same folder as the built `.exe`.
7. Build and run (`F5`).

## Roadmap

- [ ] Wave-based spawning instead of constant spawns, for longer runs
- [ ] Boss fight after a set number of kills
- [ ] Sound effects and music
- [ ] Settings menu (volume, key rebinding)
- [ ] Extended scoring: combo multipliers and bonus points
- [ ] Further balancing of dash and regeneration
- [ ] Difficulty levels (still deciding whether this suits the arcade style)
