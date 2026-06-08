#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform vec2 resolution;
uniform float time;
uniform float hazeAmount;
uniform float groundTemp;

const float HAZE_STRENGTH = 5.0;
const float FAR_DEPTH_HAZE = 0.15;
const float DEPTH_HAZE_POWER = 1.35;

vec2 hash22(vec2 p)
{
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float perlinNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float a = dot(hash22(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0));
    float b = dot(hash22(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0));
    float c = dot(hash22(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0));
    float d = dot(hash22(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y) * 0.5 + 0.5;
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    mat2 rotate = mat2(0.8, -0.6, 0.6, 0.8);

    for (int i = 0; i < 4; ++i) {
        value += amplitude * perlinNoise(p);
        p = rotate * p * 2.03 + vec2(17.2, 9.4);
        amplitude *= 0.5;
    }

    return value;
}

float heatBandMask(vec2 uv)
{
    float lowerFade = smoothstep(0.08, 0.28, uv.y);
    float upperFade = 1.0 - smoothstep(0.78, 0.96, uv.y);
    return lowerFade * upperFade;
}

float approximateDepthMask(vec2 uv)
{
    float depth = smoothstep(0.12, 0.88, uv.y);
    depth = pow(depth, DEPTH_HAZE_POWER);
    return mix(1.0, FAR_DEPTH_HAZE, depth);
}

float uiMask(vec2 uv)
{
    vec2 p = vec2(uv.x * resolution.x, (1.0 - uv.y) * resolution.y);
    float screenScale = clamp(resolution.y / 900.0, 0.85, 1.45);
    float uiScale = screenScale;
    vec2 barMin = vec2(74.0, 66.0) * screenScale;
    vec2 barSize = vec2(30.0, 210.0) * uiScale;
    vec2 inside = step(barMin, p) * step(p, barMin + barSize);
    return inside.x * inside.y;
}

float rectMask(vec2 p, vec2 minPoint, vec2 size)
{
    vec2 inside = step(minPoint, p) * step(p, minPoint + size);
    return inside.x * inside.y;
}

vec3 drawTemperatureUI(vec2 uv, vec3 sceneColor)
{
    vec2 p = vec2(uv.x * resolution.x, (1.0 - uv.y) * resolution.y);
    float screenScale = clamp(resolution.y / 900.0, 0.85, 1.45);
    float uiScale = screenScale;

    vec2 barMin = vec2(74.0, 100.0) * screenScale;
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

void main()
{
    vec2 uv = TexCoords;
    float heat = heatBandMask(uv) * approximateDepthMask(uv) * (1.0 - uiMask(uv)) * hazeAmount;

    vec2 aspect = vec2(resolution.x / max(resolution.y, 1.0), 1.0);
    vec2 drift = vec2(time * 0.18, time * 0.05);
    vec2 p = uv * aspect * vec2(7.0, 5.0) + drift;

    float macro = fbm(p);
    float micro = fbm(uv * aspect * vec2(34.0, 13.0) + vec2(-time * 0.55, time * 0.22));
    float verticalWave = fbm(vec2(uv.x * 20.0 + time * 0.35, uv.y * 4.0));

    vec2 offset = vec2(
        (macro - 0.5) * 0.018 + (micro - 0.5) * 0.006,
        (verticalWave - 0.5) * 0.004
    ) * heat * HAZE_STRENGTH;

    vec2 sampleUV = clamp(uv + offset, vec2(0.001), vec2(0.999));
    vec3 color = texture(sceneTexture, sampleUV).rgb;
    color = drawTemperatureUI(uv, color);
    FragColor = vec4(color, 1.0);
}
