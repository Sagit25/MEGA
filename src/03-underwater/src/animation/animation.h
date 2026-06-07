// Based on the skeletal animation part of LearnOpenGL.

#ifndef ANIMATION_H
#define ANIMATION_H

#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>

#include "assimp_glm_helpers.h"
#include "bone.h"
#include "bone_info.h"

using namespace std;

// custom aiNode structure for independent handling
struct AssimpNodeData
{
	string name;
	glm::mat4 transformation;
	int childrenCount;
	vector<AssimpNodeData> children;
};

class Animation
{
public:

	template <typename AnimationModelType>
	Animation(const string& animationPath, AnimationModelType* model)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(animationPath, aiProcess_Triangulate);
		assert(scene && scene->mRootNode);

		auto animation = scene->mAnimations[0];
		m_Duration = animation->mDuration;
		m_TicksPerSecond = animation->mTicksPerSecond;
		
		ReadHierarchyData(m_RootNode, scene->mRootNode);
		ReadMissingBones(animation, *model);
	}

	Bone* FindBone(const string& name)
	{
		auto iter = find_if(m_Bones.begin(), m_Bones.end(),
			[&](const Bone& Bone)
			{
				return Bone.GetBoneName() == name;
			}
		);
		if (iter == m_Bones.end()) return nullptr;
		else return &(*iter);
	}

	inline float GetTicksPerSecond() { return m_TicksPerSecond; }
	inline float GetDuration() { return m_Duration;}
	inline void SetDuration(float duration) { m_Duration = duration; }
	inline const AssimpNodeData& GetRootNode() { return m_RootNode; }
	inline const map<string, BoneInfo>& GetBoneIDMap() { return m_BoneInfoMap; }

private:
	float m_Duration;
	int m_TicksPerSecond;
	vector<Bone> m_Bones;
	AssimpNodeData m_RootNode;
	map<string, BoneInfo> m_BoneInfoMap;

	// complete boneinfo map + setup bone animation datas
	template <typename AnimationModelType>
	void ReadMissingBones(const aiAnimation* animation, AnimationModelType& model)
	{
		int size = animation->mNumChannels;

		auto& boneInfoMap = model.GetBoneInfoMap();
		int& boneCount = model.GetBoneCount();

		for (int i = 0; i < size; i++)
		{
			auto channel = animation->mChannels[i];
			string boneName = channel->mNodeName.data;

			if (boneInfoMap.find(boneName) == boneInfoMap.end())
			{
				boneInfoMap[boneName].id = boneCount;
				boneCount++;
			}
			m_Bones.push_back(Bone(channel->mNodeName.data,
				boneInfoMap[channel->mNodeName.data].id, channel));
		}

		m_BoneInfoMap = boneInfoMap;
	}

	// copy assimp scene hierarchy to custom data
	void ReadHierarchyData(AssimpNodeData& dest, const aiNode* src)
	{
		assert(src);

		dest.name = src->mName.data;
		dest.transformation = AssimpGLMHelpers::ConvertMatrixToGLMFormat(src->mTransformation);
		dest.childrenCount = src->mNumChildren;

		for (int i = 0; i < src->mNumChildren; i++)
		{
			AssimpNodeData newData;
			ReadHierarchyData(newData, src->mChildren[i]);
			dest.children.push_back(newData);
		}
	}
};

#endif
