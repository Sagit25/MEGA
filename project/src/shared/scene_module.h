#ifndef SCENE_MODULE_H
#define SCENE_MODULE_H

#include <GLFW/glfw3.h>

struct SceneCameraPose {
    float positionX;
    float positionY;
    float positionZ;
    float yaw;
    float pitch;
    float zoom;
};

struct SceneModule {
    const char* name;
    void (*init)(GLFWwindow* window);
    void (*onEnter)(GLFWwindow* window);
    void (*renderFrame)(GLFWwindow* window);
    void (*renderFadeForeground)(GLFWwindow* window);
    void (*onFramebufferSize)(GLFWwindow* window, int width, int height);
    void (*onMouse)(GLFWwindow* window, double xpos, double ypos);
    void (*onScroll)(GLFWwindow* window, double xoffset, double yoffset);
    void (*getCameraPose)(SceneCameraPose* pose);
    void (*getDefaultCameraPose)(SceneCameraPose* pose);
    void (*setCameraPose)(const SceneCameraPose& pose);
};

#endif
