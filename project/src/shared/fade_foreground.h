#ifndef FADE_FOREGROUND_H
#define FADE_FOREGROUND_H

#include <algorithm>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "light.h"
#include "model.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"

inline void addFadeForegroundEntity(Scene& scene, std::vector<Entity*>& foregroundEntities, Entity* entity)
{
    foregroundEntities.push_back(entity);
    scene.addEntity(entity);
}

inline void beginFadeForegroundRender(int framebufferWidth, int framebufferHeight)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glClear(GL_DEPTH_BUFFER_BIT);
}

inline void endFadeForegroundRender()
{
    glBindVertexArray(0);
}

inline void drawLitFadeForegroundEntities(Shader& shader, const std::vector<Entity*>& entities)
{
    for (Entity* entity : entities) {
        if (!entity || !entity->visible || !entity->model) {
            continue;
        }

        shader.setMat4("world", entity->getModelMatrix());

        for (const SubMesh& subMesh : entity->model->subMeshes) {
            shader.setVec3("baseColor", subMesh.baseColor);

            glActiveTexture(GL_TEXTURE0);
            if (subMesh.diffuse) {
                glBindTexture(GL_TEXTURE_2D, subMesh.diffuse->ID);
                shader.setFloat("useDiffuseMap", 1.0f);
            }
            else {
                glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                shader.setFloat("useDiffuseMap", 0.0f);
            }

            glActiveTexture(GL_TEXTURE1);
            if (subMesh.specular) {
                glBindTexture(GL_TEXTURE_2D, subMesh.specular->ID);
                shader.setFloat("useSpecularMap", 1.0f);
            }
            else {
                glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                shader.setFloat("useSpecularMap", 0.0f);
            }

            glActiveTexture(GL_TEXTURE2);
            if (subMesh.normal) {
                glBindTexture(GL_TEXTURE_2D, subMesh.normal->ID);
                shader.setFloat("useNormalMap", 1.0f);
            }
            else {
                glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                shader.setFloat("useNormalMap", 0.0f);
            }

            glBindVertexArray(subMesh.mesh.VAO);
            glDrawElements(GL_TRIANGLES, subMesh.mesh.indices.size(), GL_UNSIGNED_INT, 0);
        }
    }
}

inline void renderLitFadeForegroundEntities(
    Shader& shader,
    const std::vector<Entity*>& entities,
    Camera& camera,
    DirectionalLight& light,
    DepthMapTexture& depth,
    int framebufferWidth,
    int framebufferHeight,
    CausticTexture* caustics = nullptr,
    int causticFrameCount = 0,
    float currentTime = 0.0f)
{
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    beginFadeForegroundRender(framebufferWidth, framebufferHeight);

    glm::mat4 lightProjection = light.getProjectionMatrix();
    glm::mat4 lightView = light.getViewMatrix(camera.Position);
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    glm::mat4 projection = glm::perspective(
        glm::radians(camera.Zoom),
        static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
        0.1f,
        100.0f
    );
    glm::mat4 view = camera.GetViewMatrix();

    shader.use();
    shader.setFloat("useLighting", 1.0f);
    shader.setFloat("useShadow", 1.0f);
    shader.setFloat("usePCF", 1.0f);
    shader.setVec3("light.dir", light.lightDir);
    shader.setVec3("light.color", light.lightColor);
    shader.setVec3("viewPos", camera.Position);
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);
    shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, depth.ID);

    if (caustics) {
        shader.setFloat("currentTime", currentTime);
        shader.setFloat("causticFrameCount", static_cast<float>(std::max(causticFrameCount, 1)));
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D_ARRAY, caustics->ID);
    }

    drawLitFadeForegroundEntities(shader, entities);
    endFadeForegroundRender();
}

#endif
