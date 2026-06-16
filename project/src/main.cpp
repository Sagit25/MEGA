#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>

#include "shared/scene_module.h"
#include "shared/scene_transition_effect.h"

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

namespace base { SceneModule getModule(); }
namespace volcano { SceneModule getModule(); }
namespace desert { SceneModule getModule(); }
namespace underwater { SceneModule getModule(); }

static std::vector<SceneModule> scenes;
static int sceneIdx = 0;

static SceneModule& scene()
{
    return scenes[sceneIdx];
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    if (!scenes.empty() && scene().onFramebufferSize) {
        scene().onFramebufferSize(window, width, height);
    }
}

static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!scenes.empty() && scene().onMouse) {
        scene().onMouse(window, xpos, ypos);
    }
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (!scenes.empty() && scene().onScroll) {
        scene().onScroll(window, xoffset, yoffset);
    }
}

static void switchScene(GLFWwindow* window, int idx)
{
    if (scenes.empty()) {
        return;
    }
    if (idx < 0 || idx >= (int)scenes.size()) {
        return;
    }

    SceneModule& from = scene();
    SceneCameraPose pose{};
    SceneCameraPose fromDef{};
    bool gotPose = false;
    bool gotFromDef = false;
    if (from.getCameraPose) {
        from.getCameraPose(&pose);
        gotPose = true;
    }
    if (from.getDefaultCameraPose) {
        from.getDefaultCameraPose(&fromDef);
        gotFromDef = true;
    }

    sceneIdx = idx;
    if (scene().onEnter) {
        scene().onEnter(window);
    }
    if (gotPose && scene().setCameraPose) {
        SceneCameraPose toPose = pose;
        SceneCameraPose toDef{};
        if (gotFromDef && scene().getDefaultCameraPose) {
            scene().getDefaultCameraPose(&toDef);
            toPose.positionX = toDef.positionX + (pose.positionX - fromDef.positionX);
            toPose.positionY = toDef.positionY + (pose.positionY - fromDef.positionY);
            toPose.positionZ = toDef.positionZ + (pose.positionZ - fromDef.positionZ);
            toPose.yaw = toDef.yaw + (pose.yaw - fromDef.yaw);
            toPose.pitch = toDef.pitch + (pose.pitch - fromDef.pitch);
            toPose.zoom = toDef.zoom + (pose.zoom - fromDef.zoom);
        }
        scene().setCameraPose(toPose);
    }
}

static void nextScene()
{
    if (scenes.empty() || scene_fade::active()) {
        return;
    }

    int nextIdx = (sceneIdx + 1) % (int)scenes.size();
    scene_fade::start(nextIdx, (float)glfwGetTime());
}

static void tickFade(GLFWwindow* window)
{
    int nextIdx = scene_fade::update((float)glfwGetTime());
    if (nextIdx >= 0) {
        switchScene(window, nextIdx);
    }
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        nextScene();
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
    scene_fade::init();

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

    sceneIdx = 0;
    if (scene().onEnter) {
        scene().onEnter(window);
    }

    while (!glfwWindowShouldClose(window)) {
        tickFade(window);
        if (scene().renderFrame) {
            scene().renderFrame(window);
        }
        float a = scene_fade::alpha((float)glfwGetTime());
        scene_fade::draw(a);
        if (a > 0.0f && scene().renderFadeForeground) {
            scene().renderFadeForeground(window);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
