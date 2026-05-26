#ifndef SPLINE_PATH_H
#define SPLINE_PATH_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <utility>
#include <vector>

class SplinePath {
public:
    glm::vec3 p0;
    glm::vec3 p1;
    glm::vec3 p2;
    glm::vec3 p3;
    float t = 0.0f;
    float v = 0.2f;
    float r = 0.5f;

    SplinePath(
        glm::vec3 p0,
        glm::vec3 p1,
        glm::vec3 p2,
        glm::vec3 p3,
        float r)
        : p0(p0), p1(p1), p2(p2), p3(p3), r(r)
    {}

    SplinePath(float r, std::vector<SplinePath*>& splinePaths)
        : r(r)
    {
        randomize(splinePaths);
    }

    void advance(float dt, std::vector<SplinePath*>& splinePaths) {
        updateSpeed(dt);
        t += v * dt;

        if (t >= 1.0f) {
            glm::vec3 nextCP;
            if (tryNextCP(splinePaths, nextCP)) {
                shiftCP(nextCP);
                t = glm::fract(t);
                return;
            }

            t = 1.0f;
        }
    }

    glm::mat4 calculateBSpline() {
        std::pair<glm::vec3, glm::vec3> spline = calculateBSplineAt(t);
        glm::vec3 pos = spline.first;
        glm::vec3 forward = spline.second;
        float yaw = atan2(forward.x, forward.z);
        float pitch = asin(glm::clamp(-forward.y, -1.0f, 1.0f));

        glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotation = yawRotation * pitchRotation;
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), pos);

        return translation * rotation;
    }

    bool isAvailable(std::vector<SplinePath*>& splinePaths) {
        return isAvailableExcept(splinePaths, nullptr);
    }

