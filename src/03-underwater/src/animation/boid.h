#ifndef BOID_H
#define BOID_H

#include <cmath>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

using namespace std;

class Boid {
public:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 forward;
    // unique parameter of object
    float r = 0.5f;
    float len = 1.0f;

    Boid(float r, float len, vector<Boid*>& boids, float groupWeight = 1.0f)
        : r(r), len(len),
          avoidDist(r * 2.1f),
          alignDist(len * 5.5f),
          cohereDist(len * 7.0f),
          margin(len * 2.0f),
          groupWeight(groupWeight)
    {
        randomize(boids);
    }

    /*
     * update velocity/position of object
     * based on Boids algorithm of Craig Reynolds
     */ 
    void advance(float dt, vector<Boid*>& groupBoids, vector<Boid*>& allBoids) {
        glm::vec3 force = glm::vec3(0.0f);

        force += separate(allBoids) * sepWeight;
        force += align(groupBoids) * alignWeight * groupWeight;
        force += cohere(groupBoids) * cohereWeight * groupWeight;
        force += boundForce() * boundWeight;
        force += wander() * wanderWeight;
        force = limit(force, maxForce);

        velocity += force * dt;
        velocity = limitSpeed(velocity);

        position += velocity * dt;
        
        resolveOverlaps(allBoids);
        fitBound();
        updateForward(dt);
    }

    // transform matrix of the object
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
    static constexpr float sepWeight = 3.8f;
    static constexpr float alignWeight = 1.1f;
    static constexpr float cohereWeight = 0.6f;
    static constexpr float boundWeight = 1.5f;
    static constexpr float wanderWeight = 0.03f;

    static constexpr float initVelYScale = 0.25f;
    static constexpr float wanderYScale = 0.25f;
    static constexpr float steerYScale = 0.45f;
    static constexpr float velYScale = 0.995f;

    const float xMin = -85.0f;
    const float xMax = 85.0f;
    const float yMin = 0.0f;
    const float yMax = 40.0f;
    const float zMin = -85.0f;
    const float zMax = 42.5f;

    const float minSpeed = 11.0f;
    const float maxSpeed = 20.0f;
    const float maxForce = 6.0f;

    float avoidDist;
    float alignDist;
    float cohereDist;
    float margin;
    float groupWeight;

    /*
     * setting initial state of current object
     * should not overlap with other objects
     * try with random state at most 100 times
     */ 
    void randomize(vector<Boid*>& boids) {
        for (int i = 0; i < 100; i++) {
            position = randomPos();
            velocity = randomVel();
            forward = glm::normalize(velocity);

            if (isAvailable(boids)) {
                return;
            }
        }
    }

    glm::vec3 randomPos() {
        return glm::vec3(
            glm::linearRand(xMin, xMax),
            glm::linearRand(yMin, yMax),
            glm::linearRand(zMin, zMax));
    }

    glm::vec3 randomVel() {
        glm::vec3 v = glm::sphericalRand(1.0f);
        v.y *= initVelYScale;
        return glm::normalize(v) * glm::linearRand(minSpeed, maxSpeed);
    }

    // is this object doesn't overlap with others?
    bool isAvailable(vector<Boid*>& boids) {
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

    // Move against the others
    glm::vec3 separate(vector<Boid*>& boids) {
        glm::vec3 steer = glm::vec3(0.0f);
        int count = 0;

        for (Boid* boid : boids) {
            if (!boid || boid == this) {
                continue;
            }

            glm::vec3 diff = position - boid->position;
            float dist = glm::length(diff);
            float pairAvoidDist = avoidDist + boid->avoidDist;

            if (dist > 0.001f && dist < pairAvoidDist) {
                steer += glm::normalize(diff) / dist;
                count++;
            }
        }

        if (count == 0) {
            return steer;
        }

        steer /= (float)count;
        return steerTo(steer);
    }

    // Move along the others
    glm::vec3 align(vector<Boid*>& boids) {
        glm::vec3 avg = glm::vec3(0.0f);
        int count = 0;

        for (Boid* boid : boids) {
            if (!boid || boid == this) {
                continue;
            }

            if (glm::length(position - boid->position) < alignDist) {
                avg += boid->velocity;
                count++;
            }
        }

        if (count == 0) {
            return avg;
        }

        avg /= (float)count;
        return steerTo(avg);
    }

    // Move to the center of boids
    glm::vec3 cohere(vector<Boid*>& boids) {
        glm::vec3 center = glm::vec3(0.0f);
        int count = 0;

        for (Boid* boid : boids) {
            if (!boid || boid == this) {
                continue;
            }

            if (glm::length(position - boid->position) < cohereDist) {
                center += boid->position;
                count++;
            }
        }

        if (count == 0) {
            return center;
        }

        center /= (float)count;
        return steerTo(center - position);
    }

    // Move to the bound center (if near to boundary)
    glm::vec3 boundForce() {
        glm::vec3 force = glm::vec3(0.0f);

        if (position.x < xMin + margin) force.x += 1.0f;
        if (position.x > xMax - margin) force.x -= 1.0f;
        if (position.y < yMin + margin) force.y += 1.0f;
        if (position.y > yMax - margin) force.y -= 1.0f;
        if (position.z < zMin + margin) force.z += 1.0f;
        if (position.z > zMax - margin) force.z -= 1.0f;

        return steerTo(force);
    }

    // Move randomly
    glm::vec3 wander() {
        glm::vec3 force = glm::sphericalRand(maxForce);
        force.y *= wanderYScale;
        return force;
    }

    // Update forward smoothly (lerp)
    void updateForward(float dt) {
        if (glm::length(velocity) < 0.001f) {
            return;
        }

        glm::vec3 targetForward = glm::normalize(velocity);
        float u = glm::clamp(dt * 2.0f, 0.0f, 1.0f);
        forward = glm::normalize(glm::mix(forward, targetForward, u));
    }

    // Steering force to desired velocity(maxSpeed to dir)
    glm::vec3 steerTo(glm::vec3 dir) {
        dir.y *= steerYScale;
        if (glm::length(dir) < 0.001f) {
            return glm::vec3(0.0f);
        }

        glm::vec3 desired = glm::normalize(dir) * maxSpeed;
        return limit(desired - velocity, maxForce);
    }

    glm::vec3 limit(glm::vec3 v, float maxLength) {
        if (glm::length(v) > maxLength) {
            return glm::normalize(v) * maxLength;
        }

        return v;
    }

    glm::vec3 limitSpeed(glm::vec3 v) {
        v.y *= velYScale;
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

    /*
     * resolve overlapped position,
     * remove velocity toward overlapping direction
     */
    void resolveOverlaps(vector<Boid*>& boids) {
        for (Boid* boid : boids) {
            if (!boid || boid == this) {
                continue;
            }

            glm::vec3 diff = position - boid->position;
            float minDist = (r + boid->r) * 1.05f;
            float distSquared = glm::dot(diff, diff);
            if (distSquared >= minDist * minDist) {
                continue;
            }

            float dist = glm::sqrt(distSquared);
            glm::vec3 pushDir = dist > 0.001f ? diff / dist : forward;
            position += pushDir * (minDist - dist);

            float inSpeed = glm::dot(velocity, pushDir);
            if (inSpeed < 0.0f) {
                velocity -= pushDir * inSpeed;
            }
        }
    }

    // reflection at the boundary
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
