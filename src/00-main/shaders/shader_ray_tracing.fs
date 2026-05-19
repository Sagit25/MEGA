#version 330
out vec4 FragColor;

in vec2 TexCoords;

// You can change the code whatever you want

const int MAX_DEPTH = 20; // maximum bounce
vec3 colorStack[MAX_DEPTH];

// Optional for Figure 1(h): accumulation support.
uniform sampler2D accumPrev;
uniform int frameCountWithoutMove;
uniform bool displayOnly;

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

uniform Material material_ground;
uniform Material material_sphere_middle;
uniform Material material_sphere_left;
uniform Material material_sphere_right;
uniform Material material_inside_left;


Sphere spheres[] = Sphere[](
    Sphere(vec3(0,-100.5,-1), 100, material_ground),
    Sphere(vec3(0,0,-1), 0.5, material_sphere_middle),
    Sphere(vec3(-1.01,0,-1), 0.5, material_sphere_left),
    Sphere(vec3(1,0,-1), 0.5, material_sphere_right),
    Sphere(vec3(-1,0, -1), 0.4, material_inside_left)
);


// Math functions
/* returns a varying number between 0 and 1 */
float rand(vec2 co) {
  return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

float max3 (vec3 v) {
  return max (max (v.x, v.y), v.z);
}

float min3 (vec3 v) {
  return min (min (v.x, v.y), v.z);
}


uniform vec3 cameraPosition;
uniform mat3 cameraToWorldRotMatrix;
uniform float fovY; //set to 45
uniform float H;
uniform float W;

Ray getRay(vec2 uv){
    Ray ray;

    // set ray origin to camera position
    ray.origin = cameraPosition;

    // find ray direction in camera local space and set ray direction by multiplying rotMat
    vec3 dir = vec3(2.0 * uv.x - 1.0, 2.0 * uv.y - 1.0, -1.0);
    dir.x *= (W / H) * tan(fovY / 2.0);
    dir.y *= tan(fovY / 2.0);
    ray.direction = normalize(cameraToWorldRotMatrix * dir);

    return ray;
}

const float bias = 0.0001; // to prevent point too close to surface.

bool sphereIntersect(Sphere sp, Ray r, inout HitRecord hit){
    // referenced this part from Ray Tracing in One Weekend

    // find the discriminant of the quadratic equation
    vec3 oc = sp.center - r.origin;
    float a = dot(r.direction, r.direction);
    float b = -2.0 * dot(oc, r.direction);
    float c = dot(oc, oc) - sp.radius * sp.radius;
    float discriminant = b * b - 4.0 * a * c;

    // find the nearest hit point and update hit record
    if (discriminant > 0.0) {
        float root1 = -1.0 * (b + sqrt(discriminant)) / (2.0 * a);
        float root2 = -1.0 * (b - sqrt(discriminant)) / (2.0 * a);
        if (root1 < bias || root1 > hit.t) {
            if (root2 < bias || root2 > hit.t) {
                return false;
            }
            hit.t = root2;
        } 
        else hit.t = root1;
        hit.p = r.origin + hit.t * r.direction;
        hit.normal = normalize((hit.p - sp.center) / sp.radius);
        if (dot(r.direction, hit.normal) > 0.0) {
            hit.normal = -hit.normal;
            hit.frontFace = false;
        } 
        else hit.frontFace = true;
        hit.mat = sp.mat;
        return true;
    } 
    else if (discriminant == 0.0) {
        float root = -1.0 * (b) / (2.0 * a);
        if (root < bias || root > hit.t) {
            return false;
        }
        hit.t = root;
        hit.t = -1.0 * b / (2.0 * a);
        hit.p = r.origin + hit.t * r.direction;
        hit.normal = normalize((hit.p - sp.center) / sp.radius);
        if (dot(r.direction, hit.normal) > 0.0) {
            hit.normal = -hit.normal;
            hit.frontFace = false;
        } 
        else hit.frontFace = true;
        hit.mat = sp.mat;
        return true;
    }
    else return false;
}

float schlick(float cosine, float r0) {
    // implements equation given at Schlick's approximation

    float rTheta = r0;
    rTheta += (1.0 - r0) * pow(1.0 - cosine, 5.0);
    return rTheta;
}

bool trace(Ray r, out HitRecord hit){
    // referenced this part from Ray Tracing in One Weekend
    HitRecord tempRec;
    bool hitAnything = false;
    
    // find the nearest hit point among all spheres and update hit record
    tempRec.t = 1000000.0;
    for (int i = 0; i < spheres.length(); i++) {
        if (sphereIntersect(spheres[i], r, tempRec)) {
            hitAnything = true;
            hit = tempRec;
        }
    }

    return hitAnything;
}

// utilized this function given in homework description file
vec3 skyColor(Ray ray) {
    vec3 dir = normalize(ray.direction);
    float a = 0.5 * (dir.y + 1.0);
    return (1.0 - a) * vec3(1.0) + a * vec3(0.5, 0.7, 1.0);
}

vec3 castRay(Ray ray){
    // referenced this part from Ray Tracing in One Weekend
    
    // reset color stack
    for (int i = 0; i < MAX_DEPTH; i++) {
        colorStack[i] = vec3(1.0);
    }

    // bounce the ray until it hits the sky or reaches max depth
    Ray currentRay = ray;
    HitRecord hit;
    int depth = 0;
    for (int i = 0; i < MAX_DEPTH; i++) {
        if (trace(currentRay, hit)) {
            currentRay.origin = hit.p; // set the new ray origin to hit point
            depth++; // increase depth
            if (hit.mat.material_type == mat_refractive) colorStack[i] = vec3(1.0);
            else colorStack[i] = hit.mat.albedo;
            vec2 seed = hit.p.xy + vec2(frameCountWithoutMove * 2.87, i * 1.35); // set random seed using frame and i
            if (hit.mat.material_type == mat_diffuse) { // diffuse
                vec3 randomUnitDir = normalize(2.0 * vec3(rand(seed), rand(seed + vec2(0.2, 0.3)), rand(seed - vec2(0.2, 0.3))) - vec3(1.0));
                vec3 dir = hit.normal + randomUnitDir;
                currentRay.direction = normalize(dir);
            } 
            else if (hit.mat.material_type == mat_reflective) { // reflective
                vec3 dir = normalize(currentRay.direction);
                vec3 reflectDir = dir - 2.0 * dot(dir, hit.normal) * hit.normal;
                vec3 randomDir = normalize(2.0 * vec3(rand(seed), rand(seed + vec2(0.2, 0.3)), rand(seed - vec2(0.2, 0.3))) - vec3(1.0));
                currentRay.direction = normalize(reflectDir + hit.mat.fuzz * randomDir);
                if (dot(currentRay.direction, hit.normal) <= 0.0) {
                    colorStack[i] = vec3(0.0);
                    break;
                }
            }
            else if (hit.mat.material_type == mat_refractive) { // refractive
                vec3 dir = normalize(currentRay.direction);
                float n = hit.mat.ior;
                if (hit.frontFace) n = 1.0 / hit.mat.ior;
                float cosTheta = min(dot(-dir, hit.normal), 1.0);
                float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
                bool cannotRefract = n * sinTheta > 1.0; // total internal reflection
                float reflectance = schlick(cosTheta, pow((1.0 - n) / (1.0 + n), 2.0)); // schlick's approximation
                if (cannotRefract || reflectance > rand(seed + vec2(0.5, 0.5))) {
                    vec3 dir = normalize(currentRay.direction);
                    vec3 reflectDir = dir - 2.0 * dot(dir, hit.normal) * hit.normal;
                    vec3 randomDir = normalize(2.0 * vec3(rand(seed), rand(seed + vec2(0.2, 0.3)), rand(seed - vec2(0.2, 0.3))) - vec3(1.0));
                    currentRay.direction = normalize(reflectDir + hit.mat.fuzz * randomDir);
                } 
                else {
                    vec3 perpDir = n * (dir + cosTheta * hit.normal);
                    vec3 parallelDir = -sqrt(abs(1.0 - dot(perpDir, perpDir))) * hit.normal;
                    currentRay.direction = normalize(perpDir + parallelDir);
                }
            }
        } 
        else { // hit nothing, return sky color
            colorStack[i] = skyColor(currentRay);
            depth++;
            break;
        }
    }

    if (depth == 0) return vec3(0.0);

    // calculate the final color by multiplying the albedo of each point
    vec3 color = vec3(1.0);
    for (int i = 0; i < depth; i++) {
        color *= colorStack[i];
    }

    return color;
}


void main()
{
    if (displayOnly) {
        // Add gamma correction (gamma = 2.2)
        // FragColor = texture(accumPrev, TexCoords);
        const float gamma = 2.2;
        float colorR = pow(texture(accumPrev, TexCoords).r, 1.0 / gamma);
        float colorG = pow(texture(accumPrev, TexCoords).g, 1.0 / gamma);
        float colorB = pow(texture(accumPrev, TexCoords).b, 1.0 / gamma);
        FragColor = vec4(colorR, colorG, colorB, 1.0);
        return;
    }

    // TODO
    const int nsamples = 10;
    vec3 color = vec3(0);
    for (int i = 0; i < nsamples; i++) {
        vec2 seed = vec2(frameCountWithoutMove * 2.17, i * 1.35); // set random seed using frame and i
        Ray r = getRay(TexCoords + vec2(rand(seed), rand(seed + vec2(0.13, 0.17))) * 0.0001);
        color += castRay(r);
    }
    color /= nsamples;

    // Optional for Figure 1(h)
    // Blend the current-frame color with accumPrev using frameCountWithoutMove.
    if (frameCountWithoutMove > 0) {
        vec3 prevColor = texture(accumPrev, TexCoords).rgb;
        float weight = 1.0 / float(frameCountWithoutMove + 1);
        color = weight * color + (1.0 - weight) * prevColor;
    }

    FragColor = vec4(color, 1.0);
}
