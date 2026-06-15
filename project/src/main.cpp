#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>

#include "shared/scene_module.h"

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

namespace base { SceneModule getModule(); }
namespace volcano { SceneModule getModule(); }
namespace desert { SceneModule getModule(); }
namespace underwater { SceneModule getModule(); }

static std::vector<SceneModule> scenes;
static int activeSceneIndex = 0;

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

static void switchToNextScene(GLFWwindow* window)
{
    if (scenes.empty()) {
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

    activeSceneIndex = (activeSceneIndex + 1) % static_cast<int>(scenes.size());
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
    std::cout << "Active scene: " << activeScene().name << std::endl;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        switchToNextScene(window);
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
    std::cout << "Active scene: " << activeScene().name << std::endl;

    while (!glfwWindowShouldClose(window)) {
        if (activeScene().renderFrame) {
            activeScene().renderFrame(window);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
