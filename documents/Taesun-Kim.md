## Personal Contribution by Taesun Kim

**Main file for 1~4:** `shaders/2-desert/shader_ray_tracing.fs`

### 1. Mirage Renderer with Non-linear Ray Marching

I implemented the main mirage rendering feature for the desert scene using non-linear ray marching.

Key implementation details:

- Generated camera rays in shader space using `cameraPosition`, `cameraToWorldRotMatrix`, screen size, and field of view.
- Advanced each ray with a small fixed step size instead of using a single straight-line intersection test.
- Updated the ray direction at every step according to the current temperature and refractive index field.
- Checked hits against the ground and pyramids during the marching process.
- Returned skybox color when the ray escaped into the upper sky region.

This made it possible to show a mirage-like image because the light path is gradually curved by the hot air layer above the desert surface.


### 2. Temperature Field for the Desert Air

I designed the temperature field used to control the refractive behavior of the air.

Main components:

- XZ-plane elliptical heat area centered near the distant pyramid region.
- Exponential height decay so the strongest heat remains close to the sand.
- Large-scale drifting noise for slow movement of warm air patches.
- Small-scale temporal noise for boiling heat shimmer.
- Temperature-to-IOR conversion through `getIOR`.
- Finite-difference sampling in `getIORGradient` to estimate how the refractive index changes in space.

With this setup, the optical distortion is tied to both position and time, which makes the desert air feel dynamic.


### 3. Eikonal-inspired Ray Direction Update

I implemented an approximate refraction model based on the gradient-index ray equation.

Main steps:

- Samples the local refractive index at the current ray position.
- Computes the IOR gradient from nearby temperature samples.
- Removes the gradient component parallel to the ray direction.
- Uses the perpendicular gradient as the bending force.
- Clamps extreme gradient values to keep the march stable.
- Updates and normalizes the ray direction after applying step size, IOR, bend strength, and mask value.

This is not a full physical optics simulation, but it gives a real-time approximation that visually matches the behavior of a desert mirage.


### 4. Mirage Masking and Analytic Scene Hits

I added spatial limits and analytic hit tests so the effect would remain controlled and readable.

Main elements:

- Height mask that restricts bending to a thin layer above the ground.
- Depth mask that activates the mirage mainly in the middle-to-far desert range.
- Ground contact test using the fixed `groundHeight` value.
- Pyramid hit test based on local coordinates and face boundaries.
- Pyramid normal and UV calculation for textured shading.
- Separate shading results for ground and pyramid surfaces.

These restrictions were important because a mirage should feel attached to the hot surface. If the whole image bends equally, the result looks like a generic camera filter rather than hot desert air.


### 5. Heat Haze Post-processing Pass

**Primary source:** `shaders/2-desert/shader_heat_haze.fs`

To make the heat more visible, I implemented an additional screen-space haze pass.

Main work in this shader:

- Samples the rendered desert scene from an off-screen texture.
- Distorts UV coordinates before reading the scene color.
- Uses Perlin noise and fBM to create irregular heat motion.
- Mixes macro distortion, micro shimmer, and vertical wave movement.
- Applies heat-band and approximate-depth masks to localize the haze.
- Excludes the temperature UI area so the overlay stays clear.

This pass strengthens the perception of unstable hot air while still relying on the ray-marched scene as the underlying image.


### 6. Temperature Animation, Controls, and Rendering Flow

**Primary source:** `src/2-desert/scene.cpp`

I connected the shader features to the desert scene runtime and added temperature-driven interaction.

Scene-side work:

- Animated ground temperature in the range from 20 to 210 degrees.
- Added up/down arrow input to adjust the temperature during execution.
- Sent `groundTemp`, `skyTemp`, time, camera data, and haze amount to the shaders every frame.
- Rendered the ray-marched desert scene into a separate framebuffer.
- Applied the heat haze shader as a full-screen post pass.
- Drew the temperature overlay in a separate `overlayOnly` pass after the main scene.

This made the mirage respond to the current thermal state instead of staying visually constant.


### 7. Artistic Tuning

I tuned the final scene so the effect would be understandable to viewers, not only technically present.

Artistic decisions:

- Placed the strongest heat influence around the distant pyramids to create a clear focal area.
- Kept the bend layer low so the distortion appears to rise from the sand.
- Balanced ray bending with screen-space haze to avoid excessive visual noise.
- Used time-varying macro and micro motion so the air does not look frozen.
- Combined desert skybox, sand textures, pyramid placement, and warm color choices to support the illusion of heat.
- Kept the temperature bar simple so it explains the thermal state without taking attention away from the scene.

The final result presents the mirage as both a technical refraction effect and an artistic desert atmosphere element.