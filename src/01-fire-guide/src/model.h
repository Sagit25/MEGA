#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Assimp 헤더 추가
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "mesh.h"
#include "texture.h"

#include <string>
#include <vector>
#include <iostream>

// 메쉬 파트와 해당 파트의 텍스처를 묶어주는 구조체
struct SubMesh {
    Mesh mesh; 
    Texture* diffuse = nullptr;
    Texture* specular = nullptr;
    Texture* normal = nullptr;
};

class Model {
public:
    std::vector<SubMesh> subMeshes;
    std::vector<Texture*> textures_loaded; // 중복 로드 방지용 캐시
    std::string directory;
    bool ignoreShadow = false;

    Model(const char* filePath, bool ignoreShadow = false) : ignoreShadow(ignoreShadow) {
        loadModel(filePath);
    }

private:
    void loadModel(std::string const &path) {
        Assimp::Importer importer;
        // Triangulate(삼각형화), GenSmoothNormals(노멀 계산), FlipUVs(텍스처 뒤집기) 적용
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
            return;
        }
        directory = path.substr(0, path.find_last_of('/'));

        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode *node, const aiScene *scene) {
        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            subMeshes.push_back(processMesh(mesh, scene));
        }
        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    SubMesh processMesh(aiMesh *mesh, const aiScene *scene) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        // 1. 정점(Vertex) 데이터 추출
        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
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

        // 2. 인덱스(Index) 데이터 추출
        for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        SubMesh subMesh;
        subMesh.mesh = Mesh(vertices, indices);

        // 3. 매터리얼(Texture) 추출 (.mtl 파일 기반)
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
            // Normal (일부 모델은 HEIGHT로 노멀맵을 저장함)
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
        return subMesh;
    }

    Texture* loadMaterialTexture(const char* path) {
        std::string fullPath = directory + "/" + std::string(path);
        
        // 캐시 확인
        for(unsigned int i = 0; i < textures_loaded.size(); i++) {
            if(textures_loaded[i]->path == fullPath) {
                return textures_loaded[i];
            }
        }
        
        Texture* texture = new Texture(fullPath.c_str());
        textures_loaded.push_back(texture);
        return texture;
    }
};

class Entity {
public:
    Model* model;
    glm::mat4 modelMatrix;
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
