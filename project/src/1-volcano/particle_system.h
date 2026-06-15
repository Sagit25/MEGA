#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/noise.hpp>
#include <vector>
#include <cstdlib>

#include "shared/shader.h"
#include "shared/camera.h"
#include "shared/model.h"

// Particle structure
struct Particle {
    glm::vec3 Position, Velocity, Normal;
    glm::vec4 Color;
    float Life;
    float Size;
    Particle() : Position(0.0f), Velocity(0.0f), Normal(0.0f, 1.0f, 0.0f), Color(1.0f), Life(0.0f), Size(0.05f) { }
};

class FireParticleSystem {
public:
    // FireParticleSystem generator
    FireParticleSystem(Shader shader, unsigned int amount)
        : shader(shader), amount(amount) {
        this->init();
    }

    // Update particles at vertices of mesh
    void Update(float dt, float time, Model* targetModel, glm::mat4 modelMatrix, unsigned int newParticles = 100) {
        for (unsigned int i = 0; i < newParticles; ++i) {
            int unusedParticle = this->firstUnusedParticle();
            this->respawnParticle(this->particles[unusedParticle], targetModel, modelMatrix);
        }

        // Update all particles
        for (unsigned int i = 0; i < this->amount; ++i) {
            Particle &p = this->particles[i];
            p.Life -= dt; 
            
            if (p.Life > 0.0f) {
                float noiseScale = 2.0f;
                glm::vec3 samplePos = p.Position * noiseScale + glm::vec3(0.0f, time * 1.25f, 0.0f);
                
                // [Feature] Curl Noise
                glm::vec3 curl = computeCurlNoise(samplePos);
                glm::vec3 desiredVelocity = glm::vec3(0.0f, 1.2f, 0.0f) + curl * 1.5f;

                // [Feature] Guided particles to flow along the surface normal using normal
                float normalComponent = glm::dot(desiredVelocity, p.Normal);
                if (normalComponent > 0.0f) desiredVelocity -= p.Normal * (normalComponent * 0.8f);
                else desiredVelocity -= p.Normal * normalComponent;
                p.Position += desiredVelocity * dt; 
                float escapeSpeed = 0.1f; 
                p.Position += (p.Normal * 0.5f + glm::vec3(0.0f, 1.0f, 0.0f)) * escapeSpeed * dt;
            }
        }
    }

    void Draw(Camera& camera) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        this->shader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), 1920.0f / 1080.0f, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        this->shader.setMat4("projection", projection);
        this->shader.setMat4("view", view);

        glBindVertexArray(this->VAO);
        for (Particle particle : this->particles) {
            if (particle.Life > 0.0f) {
                this->shader.setVec4("color", particle.Color);
                
                // [Feature] Billboard
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, particle.Position);
                model = glm::scale(model, glm::vec3(0.05f));
                model[0][0] = view[0][0]; model[0][1] = view[1][0]; model[0][2] = view[2][0];
                model[1][0] = view[0][1]; model[1][1] = view[1][1]; model[1][2] = view[2][1];
                model[2][0] = view[0][2]; model[2][1] = view[1][2]; model[2][2] = view[2][2];

                this->shader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
    }

