#ifndef BOID_H
#define BOID_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <vector>

class Boid {
public:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 forward;
    float r = 0.5f;
    float len = 1.0f;

    Boid(float r, float len, std::vector<Boid*>& boids)
        : r(r), len(len)
    {
        randomize(boids);
    }

    void advance(float dt, std::vector<Boid*>& boids) {
        glm::vec3 force = glm::vec3(0.0f);

        force += separate(boids) * 4.0f;
        force += align(boids) * 0.5f;
        force += cohere(boids) * 0.15f;
        force += boundForce() * 1.6f;
        force += wander() * 0.05f;

        velocity += limit(force, maxForce) * dt;
        velocity = limitSpeed(velocity);
        position += velocity * dt;
        fitBound();
        updateForward(dt);
    }

    glm::mat4 calculateBoid() {
        float yaw = atan2(forward.x, forward.z);
        float pitch = asin(glm::clamp(-forward.y, -1.0f, 1.0f));

        glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotation = yawRotation * pitchRotation;
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);

        return translation * rotation;
    }

private:
    const float xMin = -40.0f;
    const float xMax = 80.0f;
    const float yMin = 0.0f;
    const float yMax = 40.0f;
    const float zMin = -50.0f;
    const float zMax = 0.0f;

    const float minSpeed = 7.0f;
    const float maxSpeed = 13.0f;
    const float maxForce = 4.0f;

    void randomize(std::vector<Boid*>& boids) {
        for (int i = 0; i < 100; i++) {
            position = randomPos();
            velocity = randomVel();
            forward = glm::normalize(velocity);

            if (isAvailable(boids)) {
                return;
            }
        }
    }

    glm::vec3 separate(std::vector<Boid*>& boids) {
        glm::vec3 steer = glm::vec3(0.0f);
        int count = 0;

        for (Boid* boid : boids) {
            if (!boid || boid == this) {
                continue;
            }

            glm::vec3 diff = position - boid->position;
            float dist = glm::length(diff);
            float avoidDist = (r + boid->r) * 2.5f;

            if (dist > 0.001f && dist < avoidDist) {
                steer += glm::normalize(diff) / dist;
                count++;
            }
        }

        if (count == 0) {
            return steer;
        }

        steer /= (float)count;
        return seekDir(steer);
    }

    glm::vec3 align(std::vector<Boid*>& boids) {
        glm::vec3 avg = glm::vec3(0.0f);
        int count = 0;

        for (Boid* boid : boids) {
            if (!boid || boid == this) {
                continue;
            }

            if (glm::length(position - boid->position) < len * 3.0f) {
                avg += boid->velocity;
                count++;
            }
        }

        if (count == 0) {
            return avg;
        }

        avg /= (float)count;
        return seekDir(avg);
    }

    glm::vec3 cohere(std::vector<Boid*>& boids) {
        glm::vec3 center = glm::vec3(0.0f);
        int count = 0;

        for (Boid* boid : boids) {
            if (!boid || boid == this) {
                continue;
            }

            if (glm::length(position - boid->position) < len * 4.0f) {
                center += boid->position;
                count++;
            }
        }

        if (count == 0) {
            return center;
        }

        center /= (float)count;
        return seek(center);
    }

    glm::vec3 boundForce() {
        glm::vec3 force = glm::vec3(0.0f);
        float margin = len * 2.0f;

        if (position.x < xMin + margin) force.x += 1.0f;
        if (position.x > xMax - margin) force.x -= 1.0f;
        if (position.y < yMin + margin) force.y += 1.0f;
        if (position.y > yMax - margin) force.y -= 1.0f;
        if (position.z < zMin + margin) force.z += 1.0f;
        if (position.z > zMax - margin) force.z -= 1.0f;

        return seekDir(force);
    }

    glm::vec3 wander() {
        return glm::sphericalRand(maxForce);
    }

    void updateForward(float dt) {
        if (glm::length(velocity) < 0.001f) {
            return;
        }

        glm::vec3 targetForward = glm::normalize(velocity);
        float u = glm::clamp(dt * 2.0f, 0.0f, 1.0f);
        forward = glm::normalize(glm::mix(forward, targetForward, u));
    }

    glm::vec3 seek(glm::vec3 target) {
        return seekDir(target - position);
    }

    glm::vec3 seekDir(glm::vec3 dir) {
        if (glm::length(dir) < 0.001f) {
            return glm::vec3(0.0f);
        }

        glm::vec3 desired = glm::normalize(dir) * maxSpeed;
        return limit(desired - velocity, maxForce);
    }

    glm::vec3 limitSpeed(glm::vec3 v) {
        float speed = glm::length(v);

        if (speed < 0.001f) {
            return randomVel();
        }
        if (speed > maxSpeed) {
            return glm::normalize(v) * maxSpeed;
        }
        if (speed < minSpeed) {
            return glm::normalize(v) * minSpeed;
        }

        return v;
    }

    glm::vec3 limit(glm::vec3 v, float maxLength) {
        if (glm::length(v) > maxLength) {
            return glm::normalize(v) * maxLength;
        }

        return v;
    }

    glm::vec3 randomPos() {
        return glm::vec3(
            glm::linearRand(xMin, xMax),
            glm::linearRand(yMin, yMax),
            glm::linearRand(zMin, zMax));
    }

    glm::vec3 randomVel() {
        glm::vec3 v = glm::sphericalRand(1.0f);
        v.y *= 0.35f;
        return glm::normalize(v) * glm::linearRand(minSpeed, maxSpeed);
    }

    bool isAvailable(std::vector<Boid*>& boids) {
        for (Boid* boid : boids) {
            if (!boid) {
                continue;
            }

            glm::vec3 diff = position - boid->position;
            float radiusSum = r + boid->r;

            if (glm::dot(diff, diff) <= radiusSum * radiusSum) {
                return false;
            }
        }

        return true;
    }

    void fitBound() {
        if (position.x < xMin) {
            position.x = xMin;
            velocity.x = glm::abs(velocity.x);
        }
        if (position.x > xMax) {
            position.x = xMax;
            velocity.x = -glm::abs(velocity.x);
        }
        if (position.y < yMin) {
            position.y = yMin;
            velocity.y = glm::abs(velocity.y);
        }
        if (position.y > yMax) {
            position.y = yMax;
            velocity.y = -glm::abs(velocity.y);
        }
        if (position.z < zMin) {
            position.z = zMin;
            velocity.z = glm::abs(velocity.z);
        }
        if (position.z > zMax) {
            position.z = zMax;
            velocity.z = -glm::abs(velocity.z);
        }
    }
};

#endif
