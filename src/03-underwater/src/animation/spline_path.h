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
    float len = 0.0f;

    SplinePath(
        glm::vec3 p0,
        glm::vec3 p1,
        glm::vec3 p2,
        glm::vec3 p3,
        float r,
        float len)
        : p0(p0), p1(p1), p2(p2), p3(p3), r(r), len(len), cpDist(len * 2.0f)
    {}

    /* Set unoverlapping spline path */
    SplinePath(float r, float len, std::vector<SplinePath*>& splinePaths)
        : r(r), len(len), cpDist(len * 2.0f)
    {
        randomize(splinePaths);
    }

    /*
     * Renew state of spline path
     * 1. Update speed of model
     * 2. If current spline path ends, add new control point to the end
     */
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

    /*
     * Return translation * rotation matrix
     * translation: move model position(base (0,0,0)) to spline pos
     * rotation: move model front(base (0,0,-1)) to spline direction
     */
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

    /* Check if current spline doesn't overlap with splinePaths */
    bool isAvailable(std::vector<SplinePath*>& splinePaths) {
        return isAvailableExcept(splinePaths, nullptr);
    }

private:
    /* Position bound */
    const float xMin = -40.0f;
    const float xMax = 80.0f;
    const float yMin = 0.0f;
    const float yMax = 40.0f;
    const float zMin = -50.0f;
    const float zMax = 0.0f;

    /* New control point condition */
    float cpDist = 15.0f;
    const float turnCos = cos(glm::radians(45.0f));
    const float minTurnChance = 0.01f;
    const int overlapSampleCount = 16;

    /* Speed bound */
    const float minSpeed = 0.5f;
    const float maxSpeed = 0.8f;
    const float minSpeedStayTime = 1.0f;
    const float maxSpeedStayTime = 4.0f;

    /* Speed */
    float startSpeed = 0.6f;
    float targetSpeed = 0.6f;

    /* Timer */
    const float speedChangeTime = 1.0f;
    float speedStayTime = 2.0f;
    float speedChangeTimer = 1.0f;
    float speedStayTimer = 0.0f;

    /* Random control point in given cube */
    glm::vec3 randomCP() {
        return glm::vec3(
            glm::linearRand(xMin, xMax),
            glm::linearRand(yMin, yMax),
            glm::linearRand(zMin, zMax));
    }

    /* Is the point is in the bound? */
    bool boundOK(glm::vec3 p) {
        return p.x >= xMin && p.x <= xMax
            && p.y >= yMin && p.y <= yMax
            && p.z >= zMin && p.z <= zMax;
    }

    /* 
     * Check if the direction is OK
     * If direction diff is within the fixed angle, it's ok
     * Else if direction is opposite, it's bad
     * Else, pass with low probability
     */
    bool turnOK(glm::vec3 prevCP, glm::vec3 currentCP, glm::vec3 nextCP) {
        glm::vec3 oldDir = currentCP - prevCP;
        glm::vec3 newDir = nextCP - currentCP;

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

        return glm::linearRand(0.0f, 1.0f) <= minTurnChance;
    }

    /* Set the first 4 control points */
    void randomize(std::vector<SplinePath*>& splinePaths) {
        for (int i = 0; i < 100; i++) {
            p0 = randomCP();
            p1 = p0 + glm::sphericalRand(cpDist);
            p2 = p1 + glm::sphericalRand(cpDist);
            p3 = p2 + glm::sphericalRand(cpDist);

            if (!boundOK(p1) || !boundOK(p2) || !boundOK(p3)) {
                continue;
            }

            if (!turnOK(p0, p1, p2) || !turnOK(p1, p2, p3)) {
                continue;
            }

            if (isAvailable(splinePaths)) {
                return;
            }
        }
    }

    /*
     * Update speed
     * speed change in smooth function from startSpeed to targetSpeed
     * if speed reaches targetSpeed, it stays some random time
     * then choose random next targetSpeed
     */
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

    float reflectCoord(float from, float value, float minValue, float maxValue) {
        if (value < minValue || value > maxValue) {
            value = 2.0f * from - value;
        }

        return value;
    }

    /*
     * If "p" doesn't within the bound, reflect against "from"
     * check with each axis
     */
    glm::vec3 fitBound(glm::vec3 from, glm::vec3 p) {
        p.x = reflectCoord(from.x, p.x, xMin, xMax);
        p.y = reflectCoord(from.y, p.y, yMin, yMax);
        p.z = reflectCoord(from.z, p.z, zMin, zMax);
        return p;
    }

    /* Returns b-spline pos/dir */
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

    /*
     * Check overlap between two spline paths
     * one path overlaps other when a segment of it
     * is within the other path's radius
     * approximate checking with sampling
     */
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

    /*
     * Check overlap with other paths
     * ignoredSplinePath parameter for avoiding self-overlap
     */
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

    /*
     * Find next control point
     * return immediately when find valid control point
     * after 100 tries, just retrun with last candidate
     */
    bool tryNextCP(
        std::vector<SplinePath*>& splinePaths,
        glm::vec3& nextCP)
    {
        for (int i = 0; i < 100; i++) {
            nextCP = p3 + glm::sphericalRand(cpDist);

            if (!turnOK(p2, p3, nextCP)) {
                continue;
            }

            nextCP = fitBound(p3, nextCP);
            SplinePath candidate(p1, p2, p3, nextCP, r, len);

            if (candidate.isAvailableExcept(splinePaths, this)) {
                return true;
            }
        }

        nextCP = fitBound(p3, nextCP);
        return true;
    }

    /* Discard old control point and accept new one */
    void shiftCP(glm::vec3& nextCP) {
        p0 = p1;
        p1 = p2;
        p2 = p3;
        p3 = nextCP;
    }
};

#endif
