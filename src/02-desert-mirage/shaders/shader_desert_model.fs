#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;

uniform sampler2D diffuseTexture;
uniform sampler2D depthMapSampler;
uniform float useDiffuseMap;
uniform float useShadow;
uniform float usePCF;
uniform vec3 baseColor;

struct Light {
    vec3 dir;
    vec3 color;
};

uniform Light light;

float random(vec3 seed, int i)
{
    vec4 seed4 = vec4(seed, i);
    float dotProduct = dot(seed4, vec4(12.9898, 78.233, 45.164, 94.673));
    return fract(sin(dotProduct) * 43758.5453);
}

float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float closestDepth = texture(depthMapSampler, projCoords.xy).r;
    float currentDepth = projCoords.z;
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(-light.dir);
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    float shadow = 0.0;

    if (usePCF > 0.5) {
        vec2 poissonDisk[16] = vec2[](
            vec2(-0.94201624, -0.39906216),
            vec2(0.94558609, -0.76890725),
            vec2(-0.094184101, -0.92938870),
            vec2(0.34495938, 0.29387760),
            vec2(-0.91588581, 0.45771432),
            vec2(-0.81544232, -0.87912464),
            vec2(-0.38277543, 0.27676845),
            vec2(0.97484398, 0.75648379),
            vec2(0.44323325, -0.97511554),
            vec2(0.53742981, -0.47373420),
            vec2(-0.26496911, -0.41893023),
            vec2(0.79197514, 0.19090188),
            vec2(-0.24188840, 0.99706507),
            vec2(-0.81409955, 0.91437590),
            vec2(0.19984126, 0.78641367),
            vec2(0.14383161, -0.14100790)
        );

        for (int i = 0; i < 4; i++) {
            int index = int(16.0 * random(floor(FragPos.xyz * 1000.0), i)) % 16;
            float pcfDepth = texture(depthMapSampler, projCoords.xy + poissonDisk[index] / 700.0).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
        shadow /= 4.0;
    }
    else {
        shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    }

    if (projCoords.z > 1.0) shadow = 0.0;
    return shadow;
}

void main()
{
    vec3 color = baseColor;
    if (useDiffuseMap > 0.5) {
        color = texture(diffuseTexture, TexCoord).rgb;
    }

    vec3 normal = normalize(Normal);
    vec3 ambient = 0.3 * light.color * color;
    float shadow = useShadow > 0.5 ? ShadowCalculation(FragPosLightSpace) : 0.0;
    float diff = max(dot(normal, normalize(-light.dir)), 0.0);
    vec3 diffuse = diff * light.color * color;

    FragColor = vec4(ambient + (1.0 - shadow) * diffuse, 1.0);
}
