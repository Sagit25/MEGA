#define GLM_ENABLE_EXPERIMENTAL

#include "volcano_trajectory.h"

#include <cmath>
#include <glm/gtc/constants.hpp>

namespace volcano {

// Set of trajectory functions for Toothless and the dragon flight sequence
const glm::vec3 fireSceneOffset = glm::vec3(-4.0f, 0.0f, -45.0f);
const float flightSpeed = 1.5f;

glm::vec3 rotateAroundX(const glm::vec3& value, float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    return glm::vec3(
        value.x,
        value.y * c - value.z * s,
        value.y * s + value.z * c
    );
}

glm::mat4 getToothlessFlightModelMatrix(float time)
{
    const float straightDuration = 10.0f / 1.5f;
    const float turnDuration = 10.0f;
    const float pi = glm::pi<float>();
    const float radiusX = 28.0f;
    const float radiusZ = 10.0f;
    const float straightStartBackOffset = 10.0f;
    const float turnStartAngle = pi * 0.5f;
    const float bankRoll = glm::radians(14.0f);

    glm::vec3 center = glm::vec3(0.0f, 2.0f, 0.5f) + fireSceneOffset;
    glm::vec3 turnStartPosition = center + rotateAroundX(
        glm::vec3(std::cos(turnStartAngle) * radiusX, 0.0f, std::sin(turnStartAngle) * radiusZ),
        bankRoll
    );
    glm::vec3 position;
    glm::vec3 tangent;
    float roll = bankRoll;

    if (time < straightDuration) {
        float progress = time / straightDuration;
        float startX = turnStartPosition.x + radiusX + straightStartBackOffset;
        position = glm::vec3(
            startX - progress * (startX - turnStartPosition.x),
            turnStartPosition.y,
            turnStartPosition.z
        );
        tangent = glm::vec3(-(startX - turnStartPosition.x) / straightDuration, 0.0f, 0.0f);
    }
    else {
        float turnTime = time - straightDuration;
        float turnProgress = turnTime / turnDuration;
        float angle = turnStartAngle + turnProgress * pi;
        glm::vec3 orbitOffset = rotateAroundX(
            glm::vec3(
                std::cos(angle) * radiusX,
                std::sin(angle * 2.0f) * 0.7f,
                std::sin(angle) * radiusZ
            ),
            bankRoll
        );

        position = center + orbitOffset;

        tangent = rotateAroundX(
            glm::vec3(
                -std::sin(angle) * radiusX,
                std::cos(angle * 2.0f) * 0.45f,
                std::cos(angle) * radiusZ
            ),
            bankRoll
        );
        roll = bankRoll;
    }

    glm::vec3 forward = glm::normalize(tangent);
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));
    glm::vec3 up = glm::normalize(glm::cross(forward, right));

    glm::mat4 direction = glm::mat4(1.0f);
    direction[0] = glm::vec4(right, 0.0f);
    direction[1] = glm::vec4(up, 0.0f);
    direction[2] = glm::vec4(forward, 0.0f);

    glm::mat4 glideRoll = glm::rotate(glm::mat4(1.0f), roll, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 modelCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    transform = transform * direction * modelCorrection * glideRoll;
    transform = glm::scale(transform, glm::vec3(0.05f));
    return transform;
}

