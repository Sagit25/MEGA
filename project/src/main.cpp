#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <vector>

#include "shared/scene_module.h"
#include "shared/shader.h"

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

namespace base { SceneModule getModule(); }
namespace volcano { SceneModule getModule(); }
namespace desert { SceneModule getModule(); }
namespace underwater { SceneModule getModule(); }

static std::vector<SceneModule> scenes;
static int activeSceneIndex = 0;
static Shader fadeOverlayShader;
static unsigned int fadeOverlayVAO = 0;
static unsigned int fadeOverlayVBO = 0;

enum class SceneTransitionPhase {
    None,
    FadeOut,
    FadeIn,
};

static const float SCENE_FADE_OUT_SECONDS = 1.0f;
static const float SCENE_FADE_IN_SECONDS = 1.0f;
static SceneTransitionPhase sceneTransitionPhase = SceneTransitionPhase::None;
static float sceneTransitionPhaseStart = 0.0f;
static int pendingSceneIndex = -1;

static SceneModule& activeScene()
{
    return scenes[activeSceneIndex];
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    if (!scenes.empty() && activeScene().onFramebufferSize) {
        activeScene().onFramebufferSize(window, width, height);
    }
}

static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!scenes.empty() && activeScene().onMouse) {
        activeScene().onMouse(window, xpos, ypos);
    }
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (!scenes.empty() && activeScene().onScroll) {
        activeScene().onScroll(window, xoffset, yoffset);
    }
}

static void initFadeOverlay()
{
    fadeOverlayShader = Shader("../shaders/shared/fade_overlay.vs", "../shaders/shared/fade_overlay.fs");

    const float quadVertices[] = {
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
    };

    glGenVertexArrays(1, &fadeOverlayVAO);
    glGenBuffers(1, &fadeOverlayVBO);
    glBindVertexArray(fadeOverlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fadeOverlayVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void drawFadeOverlay(float alpha)
{
    alpha = std::max(0.0f, std::min(alpha, 1.0f));
    if (alpha <= 0.0f || fadeOverlayVAO == 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    fadeOverlayShader.use();
    fadeOverlayShader.setFloat("alpha", alpha);
    glBindVertexArray(fadeOverlayVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

static void switchToScene(GLFWwindow* window, int targetSceneIndex)
{
    if (scenes.empty()) {
        return;
    }
    if (targetSceneIndex < 0 || targetSceneIndex >= static_cast<int>(scenes.size())) {
        return;
    }

    SceneModule& sourceScene = activeScene();
    SceneCameraPose cameraPose{};
    SceneCameraPose sourceDefaultPose{};
    bool hasCameraPose = false;
    bool hasSourceDefaultPose = false;
    if (sourceScene.getCameraPose) {
        sourceScene.getCameraPose(&cameraPose);
        hasCameraPose = true;
    }
    if (sourceScene.getDefaultCameraPose) {
        sourceScene.getDefaultCameraPose(&sourceDefaultPose);
        hasSourceDefaultPose = true;
    }

    activeSceneIndex = targetSceneIndex;
    if (activeScene().onEnter) {
        activeScene().onEnter(window);
    }
    if (hasCameraPose && activeScene().setCameraPose) {
        SceneCameraPose targetPose = cameraPose;
        SceneCameraPose targetDefaultPose{};
        if (hasSourceDefaultPose && activeScene().getDefaultCameraPose) {
            activeScene().getDefaultCameraPose(&targetDefaultPose);
            targetPose.positionX = targetDefaultPose.positionX + (cameraPose.positionX - sourceDefaultPose.positionX);
            targetPose.positionY = targetDefaultPose.positionY + (cameraPose.positionY - sourceDefaultPose.positionY);
            targetPose.positionZ = targetDefaultPose.positionZ + (cameraPose.positionZ - sourceDefaultPose.positionZ);
            targetPose.yaw = targetDefaultPose.yaw + (cameraPose.yaw - sourceDefaultPose.yaw);
            targetPose.pitch = targetDefaultPose.pitch + (cameraPose.pitch - sourceDefaultPose.pitch);
            targetPose.zoom = targetDefaultPose.zoom + (cameraPose.zoom - sourceDefaultPose.zoom);
        }
        activeScene().setCameraPose(targetPose);
    }
}

static bool isSceneTransitionActive()
{
    return sceneTransitionPhase != SceneTransitionPhase::None;
}

static void requestNextSceneTransition()
{
    if (scenes.empty() || isSceneTransitionActive()) {
        return;
    }

    pendingSceneIndex = (activeSceneIndex + 1) % static_cast<int>(scenes.size());
    sceneTransitionPhase = SceneTransitionPhase::FadeOut;
    sceneTransitionPhaseStart = static_cast<float>(glfwGetTime());
}

static void updateSceneTransition(GLFWwindow* window)
{
    if (!isSceneTransitionActive()) {
        return;
    }

    float now = static_cast<float>(glfwGetTime());
    float elapsed = now - sceneTransitionPhaseStart;

    if (sceneTransitionPhase == SceneTransitionPhase::FadeOut && elapsed >= SCENE_FADE_OUT_SECONDS) {
        switchToScene(window, pendingSceneIndex);
        sceneTransitionPhase = SceneTransitionPhase::FadeIn;
        sceneTransitionPhaseStart = now;
        return;
    }

    if (sceneTransitionPhase == SceneTransitionPhase::FadeIn && elapsed >= SCENE_FADE_IN_SECONDS) {
        sceneTransitionPhase = SceneTransitionPhase::None;
        pendingSceneIndex = -1;
    }
}

static float getSceneTransitionFadeAlpha()
{
    if (!isSceneTransitionActive()) {
        return 0.0f;
    }

    float now = static_cast<float>(glfwGetTime());
    float elapsed = now - sceneTransitionPhaseStart;

    if (sceneTransitionPhase == SceneTransitionPhase::FadeOut) {
        return std::min(elapsed / SCENE_FADE_OUT_SECONDS, 1.0f);
    }
    if (sceneTransitionPhase == SceneTransitionPhase::FadeIn) {
        return 1.0f - std::min(elapsed / SCENE_FADE_IN_SECONDS, 1.0f);
    }
    return 0.0f;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        requestNextSceneTransition();
    }
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    initFadeOverlay();

    scenes = {
        base::getModule(),
        volcano::getModule(),
        desert::getModule(),
        underwater::getModule(),
    };

    for (SceneModule& scene : scenes) {
        if (scene.init) {
            scene.init(window);
        }
    }

    activeSceneIndex = 0;
    if (activeScene().onEnter) {
        activeScene().onEnter(window);
    }

    while (!glfwWindowShouldClose(window)) {
        updateSceneTransition(window);
        if (activeScene().renderFrame) {
            activeScene().renderFrame(window);
        }
        float fadeAlpha = getSceneTransitionFadeAlpha();
        drawFadeOverlay(fadeAlpha);
        if (fadeAlpha > 0.0f && activeScene().renderFadeForeground) {
            activeScene().renderFadeForeground(window);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