private:
    std::vector<Particle> particles;
    unsigned int amount;
    Shader shader;
    unsigned int VAO;

    // Curl Noise helper function
    glm::vec3 snoiseVec3(glm::vec3 x) {
        float s  = glm::simplex(x);
        float s1 = glm::simplex(glm::vec3(x.y - 19.1f, x.z + 33.4f, x.x + 47.2f));
        float s2 = glm::simplex(glm::vec3(x.z + 74.2f, x.x - 124.5f, x.y + 99.4f));
        return glm::vec3(s, s1, s2);
    }

    // [Feature] Curl Noise 
    glm::vec3 computeCurlNoise(glm::vec3 p) {
        const float e = 0.1f;
        glm::vec3 dx = glm::vec3(e, 0.0f, 0.0f);
        glm::vec3 dy = glm::vec3(0.0f, e, 0.0f);
        glm::vec3 dz = glm::vec3(0.0f, 0.0f, e);

        glm::vec3 p_x0 = snoiseVec3(p - dx);
        glm::vec3 p_x1 = snoiseVec3(p + dx);
        glm::vec3 p_y0 = snoiseVec3(p - dy);
        glm::vec3 p_y1 = snoiseVec3(p + dy);
        glm::vec3 p_z0 = snoiseVec3(p - dz);
        glm::vec3 p_z1 = snoiseVec3(p + dz);

        float x = p_y1.z - p_y0.z - p_z1.y + p_z0.y;
        float y = p_z1.x - p_z0.x - p_x1.z + p_x0.z;
        float z = p_x1.y - p_x0.y - p_y1.x + p_y0.x;

        const float divisor = 1.0f / (2.0f * e);
        return glm::normalize(glm::vec3(x, y, z) * divisor);
    }

    void init() {
        unsigned int VBO;
        float particle_quad[] = {
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f
        };
        glGenVertexArrays(1, &this->VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(this->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(particle_quad), particle_quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        for (unsigned int i = 0; i < this->amount; ++i)
            this->particles.push_back(Particle());
    }

    unsigned int lastUsedParticle = 0;
    unsigned int firstUnusedParticle() {
        for (unsigned int i = lastUsedParticle; i < this->amount; ++i) {
            if (this->particles[i].Life <= 0.0f) {
                lastUsedParticle = i;
                return i;
            }
        }
        for (unsigned int i = 0; i < lastUsedParticle; ++i) {
            if (this->particles[i].Life <= 0.0f) {
                lastUsedParticle = i;
                return i;
            }
        }
        lastUsedParticle = 0;
        return 0;
    }

    // Spawn particle at random vertex of submesh
    void respawnParticle(Particle &particle, Model* targetModel, glm::mat4 modelMatrix) {
        if (!targetModel || targetModel->subMeshes.empty()) return;
        int subMeshIdx = rand() % targetModel->subMeshes.size();
        auto& vertices = targetModel->subMeshes[subMeshIdx].mesh.vertices;
        if (vertices.empty()) return;
        Vertex randomVertex = vertices[rand() % vertices.size()];

        // Transformation between space
        particle.Position = glm::vec3(modelMatrix * glm::vec4(randomVertex.Position, 1.0f));
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));
        particle.Normal = glm::normalize(normalMatrix * randomVertex.Normal);

        float hoverOffset = 0.01f; // set little above the vertex
        particle.Position += particle.Normal * hoverOffset;
        particle.Color = glm::vec4(1.0f, (rand() % 30)/100.0f, 0.0f, 1.0f); 
        particle.Life = 1.0f + (rand() % 100) / 100.0f; 
        particle.Velocity = glm::vec3(0.0f); 
    }
};

// [Feature] Meteor Trail Particle System: Add some random concepts
class MeteorParticleSystem {
public:
    MeteorParticleSystem(Shader shader, unsigned int amount)
        : shader(shader), amount(amount) {
        this->init();
    }

    void EmitTrail(const glm::vec3& headPosition, const glm::vec3& velocity, unsigned int newParticles = 25,
                   float particleSize = 0.14f, float spread = 0.25f, float minLife = 0.35f, float maxLife = 0.9f) {
        glm::vec3 trailDir = glm::normalize(-velocity);
        glm::vec3 sideA = glm::cross(trailDir, glm::vec3(0.0f, 1.0f, 0.0f));
        if (glm::length(sideA) < 0.01f) {
            sideA = glm::vec3(1.0f, 0.0f, 0.0f);
        } else {
            sideA = glm::normalize(sideA);
        }
        glm::vec3 sideB = glm::normalize(glm::cross(trailDir, sideA));

        for (unsigned int i = 0; i < newParticles; ++i) {
            int unusedParticle = this->firstUnusedParticle();
            this->respawnParticle(this->particles[unusedParticle], headPosition, velocity, trailDir, sideA, sideB,
                                  particleSize, spread, minLife, maxLife);
        }
    }

    void EmitSurfaceFire(Model* targetModel, const glm::mat4& modelMatrix, const glm::vec3& velocity,
                         unsigned int newParticles = 40, float particleSize = 0.14f, float spread = 0.5f,
                         float minLife = 0.45f, float maxLife = 1.0f) {
        if (!targetModel || targetModel->subMeshes.empty()) return;

        glm::vec3 trailDir = glm::normalize(-velocity);
        for (unsigned int i = 0; i < newParticles; ++i) {
            int unusedParticle = this->firstUnusedParticle();
            this->respawnSurfaceParticle(this->particles[unusedParticle], targetModel, modelMatrix, velocity,
                                         trailDir, particleSize, spread, minLife, maxLife);
        }
    }

    void Update(float dt) {
        for (unsigned int i = 0; i < this->amount; ++i) {
            Particle& p = this->particles[i];
            p.Life -= dt;
            if (p.Life > 0.0f) {
                p.Position += p.Velocity * dt;
                p.Velocity *= 0.97f;
                p.Color.a = glm::clamp(p.Life / 0.9f, 0.0f, 1.0f);
            }
        }
    }

    void Draw(Camera& camera) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        this->shader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), 1920.0f / 1080.0f, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        this->shader.setMat4("projection", projection);
        this->shader.setMat4("view", view);

        glBindVertexArray(this->VAO);
        for (Particle particle : this->particles) {
            if (particle.Life > 0.0f) {
                this->shader.setVec4("color", particle.Color);

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, particle.Position);
                model = glm::scale(model, glm::vec3(particle.Size));
                model[0][0] = view[0][0]; model[0][1] = view[1][0]; model[0][2] = view[2][0];
                model[1][0] = view[0][1]; model[1][1] = view[1][1]; model[1][2] = view[2][1];
                model[2][0] = view[0][2]; model[2][1] = view[1][2]; model[2][2] = view[2][2];

                this->shader.setMat4("model", model);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
    }