DragonFlightPose getDragonFlightPose(float time)
{
    const float straightDuration = 12.0f * flightSpeed / 2.0f;
    const float turnDuration = 14.0f;
    const float pi = glm::pi<float>();
    const float radiusX = 27.3f;
    const float radiusZ = 21.6f;
    const float straightStartBackOffset = 10.0f;
    const float turnStartAngle = pi * 0.5f;
    const float bankRoll = glm::radians(12.0f);
    const float dragonYOffset = 0.5f;
    const float zWaveFrequency = 2.35f;
    const float zWaveAmplitude = 1.5f;
    const float yWaveFrequency = 1.65f;
    const float yWaveAmplitude = 0.85f;
    const float yDriftFrequency = 0.7f;
    const float yDriftAmplitude = 0.25f;

    const float toothlessTurnRadiusZ = 10.0f;
    const float toothlessBankRoll = glm::radians(14.0f);
    glm::vec3 toothlessTurnStartPosition =
        glm::vec3(0.0f, -1.0f, 0.5f) + fireSceneOffset
        + rotateAroundX(glm::vec3(0.0f, 0.0f, toothlessTurnRadiusZ), toothlessBankRoll);
    glm::vec3 turnStartPosition = toothlessTurnStartPosition + glm::vec3(0.0f, dragonYOffset, 0.0f);
    glm::vec3 center = turnStartPosition - rotateAroundX(
        glm::vec3(std::cos(turnStartAngle) * radiusX, 0.0f, std::sin(turnStartAngle) * radiusZ),
        bankRoll
    );

    if (time < straightDuration) {
        float progress = time / straightDuration;
        float startX = turnStartPosition.x + radiusX + straightStartBackOffset;
        glm::vec3 position(
            startX - progress * (startX - turnStartPosition.x),
            turnStartPosition.y,
            turnStartPosition.z
        );
        glm::vec3 tangent(-(startX - turnStartPosition.x) / straightDuration, 0.0f, 0.0f);
        return { position, tangent, turnStartAngle };
    }

    float turnTime = time - straightDuration;
    float turnProgress = turnTime / turnDuration;
    float angle = turnStartAngle + turnProgress * pi;
    float yWaveStart =
        std::sin(turnStartAngle * yWaveFrequency + 0.4f) * yWaveAmplitude
        + std::sin(turnStartAngle * yDriftFrequency + 1.1f) * yDriftAmplitude;
    float zWaveStart = std::sin(turnStartAngle * zWaveFrequency + 0.8f) * zWaveAmplitude;
    float yWave =
        std::sin(angle * yWaveFrequency + 0.4f) * yWaveAmplitude
        + std::sin(angle * yDriftFrequency + 1.1f) * yDriftAmplitude;
    float zWave = std::sin(angle * zWaveFrequency + 0.8f) * zWaveAmplitude;
    glm::vec3 orbitOffset = rotateAroundX(
        glm::vec3(
            std::cos(angle) * radiusX,
            yWave - yWaveStart,
            std::sin(angle) * radiusZ + zWave - zWaveStart
        ),
        bankRoll
    );
    glm::vec3 position = center + orbitOffset;

    glm::vec3 tangent = rotateAroundX(
        glm::vec3(
            -std::sin(angle) * radiusX,
            std::cos(angle * yWaveFrequency + 0.4f) * yWaveFrequency * yWaveAmplitude
                + std::cos(angle * yDriftFrequency + 1.1f) * yDriftFrequency * yDriftAmplitude,
            std::cos(angle) * radiusZ
                + std::cos(angle * zWaveFrequency + 0.8f) * zWaveFrequency * zWaveAmplitude
        ),
        bankRoll
    );

    return { position, tangent, angle };
}

glm::vec3 getDragonFlightVelocity(float time)
{
    return glm::normalize(getDragonFlightPose(time).tangent);
}

glm::mat4 getDragonFlightModelMatrix(float time)
{
    DragonFlightPose pose = getDragonFlightPose(time);
    glm::vec3 forward = glm::normalize(pose.tangent);
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));
    glm::vec3 up = glm::normalize(glm::cross(forward, right));

    glm::mat4 direction = glm::mat4(1.0f);
    direction[0] = glm::vec4(right, 0.0f);
    direction[1] = glm::vec4(up, 0.0f);
    direction[2] = glm::vec4(forward, 0.0f);

    float roll = glm::radians(12.0f);
    glm::mat4 glideRoll = glm::rotate(glm::mat4(1.0f), roll, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 modelCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, pose.position);
    transform = transform * direction * modelCorrection * glideRoll;
    transform = glm::scale(transform, glm::vec3(0.09f));
    return transform;
}

} // namespace volcano
