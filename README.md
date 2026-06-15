# MEGA: Magic Elements in Graphical Arts

Spring 2026 SNU Graphics Programming (430.638) final project repository.

## Team Member

Sukhun Yang (@Sagit25), Kanghyeon Cho (@KangHyeoni), Taesun Kim (@alexdavid-113)

## Code Structure

```text
project/
|-- CMakeLists.txt                      # CMake build configuration
|-- build_mac.sh                        # macOS build helper script
|-- glad.c                              # GLAD OpenGL loader implementation
|-- build/                              # Generated build output and main executable
|-- src/
|   |-- main.cpp                        # App entry point, scene registration, scene switching
|   |-- shared/                         # Common rendering utilities and reusable scene infrastructure
|   |   |-- scene_module.h              # Scene interface used by main.cpp
|   |   |-- scene_transition_effect.*   # Fade transition overlay between scenes
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
`-- resources/
    |-- 0-base/                         # Shared room, furniture, and prop assets
    |-- 1-volcano/                      # Volcano, dragon, airplane, boulder, and skybox assets
    |-- 2-desert/                       # Desert skybox and pyramid textures
    `-- 3-underwater/                   # Fish, boat, caustics, seashell, and terrain assets
```

## Each Member's work
- Sukhun Yang: [Sukhun-Yang.md](Sukhun-Yang.md)
