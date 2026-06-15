// Based on the skeletal animation part of LearnOpenGL.

#ifndef MODEL_ANIMATION_H
#define MODEL_ANIMATION_H

#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <vector>

#include <assimp/scene.h>
#include <glm/glm.hpp>

#include "shared/model.h"
#include "shared/mesh.h"
#include "assimp_glm_helpers.h"
#include "bone_info.h"

using namespace std;

class Animator;

class AnimationModel : public Model
{
public:
    Animator* animator;
    float radius = 0.0f; // for collision avoid
    float length = 0.0f; // for boid calculation

    AnimationModel(const char* filePath, bool ignoreShadow = false, bool uvFlip = true)
    {
        this->ignoreShadow = ignoreShadow;
        this->preserveMaterialTexturePath = true;
        loadModel(filePath, uvFlip);
        setModelSize();
    }

    map<string, BoneInfo>& GetBoneInfoMap() { return m_BoneInfoMap; }
	int& GetBoneCount() { return m_BoneCounter; }
    virtual bool IsAnimated() const override { return true; }

protected:

    // Bones are shared across meshes
	map<string, BoneInfo> m_BoneInfoMap;
	int m_BoneCounter = 0;
    
    // for radius/length calculation
    glm::vec3 minPos = glm::vec3(+1000000.0f);
    glm::vec3 maxPos = glm::vec3(-1000000.0f);

    // renew minPos, maxPos
    void renewEdgePosition(glm::vec3 position) {
        minPos.x = min(minPos.x, position.x);
        minPos.y = min(minPos.y, position.y);
        minPos.z = min(minPos.z, position.z);

        maxPos.x = max(maxPos.x, position.x);
        maxPos.y = max(maxPos.y, position.y);
        maxPos.z = max(maxPos.z, position.z);
    }

    /*
     * set radius/length
     * assume the model's forward direction is the x-axis.
     */
    void setModelSize() {
        glm::vec3 halfSize = (maxPos - minPos) * 0.5f;
        radius = max(halfSize.y, halfSize.z);
        length = maxPos.x - minPos.x;
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

    // extract bone data from mesh and apply weight to related vertices
	void ExtractBoneWeightForVertices(vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene)
	{
		auto& boneInfoMap = m_BoneInfoMap;
		int& boneCount = m_BoneCounter;

		for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
		{
			int boneID = -1;
			string boneName = mesh->mBones[boneIndex]->mName.C_Str();
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
        // cannot process sharkjaw node with current code
        if (string(node->mName.C_Str()) == "Sharkjaw") {
            return;
        }

        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            subMeshes.push_back(processMesh(mesh, scene));
        }

        // recursively apply to children
        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    virtual SubMesh processMesh(aiMesh *mesh, const aiScene *scene) override {
        vector<Vertex> vertices;
        vector<unsigned int> indices;

        // handle vertex: extract normal, tangent, tex coord
        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex = {};
            glm::vec3 vector;

			SetVertexBoneDataToDefault(vertex);
            
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            renewEdgePosition(vector);

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

        // handle face: extract indice
        for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        SubMesh subMesh;

        // extract textures
        if(mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            
            if(material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString str;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &str);
                subMesh.diffuse = loadMaterialTexture(str.C_Str());
            }
            if(material->GetTextureCount(aiTextureType_SPECULAR) > 0) {
                aiString str;
                material->GetTexture(aiTextureType_SPECULAR, 0, &str);
                subMesh.specular = loadMaterialTexture(str.C_Str());
            }
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

        // handle vertex: extract bone and bone weights
		ExtractBoneWeightForVertices(vertices, mesh, scene);
        
        subMesh.mesh = Mesh(vertices, indices);
        return subMesh;
    }
};

#endif
