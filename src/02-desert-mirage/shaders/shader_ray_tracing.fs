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

// Geometry
struct Pyramid {
    vec3 center;
    float size;
    Material mat;
};

uniform Material material_ground;
uniform Material material_sphere_middle;

Pyramid pyramids[] = Pyramid[](
    Pyramid(vec3(0.0, -0.5, -24.0), 5.0, material_sphere_middle),
    Pyramid(vec3(-10.0, -0.5, -27.0), 4.0, material_sphere_middle),
    Pyramid(vec3(10.0, -0.5, -29.0), 4.0, material_sphere_middle)
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
const float IOR_BEND_STRENGTH = 1.15;
const float IOR_GRADIENT_LIMIT = 0.8;
const float MIRAGE_DEPTH_START = 7.0;
const float MIRAGE_DEPTH_FULL = 14.0;
const float MIRAGE_DEPTH_END = 28.0;
const float MIRAGE_LAYER_HEIGHT = 1.4;

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

bool checkPyramid(vec3 p, Pyramid pyr, out vec3 normal) {
    vec3 localP = p - pyr.center;
    float halfBase = pyr.size;
    float height = pyr.size * 1.45;

    if (localP.y < 0.0 || localP.y > height) {
        return false;
    }

    float slope = halfBase / height;
    float halfWidth = halfBase * (1.0 - localP.y / height);
    float sideX = halfWidth - abs(localP.x);
    float sideZ = halfWidth - abs(localP.z);

    if (sideX < 0.0 || sideZ < 0.0) {
        return false;
    }

    if (localP.y < min(sideX, sideZ) && localP.y < 0.08) {
        normal = vec3(0.0, -1.0, 0.0);
    }
    else if (sideX < sideZ) {
        normal = normalize(vec3(sign(localP.x), slope, 0.0));
    }
    else {
        normal = normalize(vec3(0.0, slope, sign(localP.z)));
    }

    return true;
}

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

bool checkObjectHitVolume(vec3 p, out Material hitMat, out vec3 hitNormal, out vec2 hitUV, out int hitObjType) {
    for (int i = 0; i < pyramids.length(); i++) {
        vec3 pyrNormal;
        if (checkPyramid(p, pyramids[i], pyrNormal)) {
            hitMat = pyramids[i].mat;
            hitNormal = pyrNormal;

            vec3 localP = p - pyramids[i].center;
            float objTexScale = 0.45;

            hitUV = vec2(1.0);
            if (pyrNormal.y < -0.5) {
                hitUV.x = localP.x * objTexScale;
                hitUV.y = localP.z * objTexScale;
            }
            else if (abs(pyrNormal.x) > abs(pyrNormal.z)) {
                hitUV.x = localP.z * sign(pyrNormal.x) * objTexScale;
                hitUV.y = localP.y * objTexScale;
            }
            else {
                hitUV.x = localP.x * sign(pyrNormal.z) * -1.0 * objTexScale;
                hitUV.y = localP.y * objTexScale;
            }
            hitUV = fract(hitUV);

            hitObjType = 1;
            return true;
        }
    }

    return false;
}

bool checkHitVolume(vec3 p, out Material hitMat, out vec3 hitNormal, out vec2 hitUV, out int hitObjType) {
    if (checkObjectHitVolume(p, hitMat, hitNormal, hitUV, hitObjType)) {
        return true;
    }

    if (p.y <= groundHeight) {
        hitMat = material_ground;
        hitNormal = vec3(0.0, 1.0, 0.0);
        hitUV = fract(p.xz * texScale);
        hitObjType = 0;
        return true;
    }

    return false;
}

vec3 shadeSceneHit(int hitObjType, vec3 hitNormal, vec2 hitUV) {
    vec3 texColor = vec3(0.0);

    if (hitObjType == 0) {
        texColor = texture(groundTexture, hitUV).rgb;
    }
    else if (hitObjType == 1) {
        texColor = mix(texture(objectTexture, hitUV).rgb, vec3(0.95, 0.54, 0.22), 0.12);
    }

    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    float diff = max(dot(hitNormal, lightDir), 0.2);

    return texColor * diff;
}

float rectMask(vec2 p, vec2 minPoint, vec2 size) {
    vec2 inside = step(minPoint, p) * step(p, minPoint + size);
    return inside.x * inside.y;
}

vec3 drawTemperatureUI(vec3 sceneColor) {
    vec2 p = vec2(TexCoords.x * W, (1.0 - TexCoords.y) * H);
    float screenScale = clamp(H / 900.0, 0.85, 1.45);
    float uiScale = screenScale * 0.5;

    vec2 barMin = vec2(46.0, 42.0) * screenScale;
    vec2 barSize = vec2(30.0, 210.0) * uiScale;
    float border = max(1.5, 3.0 * uiScale);
    vec3 frameColor = vec3(0.43, 0.47, 0.51);
    vec3 markerColor = vec3(0.36, 0.39, 0.43);

    float outerMask = rectMask(p, barMin, barSize);
    vec2 innerMin = barMin + vec2(border);
    vec2 innerSize = barSize - vec2(border * 2.0);
    float innerMask = rectMask(p, innerMin, innerSize);

    vec3 color = sceneColor;
    if (outerMask > 0.5) {
        color = frameColor;
    }
    if (innerMask > 0.5) {
        float barT = 1.0 - clamp((p.y - innerMin.y) / innerSize.y, 0.0, 1.0);
        vec3 coldColor = vec3(0.45, 0.63, 0.88);
        vec3 midColor = vec3(0.82, 0.73, 0.83);
        vec3 hotColor = vec3(0.94, 0.48, 0.42);
        color = barT < 0.5
            ? mix(coldColor, midColor, barT * 2.0)
            : mix(midColor, hotColor, (barT - 0.5) * 2.0);
    }

    float tempT = clamp((groundTemp - 20.0) / 190.0, 0.0, 1.0);
    float markerY = innerMin.y + (1.0 - tempT) * innerSize.y - max(1.0, border * 0.65);
    float markerLength = 24.0 * uiScale;
    float markerThickness = max(1.5, 3.0 * uiScale);
    vec2 markerMin = vec2(barMin.x - markerLength, markerY - markerThickness * 0.5);
    vec2 markerSize = vec2(markerLength + border, markerThickness);
    float markerMask = rectMask(p, markerMin, markerSize);

    if (markerMask > 0.5) {
        color = markerColor;
    }

    return color;
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
    float HEAT_CENTER_X = 0.0;
    float HEAT_CENTER_Z = -25.0;
    float HEAT_RADIUS_X = 17.0;
    float HEAT_RADIUS_Z = 16.0;
    float HEAT_EDGE_START = 0.65;
    float DECAY_RATE = 0.7;              // Cooling rate with altitude (smaller values produce a thicker heat layer,
    // making distorted objects appear less vertically compressed)

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

    // Elliptical hot patch on the XZ plane.
    vec2 ellipseCoord = vec2(
        (p.x - HEAT_CENTER_X) / HEAT_RADIUS_X,
        (p.z - HEAT_CENTER_Z) / HEAT_RADIUS_Z
    );
    float ellipseDistance = dot(ellipseCoord, ellipseCoord);
    float baseShape = 1.0 - smoothstep(HEAT_EDGE_START, 1.0, ellipseDistance);
    float localHeatFactor = clamp(
        baseShape * ((1.0 - MACRO_AMP) + MACRO_AMP * macroNoise),
        0.0,
        1.0
    );

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

float getMirageBendMask(vec3 p) {
    float height = max(p.y - groundHeight, 0.0);
    float heightMask = 1.0 - smoothstep(0.15, MIRAGE_LAYER_HEIGHT, height);

    float depth = -p.z;
    float nearMask = smoothstep(MIRAGE_DEPTH_START, MIRAGE_DEPTH_FULL, depth);
    float farMask = 1.0 - smoothstep(MIRAGE_DEPTH_END, MIRAGE_DEPTH_END + 6.0, depth);

    return heightMask * nearMask * farMask;
}

vec3 bendRayDirection(vec3 p, vec3 direction, float stepSize) {
    vec3 dir = normalize(direction);
    float bendMask = getMirageBendMask(p);
    if (bendMask <= 0.0001) {
        return dir;
    }

    float n = max(getIOR(getTemperature(p)), 0.0001);
    vec3 gradN = getIORGradient(p);

    // Gradient-index ray equation:
    // dT/ds = (grad(n) - T * dot(T, grad(n))) / n
    // Only the component perpendicular to the current ray direction bends the ray.
    vec3 perpendicularGrad = gradN - dir * dot(dir, gradN);
    float gradLen = length(perpendicularGrad);
    if (gradLen > IOR_GRADIENT_LIMIT) {
        perpendicularGrad *= IOR_GRADIENT_LIMIT / gradLen;
    }

    return normalize(dir + perpendicularGrad * (stepSize / n) * IOR_BEND_STRENGTH * bendMask);
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

        if (checkHitVolume(currentPos, hitMat, hitNormal, hitUV, hitObjType)) {
            return shadeSceneHit(hitObjType, hitNormal, hitUV);
        }

        if (currentPos.y > skyHeight) {
            return texture(skybox, currentDir).rgb;
        }

        currentDir = bendRayDirection(currentPos, currentDir, stepSize);
        currentPos += currentDir * stepSize;
    }

    return texture(skybox, currentDir).rgb;
}


void main()
{
    // 1. Pass for screen output (Same as origin)
    if (displayOnly) {
        FragColor = vec4(drawTemperatureUI(texture(accumPrev, TexCoords).rgb), 1.0);
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
    color = drawTemperatureUI(color);

    FragColor = vec4(color, 1.0);
}
