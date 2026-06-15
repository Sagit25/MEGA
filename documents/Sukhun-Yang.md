# Parts Implemented by Sukhun Yang

## 1. Technical contents

### 1.1. Surface-Guided Fire Particle System

**Main file:** `project/src/1-volcano/particle_system.h`

I implemented the fire particle behavior used for the volcano scene. (referenced from [LearnOpenGL-Particles](https://learnopengl.com/In-Practice/2D-Game/Particles))

Key implementation details:

- Samples random vertices from `Model::subMeshes` in `FireParticleSystem::respawnParticle()`.
- Converts local-space vertex positions into world space using the current model matrix.
- Converts local-space normals into world-space normals using the inverse transpose of the model matrix.
- Spawns particles slightly above model surfaces to avoid z-fighting.
- Uses curl-noise-based motion through `computeCurlNoise()` for turbulent fire movement. (referenced from [Curl-Noise, SIGGRAPH2007](https://www.cs.ubc.ca/~rbridson/docs/bridson-siggraph2007-curlnoise.pdf))
- Guides particle motion with the sampled surface normal inside `FireParticleSystem::Update()` so the fire follows model surfaces instead of only emitting from a point.
- Renders particles as camera-facing billboard quads in `FireParticleSystem::Draw()`.
- Uses additive blending and depth-mask control for a glowing semi-transparent fire effect.

This makes the fire appear to come from the actual surface of animated 3D models.
It also makes the effect reusable for any loaded model because the emitter depends on mesh vertices, normals, and the model matrix instead of hard-coded emitter positions.

### 1.2. Meteor and Moving-Object Fire Effects

**Main file:** `project/src/1-volcano/particle_system.h`

I also implemented `MeteorParticleSystem` for fiery trails and surface fire on moving scene objects.

Implemented behavior includes:

- Trail emission behind a moving head position using `MeteorParticleSystem::EmitTrail()`.
- Surface-based emission through `MeteorParticleSystem::EmitSurfaceFire()`, reusing transformed model vertices and normals.
- Randomized spread, lifetime, size, and color values in `respawnParticle()` and `respawnSurfaceParticle()` for a less uniform fire look.
- Velocity damping and alpha fading during `MeteorParticleSystem::Update()`.
- Billboard rendering with per-particle size control in `MeteorParticleSystem::Draw()`.

The same system is used for the dragon, falling meteors, and falling airplanes in the volcano scene.
This made the fire effect scalable across multiple moving object types instead of being limited to one custom particle emitter.

## 2. Artistic Consideration

### 2.1. Base Scene Composition

**Main file:** `project/src/0-base/scene.cpp`

I implemented the base indoor scene that acts as the shared starting space for the project.

Main implementation details:

- Loaded and configured the base-scene models and textures, including the warehouse/house, fire extinguisher, sofa, table, tiled grass ground, and skybox.
- Arranged the indoor props with manually tuned position, rotation, and scale values so the scene reads as a composed room rather than a random asset collection.
- Added directional lighting, normal/specular/diffuse texture binding, shadow-map rendering, PCF shadow usage, and skybox rendering. (almost same as HW2, changed in model loading and texture binding implementation)
- Exposed the scene through `base::getModule()`, `renderFrame()`, `renderFadeForeground()`, `getCameraPose()`, and `setCameraPose()` so it can be used by the combined multi-scene application.

This scene provides the visual anchor used before transitioning into the elemental scenes.
It also keeps the base scene reusable instead of being tied to a separate standalone `main()` only.

### 2.2. Volcano Scene

**Main files:**

- `project/src/1-volcano/scene.cpp`
- `project/src/1-volcano/volcano_trajectory.h`
- `project/src/1-volcano/volcano_trajectory.cpp`

I integrated the fire systems into a dynamic volcano-themed scene.

Implemented scene elements include:

- A fire skybox using the `project/resources/1-volcano/fireskybox` cubemap assets.
- Toothless and dragon models animated through custom flight trajectories.
- A separate trajectory module for flight-path calculation, including `getToothlessFlightModelMatrix()`, `getDragonFlightPose()`, `getDragonFlightVelocity()`, and `getDragonFlightModelMatrix()`.
- Model orientation is calculated from tangent vectors, banking, offsets, and scale corrections.
- Surface fire emitted from the animated Toothless and dragon model matrices.
- Randomized falling meteors using `Meteor`, `getMeteorModelMatrix()`, spawn position, velocity, rotation axis, rotation speed, and scale.
- Randomized falling airplanes using `Airplane`, `getAirplaneModelMatrix()`, spawn position, velocity, rotation, angular velocity, and per-model scale.
- Object pooling for meteors and airplanes by reactivating hidden entities instead of allocating new ones every frame.
- Per-frame model-matrix updates before surface fire emission so particles match each object's current transform.
- F/G keyboard controls for increasing or decreasing the Toothless fire spawn rate.

The volcano scene demonstrates the fire-guide feature on both static scene composition and actively moving objects.
It also shows that the particle logic is connected to the animation state every frame, so fire trails remain attached to moving and rotating models.


### 2.3. Artistic Composition and Tuning in Volcano Scene

I tuned the presentation of the base and volcano sections:

- Used the indoor house space as the repeated visual reference point across scene transitions.
- Used Toothless and the dragon as the main volcano focal objects.
- Used meteors and airplanes as secondary moving fire elements.
- Tuned particle count, size, lifetime, spread, and intensity for readability.
- Avoided applying the same fire density everywhere so the scene would not become visually noisy.
- Combined the fire skybox, animated flight paths, falling objects, and surface fire trails to create a stronger sense of heat, danger, and motion.
