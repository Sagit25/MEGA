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

    Boid(float r, float len, vector<Boid*>& boids)
        : r(r), len(len),
          avoidDist(r * 2.5f),
          alignDist(len * 3.0f),
          cohereDist(len * 4.0f)
    {
        randomize(boids);
    }

    /*
     * update velocity/position of object
     * based on Boids algorithm of Craig Reynolds
     */ 
    void advance(float dt, vector<Boid*>& boids) {
        glm::vec3 force = glm::vec3(0.0f);

        force += separate(boids) * 4.0f;
        force += align(boids) * 0.5f;
        force += cohere(boids) * 0.15f;
        force += wander() * 0.05f;
        force = limit(force, maxForce);

        velocity += force * dt;
        velocity = limitSpeed(velocity);

        position += velocity * dt;
        
        wrapBound();
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
    const float xMin = -40.0f;
    const float xMax = 80.0f;
    const float yMin = 0.0f;
    const float yMax = 40.0f;
    const float zMin = -50.0f;
    const float zMax = 0.0f;

    const float minSpeed = 7.0f;
    const float maxSpeed = 13.0f;
    const float maxForce = 4.0f;

    float avoidDist;
    float alignDist;
    float cohereDist;

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
        v.y *= 0.35f;
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

    // Move randomly
    glm::vec3 wander() {
        return glm::sphericalRand(maxForce);
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

    // wrap around the position
    void wrapBound() {
        wrapAxis(position.x, xMin, xMax);
        wrapAxis(position.y, yMin, yMax);
        wrapAxis(position.z, zMin, zMax);
    }

    void wrapAxis(float& value, float minValue, float maxValue) {
        float range = maxValue - minValue;
        while (value < minValue) {
            value += range;
        }
        while (value > maxValue) {
            value -= range;
        }
    }
};

#endif
