#version 330 core
out vec4 FragColor;

struct Material {
    sampler2D diffuseSampler;
    sampler2D specularSampler;
    sampler2D normalSampler;
    float shininess;
}; 

struct Light {
    vec3 dir;
    vec3 color; // this is I_d (I_s = I_d, I_a = 0.3 * I_d)
};

uniform vec3 viewPos;
uniform Material material;
uniform Light light;

in vec2 TexCoord;

// I referenced this part from learnopengl lighting and normal and shadow code
in vec3 Normal;  
in vec3 FragPos;
in mat3 TBN;
in vec4 FragPosLightSpace;

uniform float useNormalMap;
uniform float useSpecularMap;
uniform float useDiffuseMap;
uniform vec3 baseColor;
uniform float useShadow;
uniform float useLighting;
uniform float usePCF;

// I referenced this part from learnopengl Stratified Poisson Sampling part
uniform sampler2D depthMapSampler;

// I referenced this part from learnopengl Stratified Poisson Sampling part
float random(vec3 seed, int i){
	vec4 seed4 = vec4(seed,i);
	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
	return fract(sin(dot_product) * 43758.5453);
}

// I referenced this part from learnopengl shadow part
float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(depthMapSampler, projCoords.xy).r; 
    float currentDepth = projCoords.z;
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(-light.dir);
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    float shadow = 0.0f;

    // I referenced this part from learnopengl Stratified Poisson Sampling part
    if (usePCF > 0.5f) {
        vec2 poissonDisk[16] = vec2[]( 
            vec2( -0.94201624, -0.39906216 ), 
            vec2( 0.94558609, -0.76890725 ), 
            vec2( -0.094184101, -0.92938870 ), 
            vec2( 0.34495938, 0.29387760 ), 
            vec2( -0.91588581, 0.45771432 ), 
            vec2( -0.81544232, -0.87912464 ), 
            vec2( -0.38277543, 0.27676845 ), 
            vec2( 0.97484398, 0.75648379 ), 
            vec2( 0.44323325, -0.97511554 ), 
            vec2( 0.53742981, -0.47373420 ), 
            vec2( -0.26496911, -0.41893023 ), 
            vec2( 0.79197514, 0.19090188 ), 
            vec2( -0.24188840, 0.99706507 ), 
            vec2( -0.81409955, 0.91437590 ), 
            vec2( 0.19984126, 0.78641367 ), 
            vec2( 0.14383161, -0.14100790 ) 
        );

        for (int i = 0; i < 4; i++) {
            int index = int(16.0 * random(floor(FragPos.xyz*1000.0), i)) % 16;
            float pcfDepth = texture(depthMapSampler, projCoords.xy + poissonDisk[index] / 700.0).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
        shadow /= 4.0;
    }
    else shadow = currentDepth - bias > closestDepth ? 1.0f : 0.0f;
    
    if(projCoords.z > 1.0) shadow = 0.0;
        
    return shadow;
}


void main()
{
	vec3 color;
    if (useDiffuseMap > 0.5f) {
        color = texture(material.diffuseSampler, TexCoord).rgb;
    } 
    else {
        color = baseColor;
    }

    // I referenced this part from learnopengl lighting part
    // ambient part
    vec3 ambient = 0.3 * light.color * color;
    vec3 totalColor = ambient;
    vec3 normal = normalize(Normal); // geometric normal

    // on-off by key 3 (useLighting). 
    // if useLighting is 0, return diffuse value without considering any lighting.(DO NOT CHANGE)
	if (useLighting < 0.5f){
        FragColor = vec4(color, 1.0); 
        return; 
    }

    // on-off by key 2 (useShadow).
    // calculate shadow
    // if useShadow is 0, do not consider shadow.
    // if useShadow is 1, consider shadow.
    float shadow = 0.0f;
    if(useShadow > 0.5f)
    {
        shadow = ShadowCalculation(FragPosLightSpace);
    }
    
    // on-off by key 1 (useNormalMap).
    // if model does not have a normal map, this should be always 0.
    // if useNormalMap is 0, we use a geometric normal as a surface normal.
    // if useNormalMap is 1, we use a geometric normal altered by normal map as a surface normal.
	if(useNormalMap > 0.5f)
	{
        // I referenced this part from learnopengl normal part
        normal = texture(material.normalSampler, TexCoord).rgb; // normal map
        normal = normalize(normal * 2.0 - 1.0);
        normal = normalize(TBN * normal);
	}

    // I referenced this part from learnopengl lighting part
    // diffuse part
    float diff = max(dot(normal, normalize(-light.dir)), 0.0);
    vec3 diffuse = diff * light.color * color;
    totalColor += (1.0f - shadow) * diffuse;
	
    // if model does not have a specular map, this should be always 0.
    // if useSpecularMap is 0, ignore specular lighting.
    // if useSpecularMap is 1, calculate specular lighting.
	if(useSpecularMap > 0.5f)
	{
        //use only red channel of specularSampler as a reflectance coefficient(k_s).
        if (diff > 0.0f) {
            // I referenced this part from learnopengl lighting part
            // specular part
            vec3 viewDir = normalize(viewPos - FragPos);
            vec3 reflectDir = reflect(light.dir, normal);  
            float ks = texture(material.specularSampler, TexCoord).r;
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
            vec3 specular = ks * spec * light.color; 
            totalColor += (1.0f - shadow) * specular;
        }
	}
	
    FragColor = vec4(totalColor, 1.0);
}
