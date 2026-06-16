# Parts Implemented by Kanghyeon Cho

## 1. Technical contents

### 1.1. Skeletal Animation for Underwater Fish

**Main files:**

- `project/src/3-underwater/animation/model_animation.h`
- `project/src/3-underwater/animation/animation.h`
- `project/src/3-underwater/animation/animator.h`
- `project/src/3-underwater/animation/bone.h`
- `project/shaders/3-underwater/shader_lighting.vs`

I implemented skeletal animation support for the animated fish models in the underwater scene. (based on [LearnOpenGL - Skeletal Animation](https://learnopengl.com/Guest-Articles/2020/Skeletal-Animation))

Key implementation details:

- Created `AnimationModel` as an animated extension of the shared `Model` class.
- Extracted bone IDs, bone weights, and bone offset matrices from Assimp mesh data.
- Loaded animation channels, copied the node hierarchy, interpolated keyframes, and recursively calculated final bone matrices.
- Sent `finalBonesMatrices` to the underwater lighting shader every frame and updated bass/shark animations with different time scales.
- Estimated each animated model's radius and length from mesh bounds so the fish size can also be used by the boid system.

This made the fish move with actual skeletal animation while still allowing the same animated model data to be reused across many fish entities.

### 1.2. Boid-Based Fish Movement

**Main files:**

- `project/src/3-underwater/animation/boid.h`
- `project/src/3-underwater/scene.cpp`

I implemented fish movement using a Boids-style flocking algorithm.

Implemented behavior includes:

- Separation, alignment, and cohesion for group movement.
- Wander force so the motion does not look too mechanical.
- Boundary steering and reflection so fish stay inside a fixed underwater volume.
- Speed/force limits and smooth forward-vector updates for stable orientation.
- Initial placement checks and approximate spherical collision handling to reduce fish overlap.

The scene uses separate bass and shark groups, but all boids are still checked for collision and overlap.
This keeps many fish moving together inside the camera-readable underwater area without frequent visual intersections.

### 1.3. Caustics with Periodic Texture Frames

**Main files:**

- `project/src/shared/texture.h` (`CausticTexture` class)
- `project/shaders/3-underwater/shader_lighting.fs`

I implemented underwater caustics using a periodic texture-frame animation.

Key implementation details:

- Loaded 32 grayscale caustic images into a `GL_TEXTURE_2D_ARRAY` in `CausticTexture`.
- Animated caustics in the fragment shader with `mod(currentTime * causticFrameRate, causticFrameCount)`.
- Interpolated adjacent frames and applied the result with world-space XZ projection, normal lighting, and distance falloff.

This creates a repeating animated light pattern that reads as underwater caustic shimmer without requiring dynamic water simulation.

## 2. Artistic Consideration

### 2.1. Underwater Color and Fog Mood

**Main files:**

- `project/shaders/3-underwater/shader_lighting.fs`

I tuned the underwater scene so objects feel submerged rather than simply placed in front of a blue background.

Artistic decisions:

- Chose a blue-green base water color.
- Applied exponential absorption and distance-based fog so far objects become more water-tinted.
- Kept the fog blend limited so nearby props and fish remain readable.
- Excluded the house box from fog so the indoor foreground remains visually stable.

This gives the scene depth and underwater atmosphere without hiding the main objects.

### 2.2. Underwater Scene Composition

**Main file:** `project/src/3-underwater/scene.cpp`

I added and arranged multiple object types to make the underwater scene feel richer.

Scene composition work:

- Added animated bass and shark groups as the main moving elements.
- Added seashells, pebbles, a wooden boat, and mountain/floor meshes as supporting environmental objects.
- Tuned object position, rotation, and scale values manually so the props read as an underwater environment instead of isolated assets.
- Reused the shared house, fire extinguisher, sofa, and table assets so the underwater scene connects visually to the base scene.
- Kept fish movement concentrated inside a fixed 3D region so the scene looks active rather than sparse.

These choices make the scene denser and more understandable from the camera's point of view.

### 2.3. Multi-Scene Module Integration

**Main files:**

- `project/src/main.cpp`
- `project/src/shared/scene_module.h`
- `project/src/shared/scene_transition_effect.h`
- `project/src/shared/scene_transition_effect.cpp`

I also helped restructure the project so every scene can be loaded and rendered from one shared application entry point.

Main implementation details:

- Wrapped each scene file in its own namespace: `base`, `volcano`, `desert`, and `underwater`.
- Exposed each scene through a `SceneModule` returned by `getModule()`.
- Built a unified `project/src/main.cpp` that initializes all scene modules once and toggles them with the Tab key.
- Preserved relative camera pose between scenes and added a black fade transition with `scene_transition_effect`.
- Rendered house-related foreground entities after the active scene so the shared house remains visually stable across scene transitions.

This made the four scenes work as one combined application without merging all scene-specific rendering code into a single large scene file.
