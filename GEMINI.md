# Asteroidz - Project Overview (for Gemini CLI)

Asteroidz is a real-time asteroid field RTS simulation developed in C11 using SDL3. It features procedural generation, physics-based interactions, and a complex entity management system.

## 🚀 Technology Stack

- **Language:** C11
- **Graphics:** SDL3 (Vendored in `vendored/SDL`)
- **Build System:** CMake (3.16+)
- **OS:** Linux/macOS/Windows

## 📂 Project Structure & Core Components

| Component | Files | Responsibility |
| :--- | :--- | :--- |
| **Main Loop** | `src/main.c` | Entry point, timing, and top-level event handling. |
| **Game State** | `src/game.c`, `include/game.h` | Core game loop logic, entity updates, and state management. |
| **Physics** | `src/physics.c`, `include/physics.h` | Collision detection (AABB/Circular), splitting logic, and gravity. |
| **Renderer** | `src/renderer.c`, `include/renderer.h` | SDL3-based drawing, camera management, and view transformation. |
| **Assets** | `src/assets.c`, `include/assets.h` | Procedural texture generation for asteroids and celestial bodies. |
| **AI** | `src/ai.c`, `include/ai.h` | Unit steering, pathfinding, and decision-making logic. |
| **UI** | `src/ui.c`, `include/ui.h` | HUD, minimap, and interactive command interfaces. |
| **Combat** | `src/weapons.c`, `src/abilities.c` | Weapon systems (Lasers, Cannons), cooldowns, and special abilities. |
| **VFX** | `src/particles.c`, `include/particles.h` | Particle systems for explosions, engine trails, and muzzle flashes. |
| **Persistence** | `src/persistence.c`, `include/persistence.h` | Saving and loading game state to `savegame.dat`. |
| **Workers** | `src/workers.c`, `include/workers.h` | Threading or background task management (verify implementation). |

## ⚙️ Key Constants & Configuration (`include/constants.h`)

- **View:** 1280x720 Logical Resolution.
- **Camera:** Zoom (0.2x to 1.0x), Edge scrolling enabled (threshold 20px).
- **Physics:** 
    - `MAX_ASTEROIDS`: 2048
    - `ASTEROID_MIN_RADIUS`: 200.0f
    - `ASTEROID_COLLISION_SPLIT_THRESHOLD`: 600.0f
- **VFX:** `MAX_PARTICLES`: 4096.
- **AI:** `MAX_UNITS`: 128.

## 🛠️ Development Workflow

### Build Instructions
```bash
mkdir -p build && cd build
cmake ..
make
```

### Running
```bash
./asteriodz
```

## 🎮 Controls
- **Mouse Edges:** Pan camera.
- **Mouse Wheel:** Zoom.
- **ESC:** Exit.

## 📝 Developer Notes
- The game uses a **logical coordinate system** for gameplay, mapped to the screen resolution via `SDL_RenderSetLogicalPresentation`.
- **Procedural Generation:** Celestial bodies (Galaxies, Planets) are placed on a grid, and asteroids are spawned based on local density functions.
- **Memory Management:** Fixed-size arrays are preferred for entities (asteroids, particles) to avoid frequent allocations.
