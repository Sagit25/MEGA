#version 330
out vec4 FragColor;

in vec2 TexCoords;

// You can change the code whatever you want
#define PI 3.14159265359

const int MAX_DEPTH = 10; // maximum bounce
vec3 colorStack[MAX_DEPTH];

// Optional for Figure 1(h): accumulation support.
uniform sampler2D accumPrev;
uniform int frameCountWithoutMove;
uniform bool displayOnly;

// [Project] Texture
uniform samplerCube skybox;
uniform sampler2D objectTexture;
uniform sampler2D rockTexture;
uniform sampler2D groundTexture;


uniform float time;

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Material {
    int material_type; // 0: diffusion, 1: reflect, 2: refractive

    vec3 albedo;

    // parameters for reflective
    float fuzz; // fuzziness for reflective

    // parameters for refractive
    float ior; // index of refraction
};

const int mat_diffuse = 0;
const int mat_reflective = 1;
const int mat_refractive = 2;

// Just consider Light as a fixed environment light

// hit information
struct HitRecord {
    float t;        // distance to hit point
    vec3 p;         // hit point
    vec3 normal;    // hit point normal
    bool frontFace; // whether the ray hits the front face
    Material mat;   // hit point material
};

// Geometry
struct Sphere {
    vec3 center;
    float radius;
    Material mat;
};

struct Pyramid {
    vec3 center;
    float size;
    Material mat;
};

uniform Material material_ground;
uniform Material material_sphere_middle;
uniform Material material_sphere_left;
uniform Material material_sphere_right;
uniform Material material_inside_left;

Sphere spheres[] = Sphere[](
    // Sphere(vec3(0, -100.5, -1), 100, material_ground),
    Sphere(vec3(6.0, 13.0, -20), 0.9, material_sphere_middle),
    Sphere(vec3(2.0, 12.0, -20), 0.6, material_sphere_middle),
    Sphere(vec3(-4.5, 12.0, -20), 0.8, material_sphere_middle)
    // Sphere(vec3(3, 1.5, -9), 1.0, material_sphere_middle)
    // Sphere(vec3(-1, 0.5, -10), 0.4, material_inside_left)
);

Pyramid pyramids[] = Pyramid[](
    Pyramid(vec3(0.0, 16.0, -25.0), 1.5, material_sphere_middle),
    Pyramid(vec3(-10.0, 12.0, -18.0), 0.9, material_sphere_middle),
    Pyramid(vec3(11.5, 13.0, -20.0), 1.2, material_sphere_middle)
);

//float rand(vec2 co) {
//  return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
//}


uniform vec3 cameraPosition;
uniform mat3 cameraToWorldRotMatrix;
uniform float fovY; //set to 45
uniform float H;
uniform float W;

// Temperature
uniform float groundTemp;
uniform float skyTemp;   

const float groundHeight = -0.5;
const float skyHeight = 50.0;

Ray getRay(vec2 uv){
    // TODO

    vec2 uv_ndc = 2.0 * uv - 1.0;
    float tan_half = tan(fovY * 0.5);

    float dirX = uv_ndc.x * tan_half * W / H;
    float dirY = uv_ndc.y * tan_half;
    vec3 rayDir = vec3(dirX, dirY, -1.0);
    rayDir = cameraToWorldRotMatrix * rayDir;

    Ray r;
    r.origin = cameraPosition;
    r.direction = rayDir;
    return r;
}

const float bias = 0.0001; // to prevent point too close to surface.