private:
    const float minSpeed = 0.2f;
    const float maxSpeed = 0.4f;
    const float speedChangeTime = 1.0f;
    const float minSpeedStayTime = 1.0f;
    const float maxSpeedStayTime = 4.0f;
    const float turnCos = 0.707f;
    const float minTurnChance = 0.05f;
    const float minCPDist = 3.0f;

    float startSpeed = 0.2f;
    float targetSpeed = 0.2f;

    float speedChangeTimer = 1.0f;
    float speedStayTimer = 0.0f;
    float speedStayTime = 2.0f;

    const int overlapSampleCount = 16;

    glm::vec3 randomCP() {
        return glm::vec3(
            glm::linearRand(10.0f, 50.0f),
            glm::linearRand(-3.0f, 10.0f),
            glm::linearRand(-50.0f, -10.0f));
    }

    void randomize(std::vector<SplinePath*>& splinePaths) {
        for (int i = 0; i < 30; i++) {
            p0 = randomCP();
            p1 = randomCP();
            p2 = randomCP();
            p3 = randomCP();

            if (cpOK() && isAvailable(splinePaths)) {
                return;
            }
        }
    }

    void updateSpeed(float dt) {
        if (speedChangeTimer < speedChangeTime) {
            speedChangeTimer += dt;
            float u = glm::clamp(speedChangeTimer / speedChangeTime, 0.0f, 1.0f);
            float smoothU = u * u * (3.0f - 2.0f * u);
            v = startSpeed + (targetSpeed - startSpeed) * smoothU;
            return;
        }

        speedStayTimer += dt;
        if (speedStayTimer >= speedStayTime) {
            startSpeed = v;
            speedChangeTimer = 0.0f;
            speedStayTimer = 0.0f;

            targetSpeed = glm::linearRand(minSpeed, maxSpeed);
            speedStayTime = glm::linearRand(minSpeedStayTime, maxSpeedStayTime);
        }
    }

    std::pair<glm::vec3, glm::vec3> calculateBSplineAt(float pathT) {
        float t2 = pathT * pathT;
        float t3 = t2 * pathT;

        float b0 = (-t3 + 3.0f * t2 - 3.0f * pathT + 1.0f) / 6.0f;
        float b1 = (3.0f * t3 - 6.0f * t2 + 4.0f) / 6.0f;
        float b2 = (-3.0f * t3 + 3.0f * t2 + 3.0f * pathT + 1.0f) / 6.0f;
        float b3 = t3 / 6.0f;

        float db0 = (-3.0f * t2 + 6.0f * pathT - 3.0f) / 6.0f;
        float db1 = (9.0f * t2 - 12.0f * pathT) / 6.0f;
        float db2 = (-9.0f * t2 + 6.0f * pathT + 3.0f) / 6.0f;
        float db3 = (3.0f * t2) / 6.0f;

        glm::vec3 pos = b0 * p0 + b1 * p1 + b2 * p2 + b3 * p3;
        glm::vec3 dir = db0 * p0 + db1 * p1 + db2 * p2 + db3 * p3;

        if (glm::length(dir) < 0.001f) {
            dir = glm::vec3(1.0f, 0.0f, 0.0f);
        } else {
            dir = glm::normalize(dir);
        }

        return std::make_pair(pos, dir);
    }

    bool tryNextCP(
        std::vector<SplinePath*>& splinePaths,
        glm::vec3& nextCP)
    {
        for (int i = 0; i < 30; i++) {
            nextCP = randomCP();
            if (!turnOK(nextCP)) {
                continue;
            }

            SplinePath candidate(p1, p2, p3, nextCP, r);

            if (candidate.cpOK() && candidate.isAvailableExcept(splinePaths, this)) {
                return true;
            }
        }

        return false;
    }

    bool cpOK() {
        glm::vec3 d01 = p0 - p1;
        glm::vec3 d12 = p1 - p2;
        glm::vec3 d23 = p2 - p3;

        return glm::dot(d01, d01) >= minCPDist * minCPDist
            && glm::dot(d12, d12) >= minCPDist * minCPDist
            && glm::dot(d23, d23) >= minCPDist * minCPDist;
    }

    bool turnOK(glm::vec3& nextCP) {
        glm::vec3 oldDir = p2 - p1;
        glm::vec3 newDir = nextCP - p3;

        if (glm::length(oldDir) < 0.001f || glm::length(newDir) < 0.001f) {
            return true;
        }

        oldDir = glm::normalize(oldDir);
        newDir = glm::normalize(newDir);

        float dotDir = glm::clamp(glm::dot(oldDir, newDir), -1.0f, 1.0f);
        if (dotDir >= turnCos) {
            return true;
        }
        if (dotDir <= -turnCos) {
            return false;
        }

        return (glm::linearRand(0.0f, 1.0f) <= minTurnChance);
    }

    bool isAvailableExcept(
        std::vector<SplinePath*>& splinePaths,
        SplinePath* ignoredSplinePath)
    {
        for (SplinePath* splinePath : splinePaths) {
            if (!splinePath || splinePath == ignoredSplinePath) {
                continue;
            }

            if (isOverlapping(*splinePath)) {
                return false;
            }
        }

        return true;
    }

    bool isOverlapping(SplinePath& other) {
        const float radiusSum = r + other.r;

        for (int i = 0; i <= overlapSampleCount; i++) {
            float tA = (float)i / overlapSampleCount;
            glm::vec3 a = calculateBSplineAt(tA).first;

            for (int j = 0; j <= overlapSampleCount; j++) {
                float tB = (float)j / overlapSampleCount;
                glm::vec3 b = other.calculateBSplineAt(tB).first;
                glm::vec3 diff = a - b;

                if (glm::dot(diff, diff) <= radiusSum * radiusSum) {
                    return true;
                }
            }
        }

        return false;
    }

    void shiftCP(glm::vec3& nextCP) {
        p0 = p1;
        p1 = p2;
        p2 = p3;
        p3 = nextCP;
    }
};

#endif
