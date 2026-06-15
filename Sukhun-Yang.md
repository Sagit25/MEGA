## Parts Implemented by Sukhun Yang

### 1. Main Feature Asset Placement

**Main file:** `src/00-main/src/main.cpp`

I contributed to the visual completion of the intro/main scene by importing and arranging multiple external 3D assets.

Arranged assets include:

- Fire extinguisher
- Barrel
- Cat
- Room / house / warehouse assets
- Sofa and table
- Ground plane and texture setup

The models were placed into the scene with manually adjusted position, rotation, and scale values so that the scene would look visually balanced instead of randomly filled. This work mainly supports the artistic composition of the shared intro scene.

### 2. Fire-Guide Particle System

**Main file:** `src/01-fire-guide/src/particle_system.h`

I implemented the surface-based fire particle behavior in `FireParticleSystem`.

Key implementation details:

- Samples random vertices from `Model::subMeshes`.
- Converts local-space vertex positions into world space using the current model matrix.
- Converts local-space normals into world-space normals using the inverse transpose of the model matrix.
- Spawns particles slightly above the model surface using a normal-direction offset.
- Updates particle motion with curl noise and normal-based velocity correction.
- Renders fire particles as camera-facing billboard quads.
- Uses additive blending and depth-mask control for a glowing semi-transparent fire effect.

This makes the fire appear to be generated from and guided by the actual surface of a 3D model instead of coming from a fixed point emitter.

### 3. Dynamic Volcano Scene

**Main file:** `src/01-fire-guide/src/main.cpp`

I integrated the fire-guide system into a dynamic volcano-themed scene.

Implemented scene elements include:

- Dragon model with surface-based fire particles.
- Falling meteors with randomized spawn position, velocity, rotation axis, rotation speed, and scale.
- Falling airplanes with randomized motion and fire effects.
- Volcano/fire skybox using `fireskybox` resources.
- Per-frame update of dragon, meteor, and airplane model matrices before emitting particles.
- Reuse of meteor and airplane entities by activating inactive objects and deactivating them after they leave the active scene area.

This scene demonstrates that the particle system works not only on static models, but also on moving objects whose transforms change every frame.

### 4. Surface Fire for Moving Objects

**Main file:** `src/01-fire-guide/src/particle_system.h`

I implemented `MeteorParticleSystem::EmitSurfaceFire` to reuse the same surface-based fire idea for moving scene objects.

This function emits particles from the transformed surface of:

- Dragon
- Meteors
- Airplanes

It uses the current model matrix and object velocity so that fire trails follow the animated objects in the volcano scene.

### 5. Artistic Composition and Tuning

I also tuned the visual presentation of the fire scene:

- Used the dragon as the main focal object.
- Used meteors and airplanes as secondary moving fire elements.
- Tuned particle count, size, lifetime, spread, and intensity for readability.
- Avoided applying the same fire density everywhere so that the scene would not become visually noisy.
- Combined volcano skybox, flying objects, and fire trails to create a stronger sense of heat, danger, and motion.