bool sphereIntersect(Sphere sp, Ray r, inout HitRecord hit){
    // TODO
    vec3 oc = sp.center - r.origin;
    float a = dot(r.direction, r.direction);
    float b = -2.0 * dot(r.direction, oc);
    float c = dot(oc, oc) - sp.radius * sp.radius;
    float disc = b*b - 4*a*c;

    if (disc < 0) {
        return false;
    }

    float sqrtD = sqrt(disc);
    float sol = (-b - sqrtD) / (a * 2.0);

    if (sol < bias || sol > hit.t) {
        sol = (-b + sqrtD) / (a * 2.0);

        if(sol < bias || sol > hit.t){
            return false;
        }
    }

    hit.t = sol;
    hit.p = r.origin + sol * r.direction;

    vec3 n = normalize(hit.p - sp.center);

    hit.frontFace = dot(r.direction, n) < 0.0;
    hit.normal = hit.frontFace ? n : (-n);

    hit.mat = sp.mat;

    return true;
}

mat3 rotX(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat3(
        1.0, 0.0, 0.0,
        0.0, c, -s,
        0.0, s, c
    );
}

const float pyramidRotX = 40.0;

bool checkDownwardPyramid(vec3 p, Pyramid pyr, out vec3 normal) {
    vec3 localP = p - pyr.center;

    mat3 rX = rotX(radians(pyramidRotX));

    localP = rX * localP;

    vec3 n0 = vec3(0.0, 1.0, 0.0);
    vec3 n1 = normalize(vec3(1.0, -1.0, 0.0));
    vec3 n2 = normalize(vec3(-1.0, -1.0, 0.0));
    vec3 n3 = normalize(vec3(0.0, -1.0, 1.0));
    vec3 n4 = normalize(vec3(0.0, -1.0, -1.0));

    float d0 = dot(localP, n0) - pyr.size;
    float d1 = dot(localP, n1) - pyr.size;
    float d2 = dot(localP, n2) - pyr.size;
    float d3 = dot(localP, n3) - pyr.size;
    float d4 = dot(localP, n4) - pyr.size;

    float maxD = max(d0, max(max(d1, d2), max(d3, d4)));

    if (maxD <= 0.0) {
        vec3 localNormal;
        if (maxD == d0) localNormal = n0;
        else if (maxD == d1) localNormal = n1;
        else if (maxD == d2) localNormal = n2;
        else if (maxD == d3) localNormal = n3;
        else localNormal = n4;

        normal = transpose(rX) * localNormal;
        return true;
    }
    return false;
}


const float texScale = 0.2; // Texture size scaling

bool checkHitVolume(vec3 p, out Material hitMat, out vec3 hitNormal, out vec2 hitUV, out int hitObjType) {
    // Ground
    if (p.y <= groundHeight) {
        hitMat = material_ground;
        hitNormal = vec3(0.0, 1.0, 0.0);
        hitUV = fract(p.xz * texScale);
        hitObjType = 0;
        return true;
    }

    // Spheres
    for (int i = 0; i < spheres.length(); i++) {
        if (length(p - spheres[i].center) <= spheres[i].radius) {
            hitMat = spheres[i].mat;
            hitNormal = normalize(p - spheres[i].center);

            float baseU = 0.5 + atan(hitNormal.z, hitNormal.x) / (2.0 * PI);
            float baseV = 0.5 - asin(hitNormal.y) / PI;

            float sphereTexScale = 2.0;

            hitUV.x = baseU * (2.0 * sphereTexScale);
            hitUV.y = baseV * sphereTexScale;

            hitUV = fract(hitUV);

            hitObjType = 2;
            return true;
        }
    }


    // Added: Tetrahedrons!
    //for (int i = 0; i < tetras.length(); i++) {
    //    vec3 tetNormal;
    //    if (checkTetra(p, tetras[i], tetNormal)) {
    //        hitMat = tetras[i].mat;
    //        hitNormal = tetNormal;
    //        hitUV = vec2(0.5); // default value for now
    //        hitObjType = 1;
    //        return true;
    //    }
    //}

    // Pyramid
    for (int i = 0; i < pyramids.length(); i++) {
        vec3 pyrNormal;
        if (checkDownwardPyramid(p, pyramids[i], pyrNormal)) {
            hitMat = pyramids[i].mat;
            hitNormal = pyrNormal;

            vec3 localP = p - pyramids[i].center;

            mat3 rX = rotX(radians(pyramidRotX));
            vec3 rotatedLocalP = rX * localP;
            vec3 localNormal = rX * pyrNormal;

            float objTexScale = 0.8;
            float rootTwo = 1.4142;

            hitUV = vec2(1.0);
            if (localNormal.y > 0.5) {
                // Base face: normal is y-axis -> XZ plane
                hitUV.x = rotatedLocalP.x * objTexScale;
                hitUV.y = rotatedLocalP.z * objTexScale;
            }
            else if (abs(localNormal.x) > abs(localNormal.z)) {
                // X-sidal face -> ZY plane
                hitUV.x = rotatedLocalP.z * sign(localNormal.x) * objTexScale;
                hitUV.y = rotatedLocalP.y * rootTwo * objTexScale;
            }
            else {
                // Z-sidal face -> XY plane
                hitUV.x = rotatedLocalP.x * sign(localNormal.z) * -1.0 * objTexScale;
                hitUV.y = rotatedLocalP.y * rootTwo * objTexScale;
            }
            hitUV = fract(hitUV);

            hitObjType = 1;
            return true;
        }
    }

    return false;
}



