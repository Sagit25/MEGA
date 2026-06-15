#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string>
#include <vector>
#include <iostream>
#include <map>

#include "mesh.h"
#include "texture.h"

class Boid;

struct SubMesh {
    Mesh mesh;
    Texture* diffuse = nullptr;
    Texture* specular = nullptr;
    Texture* normal = nullptr;
    glm::vec3 baseColor = glm::vec3(1.0f);
};

class Model {
public:
    std::vector<SubMesh> subMeshes;
    std::vector<Texture*> textures_loaded;
    std::string directory;
    bool ignoreShadow = false;
    bool preserveMaterialTexturePath = false;

    Model() {}

    Model(const char* filePath, bool ignoreShadow = false, bool uvFlip = true, bool preserveMaterialTexturePath = false)
        : ignoreShadow(ignoreShadow), preserveMaterialTexturePath(preserveMaterialTexturePath)
    {
        loadModel(filePath, uvFlip);
    }

    virtual bool IsAnimated() const { return false; }

    void setDiffuse(const char* path) {
        Texture* tex = new Texture(path);
        if (tex->isLoaded) {
            textures_loaded.push_back(tex);
            for (auto& sm : subMeshes) sm.diffuse = tex;
        }
        else {
            delete tex;
        }
    }

    void setSpecular(const char* path) {
        Texture* tex = new Texture(path);
        if (tex->isLoaded) {
            textures_loaded.push_back(tex);
            for (auto& sm : subMeshes) sm.specular = tex;
        }
        else {
            delete tex;
        }
    }

    void setNormal(const char* path) {
        Texture* tex = new Texture(path);
        if (tex->isLoaded) {
            textures_loaded.push_back(tex);
            for (auto& sm : subMeshes) sm.normal = tex;
        }
        else {
            delete tex;
        }
    }

protected:
    static std::string getBaseName(const std::string& path)
    {
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            return path.substr(lastSlash + 1);
        }
        return path;
    }

    void loadModel(std::string const& path, bool uvFlip = true)
    {
        Assimp::Importer importer;
        unsigned int postProcessFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights;
        if (uvFlip) {
            postProcessFlags |= aiProcess_FlipUVs;
        }
        const aiScene* scene = importer.ReadFile(path, postProcessFlags);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
            return;
        }

        directory = path.substr(0, path.find_last_of('/'));
        processNode(scene->mRootNode, scene);
    }

    virtual void processNode(aiNode* node, const aiScene* scene)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            subMeshes.push_back(processMesh(mesh, scene));
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    virtual SubMesh processMesh(aiMesh* mesh, const aiScene* scene)
    {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex = {};
            glm::vec3 vector;

            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;

            if (mesh->HasNormals()) {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }
            else {
                vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            if (mesh->HasTangentsAndBitangents()) {
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
            }
            else {
                vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            if (mesh->mTextureCoords[0]) {
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            }
            else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }
            vertices.push_back(vertex);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }

        SubMesh subMesh;
        subMesh.mesh = Mesh(vertices, indices);

        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

            aiColor3D color(1.0f, 1.0f, 1.0f);
            material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
            subMesh.baseColor = glm::vec3(color.r, color.g, color.b);

            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString str;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &str);
                subMesh.diffuse = loadMaterialTexture(str.C_Str());
            }
            if (material->GetTextureCount(aiTextureType_SPECULAR) > 0) {
                aiString str;
                material->GetTexture(aiTextureType_SPECULAR, 0, &str);
                subMesh.specular = loadMaterialTexture(str.C_Str());
            }
            if (material->GetTextureCount(aiTextureType_HEIGHT) > 0) {
                aiString str;
                material->GetTexture(aiTextureType_HEIGHT, 0, &str);
                subMesh.normal = loadMaterialTexture(str.C_Str());
            }
            else if (material->GetTextureCount(aiTextureType_NORMALS) > 0) {
                aiString str;
                material->GetTexture(aiTextureType_NORMALS, 0, &str);
                subMesh.normal = loadMaterialTexture(str.C_Str());
            }
        }

        return subMesh;
    }

    Texture* loadMaterialTexture(const char* path)
    {
        std::string texturePath = std::string(path);
        if (!preserveMaterialTexturePath) {
            texturePath = getBaseName(texturePath);
        }

        std::string fullPath = directory + "/" + texturePath;

        for (unsigned int i = 0; i < textures_loaded.size(); i++) {
            if (textures_loaded[i]->path == fullPath) {
                return textures_loaded[i];
            }
        }

        Texture* texture = new Texture(fullPath.c_str());
        if (texture->isLoaded) {
            texture->path = fullPath;
            textures_loaded.push_back(texture);
            return texture;
        }

        delete texture;
        return nullptr;
    }
};

class Entity {
public:
    Model* model;
    glm::mat4 modelMatrix;
    bool visible = true;
    Boid* boid = nullptr;

    Entity(Model* model, glm::mat4 modelMatrix) {
        this->model = model;
        this->modelMatrix = modelMatrix;
    }

    Entity(Model* model, glm::vec3 position, float rotX, float rotY, float rotZ, float scale) {
        this->model = model;
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, position);
        transform = glm::rotate(transform, glm::radians(rotX), glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(rotZ), glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, glm::vec3(scale));

        this->modelMatrix = transform;
    }

    glm::mat4 getModelMatrix() {
        return this->modelMatrix;
    }
};

#endif
