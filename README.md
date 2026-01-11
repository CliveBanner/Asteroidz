# Asteroidz

An asteroid-inspired RTS game developed in C with SDL3.

## Description

Asteroidz is a classic arcade-style game inspired by Asteroids, where you control a spaceship and destroy asteroids.

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

## Running the Game

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

## Controls

*(Add your game controls here)*

- **Arrow Keys**: Control spaceship
- **Space**: Shoot
- **ESC**: Exit game

## Project Structure

```
Asteroidz/
├── CMakeLists.txt      # Build configuration
├── include/            # Header files
├── src/                # Source code
├── vendored/           # External libraries (SDL3)
└── README.md           # This file
```

## Technologies

- **Programming Language**: C (C11)
- **Build System**: CMake
- **Graphics Library**: SDL3

## Troubleshooting

### SDL3 Submodule Missing

If SDL3 is not found, make sure you've initialized the submodules:

```bash
git submodule update --init --recursive
```

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