// Project: Non-linear Ray Tracing
//// simple checker pattern using position vector p
//vec3 getProceduralColor(vec3 p, vec3 baseColor) {
//    float scale = 2.0;
//    // checker pattern with x-z coord
//    float pattern = mod(floor(p.x * scale) + floor(p.z * scale), 2.0);
//    return pattern == 0.0 ? baseColor : baseColor * 0.5; // cross colors darkly(?)
//}


//// Temperature V0: Basic
//float getTemperature(vec3 p) {
//    float height = p.y - groundHeight;
//    if (height < 0.0) height = 0.0;
//    
//    // Exponential temperature function
//    float decay = exp(-3.0 * height); 
//
//    //// Linear temp function (BAD)
//    //float decay = (skyHeight - height) / (skyHeight - groundHeight);
//
//    return skyTemp + (groundTemp - skyTemp) * decay;
//}

//// Temperature V1: Cylindrical Hot Area
//float getTemperature(vec3 p) {
//    float height = p.y - groundHeight;
//    if (height < 0.0) height = 0.0;
//
//    float roadWidth = 5.0; 
//    float localHeatFactor = exp(-(p.x * p.x) / (roadWidth * roadWidth));
//
//    float depthFactor = smoothstep(-2.0, -15.0, p.z);
//    localHeatFactor *= depthFactor;
//
//    float currentGroundTemp = skyTemp + (groundTemp - skyTemp) * localHeatFactor;
//    
//    float decay = exp(-3.0 * height);
//
//    return skyTemp + (currentGroundTemp - skyTemp) * decay;
//}


// // Temperature V2: Road Model (ground line remains flat)
//float getTemperature(vec3 p) {
//    float height = p.y - groundHeight;
//    if (height < 0.0) height = 0.0;
//
//    // Road model: cools only along the X-axis (left-right),
//    // while the heat extends far along the Z-axis (forward-backward).
//    float roadWidth = 15.0; // Use a very wide road to avoid horizontal lens distortion.
//    float localHeatFactor = exp(-(p.x * p.x) / (roadWidth * roadWidth));
//
//    // (Optional) Z-axis attenuation:
//    // The area near the camera (z = 0) remains normal,
//    // while regions farther away become hotter.
//    // This helps reinforce the fact that mirages appear on distant ground.
//    float depthFactor = smoothstep(0.0, -10.0, p.z);
//    localHeatFactor *= depthFactor;
//
//    // Compute the temperature within the road region.
//    float currentGroundTemp = skyTemp + (groundTemp - skyTemp) * localHeatFactor;
//
//    // Prevent excessive vertical compression:
//    // Increase the thickness of the heat layer by making the decay more gradual.
//    float decay = exp(-1.2 * height); // Changed from -3.0 to -1.2
//
//    // Heat-haze noise (the shimmering effect emphasized in the paper).
//    // Adjust 15.0 (frequency) and 0.1 (amplitude)
//    // to control the size and intensity of the shimmer.
//    float noise = sin(p.x * 15.0) * cos(p.z * 15.0) * 0.1;
//    float turbulence = noise * decay;
//
//    return skyTemp + (currentGroundTemp - skyTemp) * (decay + turbulence);
//}


