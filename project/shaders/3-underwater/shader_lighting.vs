#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in ivec4 boneIds;
layout (location = 5) in vec4 weights;

out vec2 TexCoord;

out vec3 Normal;
out vec3 FragPos;
out mat3 TBN;
out vec4 FragPosLightSpace;

uniform mat4 world;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

uniform float useNormalMap;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

void main()
{
	TexCoord = aTexCoord;

	// Applying bone transformation to vertices
	vec4 totalPosition = vec4(0.0f);
	float totalWeight = 0.0f;
    for (int i = 0 ; i < MAX_BONE_INFLUENCE ; i++)
    {
        if (boneIds[i] == -1) 
            continue;
        if (boneIds[i] >= MAX_BONES) 
        {
            totalPosition = vec4(aPos, 1.0f);
            break;
        }
        vec4 localPosition = finalBonesMatrices[boneIds[i]] * vec4(aPos, 1.0f);
        totalPosition += localPosition * weights[i];
			totalWeight += weights[i];
        vec3 localNormal = mat3(finalBonesMatrices[boneIds[i]]) * aNormal;
    }
	if (totalWeight == 0.0f)
		totalPosition = vec4(aPos, 1.0f);

	FragPos = vec3(world * totalPosition);
	mat3 normalMatrix = mat3(transpose(inverse(world)));
	Normal = normalMatrix * aNormal; 

	FragPosLightSpace = lightSpaceMatrix * vec4(vec3(world * totalPosition), 1.0);

	// useNormalMap is set per submesh according to normal-map availability.
	if (useNormalMap > 0.5){
		// I referenced this part from learnopengl normal code
		vec3 T = normalize(normalMatrix * aTangent);
		vec3 N = normalize(normalMatrix * aNormal);
		T = normalize(T - dot(T, N) * N);
		vec3 B = cross(N, T);
		TBN = mat3(T, B, N);
	}

	gl_Position = projection * view * world * totalPosition;
}
