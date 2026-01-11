# Asteroidz

An asteroid-inspired RTS simulation developed in C with SDL3.

## Description

Asteroidz is a real-time asteroid field simulation where you observe and navigate through a dynamically generated asteroid environment. Asteroids spawn based on density patterns, collide with each other, split into smaller pieces, and create spectacular particle effects. Navigate through space using camera controls to explore different regions of the asteroid field.

## Features

- **Dynamic Asteroid Spawning**: Asteroids spawn based on procedural density patterns around the camera
- **Realistic Collisions**: Asteroids collide and split into smaller fragments
- **Particle Effects**: Explosions with sparks and puffs when asteroids collide
- **Camera Controls**: Navigate through the asteroid field with edge scrolling and zoom
- **Procedural Generation**: Infinite asteroid field generated on-the-fly
- **Performance Optimized**: Asteroids despawn when out of view range

## Prerequisites

- **CMake** (Version 3.16 or higher)
- **C Compiler** (GCC, Clang, or MSVC with C11 support)
- **Git** (for cloning the repository and submodules)

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/CliveBanner/Asteroidz.git
cd Asteroidz
```

### 2. Initialize Submodules

This project uses SDL3 as a submodule. Initialize the submodules with:

```bash
git submodule update --init --recursive
```

## Building

### Linux / macOS

```bash
# Create build directory
mkdir build
cd build

# Configure CMake
cmake ..

# Compile the project
make
```

### Windows (with Visual Studio)

```bash
# Create build directory
mkdir build
cd build

# Configure CMake (for Visual Studio)
cmake .. -G "Visual Studio 17 2022"

# Compile the project
cmake --build . --config Release
```

### Windows (with MinGW)

```bash
# Create build directory
mkdir build
cd build

# Configure CMake
cmake .. -G "MinGW Makefiles"

# Compile the project
mingw32-make
```

## Running the Simulation

After successful compilation, you'll find the executable in the `build` directory:

### Linux / macOS

```bash
./asteriodz
```

### Windows

```bash
# Release build
.\Release\asteriodz.exe

# Or for Debug build
.\Debug\asteriodz.exe
```

The simulation runs in fullscreen mode by default.

## Controls

- **Mouse Movement to Screen Edges**: Pan camera (edge scrolling)
- **Mouse Wheel**: Zoom in/out
- **ESC**: Exit simulation

## How It Works

### Dynamic Spawning
Asteroids spawn dynamically around the camera based on a procedural density function. Dense regions have more asteroids, while sparse regions have fewer.

### Collision System
When two asteroids collide:
- Both asteroids are destroyed
- Explosion particles are generated
- If the asteroids are large enough (radius > 60), they split into two smaller asteroids each
- Smaller fragments inherit directional momentum from the collision

### Performance
- Asteroids beyond the despawn range are automatically removed
- New asteroids spawn at the edge of the visible range
- Maximum asteroid count adjusts based on local density (20-500 asteroids)

## Project Structure

```
Asteroidz/
├── CMakeLists.txt      # Build configuration
├── include/            # Header files
│   ├── constants.h     # Game constants and configuration
│   ├── game.h          # Game logic structures
│   └── ...             # Other headers
├── src/                # Source code
│   ├── main.c          # SDL3 entry point and main loop
│   ├── game.c          # Game logic and physics
│   ├── renderer.c      # Rendering and graphics
│   ├── input.c         # Input handling
│   └── utils.c         # Utility functions
├── vendored/           # External libraries
│   └── SDL/            # SDL3 (submodule)
└── README.md           # This file
```

## Technologies

- **Programming Language**: C (C11)
- **Build System**: CMake
- **Graphics Library**: SDL3
- **Rendering**: Software rendering with procedurally generated textures

## Troubleshooting

### SDL3 Submodule Missing

If SDL3 is not found, make sure you've initialized the submodules:

```bash
git submodule update --init --recursive
```

### Black Screen on Startup

The simulation generates asteroid textures on first launch, which may take a few seconds. You'll see a loading screen during this process.

### Compiler Errors

Ensure your compiler supports C11. With GCC/Clang, this is automatically set by CMake.

### CMake Version Too Old

Update CMake to version 3.16 or higher:

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install cmake

# macOS (with Homebrew)
brew install cmake

# Windows
# Download from https://cmake.org/download/
```

## License

*(Add your license information here)*

## Author

CliveBanner

## Contributing

Contributions are welcome! Feel free to open issues or pull requests.