// Temp V3: Dynamic Scene
float getTemperature(vec3 p) {
    // ==========================================================
    // Spatial Shape Configuration
    float ROAD_WIDTH = 15.0;             // Width of the hot region (larger values create a wider flat area)
    float DECAY_RATE = 1.2;              // Cooling rate with altitude (smaller values produce a thicker heat layer,
    // making distorted objects appear less vertically compressed)
    float FADE_START_Z = 0.0;            // Z position where distant heating begins
    float FADE_END_Z = -10.0;            // Z position where maximum heating is reached

    // Macro Noise (large, slowly drifting heat masses)
    vec2  MACRO_FREQ = vec2(0.8, 0.6);   // Spatial fluctuation frequency (x, z)
    vec2  MACRO_SPEED = vec2(2.0, -1.5); // Temporal movement speed
    float MACRO_AMP = 0.02;               // Distortion strength of heat patches

    // Micro Noise (fine, boiling heat shimmer)
    vec2  MICRO_FREQ = vec2(15.0, 15.0); // Spatial shimmer frequency
    vec2  MICRO_SPEED = vec2(-8.0, 6.0); // Temporal boiling speed
    float MICRO_AMP = 0.01;               // Shimmer amplitude (distortion strength)
    // ==========================================================

    float height = p.y - groundHeight;
    if (height < 0.0) height = 0.0;

    // Generate macro noise (irregular heat patches)
    float macroNoise =
        sin(p.x * MACRO_FREQ.x + time * MACRO_SPEED.x) *
        cos(p.z * MACRO_FREQ.y + time * MACRO_SPEED.y);

    // Compute the road's base heat profile and depth-dependent heating
    float baseShape = exp(-(p.x * p.x) / (ROAD_WIDTH * ROAD_WIDTH));
    float localHeatFactor =
        baseShape * ((1.0 - MACRO_AMP) + MACRO_AMP * macroNoise);

    float depthFactor = smoothstep(FADE_START_Z, FADE_END_Z, p.z);
    localHeatFactor *= depthFactor;

    // Final ground temperature at the current (x, z) location
    float currentGroundTemp =
        skyTemp + (groundTemp - skyTemp) * localHeatFactor;

    // Temperature decay with altitude
    float decay = exp(-DECAY_RATE * height);

    // Generate micro noise (temperature-dependent shimmer)
    // Compute heat intensity as a 0–1 ratio relative to the maximum
    // expected ground temperature difference (e.g. 180°C)
    float heatRatio =
        clamp((currentGroundTemp - skyTemp) / 180.0, 0.0, 1.0);

    float microNoise =
        sin(p.x * MICRO_FREQ.x + time * MICRO_SPEED.x) *
        cos(p.z * MICRO_FREQ.y + time * MICRO_SPEED.y);

    float turbulence =
        microNoise * MICRO_AMP * heatRatio * decay;

    return skyTemp +
        (currentGroundTemp - skyTemp) * (decay + turbulence);
}



// IOR(Index of Refraction) of air by temperature
float getIOR(float T) {
    float Pa = 101325.0; // Air pressure
    float c1 = 0.0000104;
    float c2 = 0.00366;
    float num = c1 * Pa * (1.0 + Pa * (60.1 - 0.972 * T) * 1e-10);
    float den = 1.0 + c2 * T;
    return 1.0 + (num / den); // refractive index of air
}

