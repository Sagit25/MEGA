#version 330 core
in vec2 TexCoords;
in vec4 ParticleColor;

out vec4 FragColor;

void main()
{
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(TexCoords, center);
    float alpha = smoothstep(0.5, 0.1, dist);
    if(alpha <= 0.0) discard;
    FragColor = vec4(ParticleColor.rgb, ParticleColor.a * alpha);
}