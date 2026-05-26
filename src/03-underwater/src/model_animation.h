#ifndef MODEL_ANIMATION_H
#define MODEL_ANIMATION_H

#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "model.h"
#include "mesh.h"
#include "shader.h"
#include "assimp_glm_helpers.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

using namespace std;

class Animator;

struct BoneInfo
{
	// index in finalBoneMatrices
	int id;
	// matrix transforms vertex from model space to bone space
	glm::mat4 offset;
};

class AnimationModel : public Model
{
public:
    Animator* animator;
    float radius = 0.0f;

    AnimationModel(const char* filePath, bool ignoreShadow = false, bool uvFlip = true)
    {
        loadModel(filePath, uvFlip);
        setRadius();
    }

    std::map<string, BoneInfo>& GetBoneInfoMap() { return m_BoneInfoMap; }
	int& GetBoneCount() { return m_BoneCounter; }

    virtual bool IsAnimated() const override { return true; }

protected:

	std::map<string, BoneInfo> m_BoneInfoMap;
	int m_BoneCounter = 0;
    
    glm::vec3 minPos = glm::vec3(+1000000.0f);
    glm::vec3 maxPos = glm::vec3(-1000000.0f);

    void addPos(glm::vec3 position) {
        minPos.x = std::min(minPos.x, position.x);
        minPos.y = std::min(minPos.y, position.y);
        minPos.z = std::min(minPos.z, position.z);

        maxPos.x = std::max(maxPos.x, position.x);
        maxPos.y = std::max(maxPos.y, position.y);
        maxPos.z = std::max(maxPos.z, position.z);
    }

    void setRadius() {
        glm::vec3 halfSize = (maxPos - minPos) * 0.5f;
        radius = std::max(halfSize.y, halfSize.z);
    }

	void SetVertexBoneDataToDefault(Vertex& vertex)
	{
		for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
		{
			vertex.m_BoneIDs[i] = -1;
			vertex.m_Weights[i] = 0.0f;
		}
	}

	void SetVertexBoneData(Vertex& vertex, int boneID, float weight)
	{
		for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
		{
			if (vertex.m_BoneIDs[i] < 0)
			{
				vertex.m_Weights[i] = weight;
				vertex.m_BoneIDs[i] = boneID;
				break;
			}
		}
	}

	void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene)
	{
		auto& boneInfoMap = m_BoneInfoMap;
		int& boneCount = m_BoneCounter;

		for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
		{
			int boneID = -1;
			std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();
			if (boneInfoMap.find(boneName) == boneInfoMap.end())
			{
				BoneInfo newBoneInfo;
				newBoneInfo.id = boneCount;
				newBoneInfo.offset = AssimpGLMHelpers::ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
				boneInfoMap[boneName] = newBoneInfo;
				boneID = boneCount;
				boneCount++;
			}
			else
			{
				boneID = boneInfoMap[boneName].id;
			}
			assert(boneID != -1);
			auto weights = mesh->mBones[boneIndex]->mWeights;
			int numWeights = mesh->mBones[boneIndex]->mNumWeights;

			for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex)
			{
				int vertexId = weights[weightIndex].mVertexId;
				float weight = weights[weightIndex].mWeight;
				assert(vertexId <= vertices.size());
				SetVertexBoneData(vertices[vertexId], boneID, weight);
			}
		}
	}

    virtual void processNode(aiNode *node, const aiScene *scene) override {
        if (std::string(node->mName.C_Str()) == "Sharkjaw") {
            return;
        }
        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            subMeshes.push_back(processMesh(mesh, scene));
        }
        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

	// Submech processing (virtual for child class)
    virtual SubMesh processMesh(aiMesh *mesh, const aiScene *scene) override {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex = {};
            glm::vec3 vector;

			SetVertexBoneDataToDefault(vertex);
            
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            addPos(vector);

            if (mesh->HasNormals()) {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }

            if (mesh->HasTangentsAndBitangents()) {
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
            } else {
                vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            if(mesh->mTextureCoords[0]) {
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            } else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }
            vertices.push_back(vertex);
        }

        for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        SubMesh subMesh;

        // mtl file based load texture
        if(mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            
            // Diffuse
            if(material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString str;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &str);
                subMesh.diffuse = loadMaterialTexture(str.C_Str());
            }
            // Specular
            if(material->GetTextureCount(aiTextureType_SPECULAR) > 0) {
                aiString str;
                material->GetTexture(aiTextureType_SPECULAR, 0, &str);
                subMesh.specular = loadMaterialTexture(str.C_Str());
            }
            // Normal
            if(material->GetTextureCount(aiTextureType_HEIGHT) > 0) {
                aiString str;
                material->GetTexture(aiTextureType_HEIGHT, 0, &str);
                subMesh.normal = loadMaterialTexture(str.C_Str());
            } else if(material->GetTextureCount(aiTextureType_NORMALS) > 0) {
                aiString str;
                material->GetTexture(aiTextureType_NORMALS, 0, &str);
                subMesh.normal = loadMaterialTexture(str.C_Str());
            }
        }

		ExtractBoneWeightForVertices(vertices, mesh, scene);
        
        subMesh.mesh = Mesh(vertices, indices);
        return subMesh;
    }
};

#endif