// IOR Gradient function
vec3 getIORGradient(vec3 p) {
    float eps = 0.01;
    float n_x = getIOR(getTemperature(p + vec3(eps, 0, 0))) - getIOR(getTemperature(p - vec3(eps, 0, 0)));
    float n_y = getIOR(getTemperature(p + vec3(0, eps, 0))) - getIOR(getTemperature(p - vec3(0, eps, 0)));
    float n_z = getIOR(getTemperature(p + vec3(0, 0, eps))) - getIOR(getTemperature(p - vec3(0, 0, eps)));
    return vec3(n_x, n_y, n_z) / (2.0 * eps);
}

// Major function: Non-linear ray marching 
vec3 rayMarch(Ray ray) {
    float stepSize = 0.05;
    int MAX_STEPS = 800;

    vec3 currentPos = ray.origin;
    vec3 currentDir = normalize(ray.direction);

    for (int i = 0; i < MAX_STEPS; i++) {

        Material hitMat;
        vec3 hitNormal;
        vec2 hitUV;
        int hitObjType;

        // Replaced trace() with checkHitVolume()
        if (checkHitVolume(currentPos, hitMat, hitNormal, hitUV, hitObjType)) {

            vec3 texColor = vec3(0.0);
            vec2 uv = vec2(0.0);

            if (hitObjType == 0) { // ground
                texColor = texture(groundTexture, hitUV).rgb;
            }
            else if (hitObjType == 1) { // object(Pyramid)
                texColor = texture(objectTexture, hitUV).rgb;
            }
            else if (hitObjType == 2) { // object(Sphere)
                texColor = texture(rockTexture, hitUV).rgb;
            }

            vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
            float diff = max(dot(hitNormal, lightDir), 0.2);

            return texColor * diff;
        }

        if (currentPos.y > skyHeight) {
            //return texture(skybox, currentDir).rgb;
            
            bool isMirage = (ray.direction.y < 0.0 && currentDir.y > 0.0);

            if (isMirage) {
                return texture(skybox, currentDir).rgb;
            }
            else {
                //return texture(skybox, ray.direction).rgb;
                return texture(skybox, currentDir).rgb;
            }
        }

        float currentIOR = getIOR(getTemperature(currentPos));
        vec3 gradient = getIORGradient(currentPos);

        // --- Core: Mirage amplification & application of the correct light-bending formula ---
        // Artificially amplify the refraction effect by 10x–20x to make the result visible in a confined space
        float mirageStrength = 2.0;


        // Eikonal equation approximation: only the gradient component perpendicular
        // to the propagation direction causes the light ray to bend.
        vec3 perpGrad = gradient - currentDir * dot(currentDir, gradient);
        currentDir = currentDir + (perpGrad / currentIOR) * stepSize * mirageStrength;
        currentDir = normalize(currentDir);

        currentPos += currentDir * stepSize;
    }

    //return texture(skybox, ray.direction).rgb;
    return texture(skybox, currentDir).rgb;
}


void main()
{
    // 1. Pass for screen output (Same as origin)
    if (displayOnly) {
        FragColor = texture(accumPrev, TexCoords);
        return;
    }

    vec3 color = vec3(0);

    // 2. initial ray for current TexCoords
    Ray r = getRay(TexCoords);

    // 3. nonlinear ray tracking
    color = rayMarch(r);

    // 4. gamma correction
    float gamma = 2.2;
    color = pow(color, vec3(1.0 / gamma));

    // // 5. Accumulation 
    // // denoise when altering temperature
    vec3 prevColor = texture(accumPrev, TexCoords).rgb;
    float maxAccumulationFrames = 100.0;
    float w = 1.0 / min(float(frameCountWithoutMove + 1.0), maxAccumulationFrames);
    color = (1.0 - w) * prevColor + w * color;

    FragColor = vec4(color, 1.0);
}
