#include "shared/scene_transition_effect.h"

#include <algorithm>

#include <glad/glad.h>

#include "shared/shader.h"

namespace scene_fade {

enum class Phase {
    None,
    FadeOut,
    FadeIn,
};

static const float outSec = 1.0f;
static const float inSec = 1.0f;

static Shader shader;
static unsigned int vao = 0;
static unsigned int vbo = 0;
static Phase phase = Phase::None;
static float startTime = 0.0f;
static int nextIdx = -1;

void init()
{
    shader = Shader("../shaders/shared/fade_overlay.vs", "../shaders/shared/fade_overlay.fs");

    const float quadVertices[] = {
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

bool active()
{
    return phase != Phase::None;
}

void start(int sceneIdx, float now)
{
    if (active()) {
        return;
    }

    nextIdx = sceneIdx;
    phase = Phase::FadeOut;
    startTime = now;
}

int update(float now)
{
    if (!active()) {
        return -1;
    }

    float elapsed = now - startTime;

    if (phase == Phase::FadeOut && elapsed >= outSec) {
        int sceneIdx = nextIdx;
        phase = Phase::FadeIn;
        startTime = now;
        return sceneIdx;
    }

    if (phase == Phase::FadeIn && elapsed >= inSec) {
        phase = Phase::None;
        nextIdx = -1;
    }

    return -1;
}

float alpha(float now)
{
    if (!active()) {
        return 0.0f;
    }

    float elapsed = now - startTime;

    if (phase == Phase::FadeOut) {
        return std::min(elapsed / outSec, 1.0f);
    }
    if (phase == Phase::FadeIn) {
        return 1.0f - std::min(elapsed / inSec, 1.0f);
    }
    return 0.0f;
}

void draw(float alpha)
{
    alpha = std::max(0.0f, std::min(alpha, 1.0f));
    if (alpha <= 0.0f || vao == 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader.use();
    shader.setFloat("alpha", alpha);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

} // namespace scene_fade