private:
    std::vector<Particle> particles;
    unsigned int amount;
    Shader shader;
    unsigned int VAO;
    unsigned int lastUsedParticle = 0;

    void init() {
        unsigned int VBO;
        float particle_quad[] = {
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f
        };
        glGenVertexArrays(1, &this->VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(this->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(particle_quad), particle_quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        for (unsigned int i = 0; i < this->amount; ++i) {
            this->particles.push_back(Particle());
        }
    }

    unsigned int firstUnusedParticle() {
        for (unsigned int i = lastUsedParticle; i < this->amount; ++i) {
            if (this->particles[i].Life <= 0.0f) {
                lastUsedParticle = i;
                return i;
            }
        }
        for (unsigned int i = 0; i < lastUsedParticle; ++i) {
            if (this->particles[i].Life <= 0.0f) {
                lastUsedParticle = i;
                return i;
            }
        }
        lastUsedParticle = 0;
        return 0;
    }

    void respawnParticle(Particle& particle, const glm::vec3& headPosition, const glm::vec3& velocity,
                         const glm::vec3& trailDir, const glm::vec3& sideA, const glm::vec3& sideB,
                         float particleSize, float spread, float minLife, float maxLife) {
        float trailOffset = 0.2f + (rand() % 120) / 100.0f;
        float spreadA = ((rand() % 100) / 100.0f - 0.5f) * spread;
        float spreadB = ((rand() % 100) / 100.0f - 0.5f) * spread;
        particle.Position = headPosition + trailDir * trailOffset + sideA * spreadA + sideB * spreadB;
        particle.Velocity = -velocity * (0.08f + (rand() % 40) / 1000.0f) + trailDir * (0.5f + (rand() % 80) / 100.0f);
        particle.Normal = trailDir;
        particle.Color = glm::vec4(1.0f, 0.25f + (rand() % 55) / 100.0f, 0.02f, 1.0f);
        particle.Life = minLife + ((rand() % 100) / 100.0f) * (maxLife - minLife);
        particle.Size = particleSize * (0.75f + (rand() % 60) / 100.0f);
    }

    void respawnSurfaceParticle(Particle& particle, Model* targetModel, const glm::mat4& modelMatrix,
                                const glm::vec3& velocity, const glm::vec3& trailDir,
                                float particleSize, float spread, float minLife, float maxLife) {
        int subMeshIdx = rand() % targetModel->subMeshes.size();
        auto& vertices = targetModel->subMeshes[subMeshIdx].mesh.vertices;
        if (vertices.empty()) {
            particle.Life = 0.0f;
            return;
        }

        Vertex randomVertex = vertices[rand() % vertices.size()];
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));
        glm::vec3 normal = glm::normalize(normalMatrix * randomVertex.Normal);
        if (glm::length(normal) < 0.01f) {
            normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        glm::vec3 sideA = glm::cross(normal, trailDir);
        if (glm::length(sideA) < 0.01f) {
            sideA = glm::cross(normal, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        if (glm::length(sideA) < 0.01f) {
            sideA = glm::vec3(0.0f, 0.0f, 1.0f);
        } else {
            sideA = glm::normalize(sideA);
        }
        glm::vec3 sideB = glm::normalize(glm::cross(normal, sideA));

        float outwardOffset = 0.02f + (rand() % 100) / 100.0f * spread * 0.2f;
        float sideOffsetA = ((rand() % 100) / 100.0f - 0.5f) * spread * 0.2f;
        float sideOffsetB = ((rand() % 100) / 100.0f - 0.5f) * spread * 0.2f;
        particle.Position = glm::vec3(modelMatrix * glm::vec4(randomVertex.Position, 1.0f));
        particle.Position += normal * outwardOffset + sideA * sideOffsetA + sideB * sideOffsetB;
        particle.Velocity = trailDir * (0.8f + (rand() % 100) / 100.0f * 1.2f)
                          + normal * (0.3f + (rand() % 100) / 100.0f * 0.8f)
                          - velocity * (0.04f + (rand() % 40) / 1000.0f);
        particle.Normal = normal;
        particle.Color = glm::vec4(1.0f, 0.22f + (rand() % 60) / 100.0f, 0.02f, 1.0f);
        particle.Life = minLife + ((rand() % 100) / 100.0f) * (maxLife - minLife);
        particle.Size = particleSize * (0.8f + (rand() % 70) / 100.0f);
    }
};

#endif
