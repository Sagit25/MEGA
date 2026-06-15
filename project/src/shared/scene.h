#pragma once
#ifndef SCENE_H
#define SCENE_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>

#include <vector>
class Scene {
public:
    std::map<Model*, std::vector<Entity*>> entities;

    Scene() {
        this->entities = std::map<Model*, std::vector<Entity*>>();
    }

    void addEntity(Entity* entity) {
        if (entities.find(entity->model) == entities.end()) {
            entities[entity->model] = std::vector<Entity*>();
        }
        entities[entity->model].push_back(entity);
    }
};

#endif
