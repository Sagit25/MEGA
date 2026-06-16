# MEGA: Magic Elements in Graphical Arts

[Spring 2026 SNU Graphics Programming (430.638) course](https://3d.snu.ac.kr/class/graphics26) final project repository.

## Project Objective

The project features a wooden house shifting through distinct environments, acting as a gateway between worlds.
Each scene integrates unique graphic features: technical and artistic.
Then, corresponding landscapes are viewed through a window.

## Results

- Proposal: [PDF](./documents/Proposal_Team5.pdf)
- Presentation: [PDF](./documents/Presentation_Team5.pdf)
- Final Video: [Video](./documents/Video_Team5.mp4)

To overcome computational cost limitations and showcase richer animations, offline rendering was used to create the video.

## Team Member

Sukhun Yang, Kanghyeon Cho, Taesun Kim

### Each Member's work (Implementation)
- Sukhun Yang: [Sukhun-Yang.md](./documents/Sukhun-Yang.md), Implement the base scene and the volcano scene (Fire-Guide Feature)
- Taesun Kim: [Taesun-Kim.md](./documents/Taesun-Kim.md), Implemented the desert scene (Mirage Feature)
- Kanghyeon Cho: [Kanghyeon-Cho.md](./documents/Kanghyeon-Cho.md), Implemented the underwater scene (Skeletal-Animation, Boids Algorithm Feature), integrated all scenes into a single application and offline rendering


## Code Structure

```text
project/
|-- src/
|   |-- main.cpp                        # App entry point, scene registration, scene switching
|   |-- shared/                         # Common rendering utilities and reusable scene infrastructure
|   |   |-- scene_module.h              # Scene interface used by main.cpp
|   |   |-- scene_transition_effect.*   # scene_fade helper for scene fade transitions
|   |   |-- fade_foreground.h           # Foreground fade rendering helpers
|   |   |-- camera.h, shader.h, model.h, mesh.h, texture*.h
|   |   `-- light.h, scene.h, math_utils.h, opengl_utils.h
|   |-- 0-base/
|   |   `-- scene.cpp                   # Base indoor/asset scene
|   |-- 1-volcano/
|   |   |-- scene.cpp                   # Volcano scene orchestration and rendering
|   |   |-- particle_system.h           # Fire and meteor particle systems
|   |   `-- volcano_trajectory.*        # Toothless/dragon flight path calculations
|   |-- 2-desert/
|   |   `-- scene.cpp                   # Desert mirage scene and temperature controls
|   `-- 3-underwater/
|       |-- scene.cpp                   # Underwater scene orchestration and rendering
|       `-- animation/                  # Skeletal animation and boid/fish helpers
|-- shaders/
|   |-- shared/                         # Shared lighting, shadow, skybox, fade overlay shaders
|   |-- 1-volcano/                      # Volcano particle shaders
|   |-- 2-desert/                       # Ray tracing and heat haze shaders
|   `-- 3-underwater/                   # Underwater lighting shaders
|-- resources/
|   |-- 0-base/                         # Shared room, furniture, and prop assets
|   |-- 1-volcano/                      # Volcano, dragon, airplane, boulder, and skybox assets
|   |-- 2-desert/                       # Desert skybox and pyramid textures
|   `-- 3-underwater/                   # Fish, boat, caustics, seashell, and terrain assets
|-- CMakeLists.txt                      # CMake build configuration
|-- build_mac.sh                        # MacOS build helper script
|-- build_windows.bat                   # Windows build helper script
`-- glad.c                              # GLAD OpenGL loader implementation
```

## How to Build

### Build on MacOS

Install the required build tools and libraries with Homebrew:

```sh
brew install cmake pkg-config glfw glm assimp freetype
```

Build the project from the repository root:

```sh
cd project
./build_mac.sh
```

The build script configures CMake, compiles the executable, and writes the result to `project/build/main`.
Run the program from the build directory so shaders and resources are resolved correctly:

```sh
cd build
./main
```

To rebuild from a clean CMake directory:

```sh
./build_mac.sh --clean
```


### Build on Windows

You can build the project using the provided batch script. Run the following command from the repository root (using Command Prompt or PowerShell):

```cmd
.\project\build_windows.bat

```

To rebuild from a clean CMake directory:

```cmd
.\project\build_windows.bat --clean

```

The build script configures CMake, compiles the executable, and writes the result to `project\build\Release\main.exe`.
Run the program from the build directory so shaders and resources are resolved correctly:

```cmd
cd .\project\build
.\Release\main.exe

```

## How to Control

- `WASD` key for camera movements
- `TAB` key for scene transition
- mouse control for change view point
