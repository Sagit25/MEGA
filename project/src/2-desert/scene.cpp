#define GLM_ENABLE_EXPERIMENTAL
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shared/shader.h"
#include "shared/opengl_utils.h"
#include <iostream>
#include <vector>
#include <functional>
#include "shared/camera.h"
#include "shared/texture.h"
#include "shared/texture_cube.h"
#include "shared/model.h"
#include "shared/mesh.h"
#include <algorithm>
#include <cmath>
#include "shared/scene_module.h"
#include "shared/fade_foreground.h"

namespace desert {

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void initAccumTargets(int width, int height);
void resizeAccumTargets(int width, int height);
void initSceneTarget(int width, int height);
void resizeSceneTarget(int width, int height);
void getCameraPose(SceneCameraPose* pose);
void getDefaultCameraPose(SceneCameraPose* pose);
void setCameraPose(const SceneCameraPose& pose);
void renderFadeForeground(GLFWwindow* window);
float getGroundTempAtTime(float renderTime);
float getTemperatureRenderTime(float elapsedTime);
float getHazeAmountAtTime(float renderTime, float currentGroundTemp);
void drawModelEntities(Shader& shader, const std::vector<Entity*>& entities);
glm::vec3 getDirectionalLightDir(float azimuth, float elevation);

bool isWindowed = true;
bool isKeyboardDone[1024] = { 0 };

static bool initialized = false;
static std::function<void(GLFWwindow*)> renderFrameImpl;
static std::function<void(GLFWwindow*)> onEnterImpl;
static Shader* fadeForegroundShader = nullptr;
static DepthMapTexture* fadeForegroundDepth = nullptr;
static std::vector<Entity*>* fadeForegroundEntities = nullptr;
static glm::vec3 fadeForegroundHousePosition = glm::vec3(0.0f);
static glm::vec3 fadeForegroundLightDir = glm::vec3(0.0f, -1.0f, 0.0f);

// setting
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;

int framebufferWidth = SCR_WIDTH;
int framebufferHeight = SCR_HEIGHT;

// camera
const glm::vec3 CAMERA_INITIAL_POS = glm::vec3(0.0f, 0.0f, 3.0f);
const float CAMERA_INITIAL_YAW = -90.0f;
const float CAMERA_INITIAL_PITCH = 0.0f;
Camera camera(CAMERA_INITIAL_POS, glm::vec3(0.0f, 1.0f, 0.0f), CAMERA_INITIAL_YAW, CAMERA_INITIAL_PITCH);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 1.0f / 60.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

// Optional accumulation targets for Figure 1(h).
unsigned int accumFBO = 0;
unsigned int accumTex[2] = { 0, 0 };
unsigned int sceneFBO = 0;
unsigned int sceneColorTexture = 0;

// For temperature
float groundTemp = 20.0f;
float skyTemp = 20.0f;
float tempBefore = groundTemp;
float grouondTemp_Initial = 100.0f;

constexpr float TEMPERATURE_INTRO_HOLD_SECONDS = 6.0f;
constexpr float TEMPERATURE_ANIMATION_SECONDS = 20.0f;
constexpr float TEMPERATURE_OUTRO_HOLD_SECONDS = 6.0f;
constexpr float TEMPERATURE_TOTAL_SECONDS =
    TEMPERATURE_INTRO_HOLD_SECONDS + TEMPERATURE_ANIMATION_SECONDS + TEMPERATURE_OUTRO_HOLD_SECONDS;

float getGroundTempAtTime(float renderTime)
{
    const float maximumGroundTemp = 210.0f;
    const float minimumGroundTemp = 20.0f;

    float heatT = 1.0f - glm::clamp(renderTime / TEMPERATURE_ANIMATION_SECONDS, 0.0f, 1.0f);
    return minimumGroundTemp + (maximumGroundTemp - minimumGroundTemp) * heatT;
}

float getTemperatureRenderTime(float elapsedTime)
{
    float animationElapsed = glm::clamp(
        elapsedTime - TEMPERATURE_INTRO_HOLD_SECONDS,
        0.0f,
        TEMPERATURE_ANIMATION_SECONDS
    );
    return TEMPERATURE_ANIMATION_SECONDS - animationElapsed;
}

float getHazeAmountAtTime(float renderTime, float currentGroundTemp)
{
    const float countdownStartTime = 15.0f;
    const float countdownMaxTime = 5.0f;
    float countdownTime = glm::clamp(renderTime, 0.0f, countdownStartTime);
    float ramp = (countdownStartTime - countdownTime) / std::max(countdownStartTime - countdownMaxTime, 0.001f);
    ramp = glm::clamp(ramp, 0.0f, 1.0f);

    float hasHeat = currentGroundTemp > skyTemp + 0.001f ? 1.0f : 0.0f;
    return hasHeat * ramp;
}

glm::vec3 getDirectionalLightDir(float azimuth, float elevation)
{
    float dirX = -std::cos(glm::radians(elevation)) * std::cos(glm::radians(azimuth));
    float dirY = -std::sin(glm::radians(elevation));
    float dirZ = -std::cos(glm::radians(elevation)) * std::sin(glm::radians(azimuth));
    return glm::normalize(glm::vec3(dirX, dirY, dirZ));
}

void drawModelEntities(Shader& shader, const std::vector<Entity*>& entities)
{
    for (Entity* entity : entities) {
        if (!entity || !entity->model) {
            continue;
        }

        shader.setMat4("world", entity->getModelMatrix());

        for (const SubMesh& subMesh : entity->model->subMeshes) {
            shader.setFloat("useDiffuseMap", subMesh.diffuse ? 1.0f : 0.0f);
            shader.setFloat("useSpecularMap", 0.0f);
            shader.setFloat("useNormalMap", 0.0f);
            shader.setVec3("baseColor", subMesh.baseColor);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, subMesh.diffuse ? subMesh.diffuse->ID : Texture::GetDummyTexture());

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());

            glBindVertexArray(subMesh.mesh.VAO);
            glDrawElements(GL_TRIANGLES, subMesh.mesh.indices.size(), GL_UNSIGNED_INT, 0);
        }
    }
}

void drawShadowModelEntities(Shader& shader, const std::vector<Entity*>& entities)
{
    for (Entity* entity : entities) {
        if (!entity || !entity->model) {
            continue;
        }

        shader.setMat4("model", entity->getModelMatrix());

        for (const SubMesh& subMesh : entity->model->subMeshes) {
            glBindVertexArray(subMesh.mesh.VAO);
            glDrawElements(GL_TRIANGLES, subMesh.mesh.indices.size(), GL_UNSIGNED_INT, 0);
        }
    }
}

void init(GLFWwindow* window)
{
    if (initialized) return;
    initialized = true;
    camera.MovementSpeed = 0.2f;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    // configure global opengl state
    // -----------------------------
    // glEnable(GL_DEPTH_TEST);

    // build and compile our shader program
    // ------------------------------------
    static Shader rayTracingShader("../shaders/2-desert/shader_ray_tracing.vs", "../shaders/2-desert/shader_ray_tracing.fs");
    static Shader heatHazeShader("../shaders/2-desert/shader_ray_tracing.vs", "../shaders/2-desert/shader_heat_haze.fs");
    static Shader lightingShader("../shaders/shared/shader_lighting.vs", "../shaders/shared/shader_lighting.fs");
    static Shader shadowShader("../shaders/shared/shadow.vs", "../shaders/shared/shadow.fs");

    std::vector<float> quad_data({
        // positions         // uvs
        1.0f, 1.0f, 0.0f,  1.0f, 1.0f,  // top right
        1.0f, -1.0f, 0.0f,  1.0f, 0.0f,  // top left
        -1.0f,  -1.0f, 0.0f, 0.0f, 0.0f,   // bottom left
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f  // bottom right
        });
    std::vector<unsigned int> quad_indices_vec({ 0,1,3,1,2,3 });
    std::vector<unsigned int> attrib_sizes({ 3, 2 });

    static VAO* quad = getVAOFromAttribData(quad_data, attrib_sizes, quad_indices_vec);
    initSceneTarget(framebufferWidth, framebufferHeight);

    // Original skybox data (works well)
    //std::vector<std::string> faces
    //{
    //    "../resources/2-desert/skybox/right.jpg",
    //    "../resources/2-desert/skybox/left.jpg",
    //    "../resources/2-desert/skybox/top.jpg",
    //    "../resources/2-desert/skybox/bottom.jpg",
    //    "../resources/2-desert/skybox/front.jpg",
    //    "../resources/2-desert/skybox/back.jpg"
    //};
    std::vector<std::string> faces
    {
        "../resources/2-desert/desert_day/sky_posx.jpg",
        "../resources/2-desert/desert_day/sky_negx.jpg",
        "../resources/2-desert/desert_day/sky_posy.jpg",
        "../resources/2-desert/desert_day/sky_negy.jpg",
        "../resources/2-desert/desert_day/sky_posz.jpg",
        "../resources/2-desert/desert_day/sky_negz.jpg"
    };
    static CubemapTexture skyboxTexture = CubemapTexture(faces);

    


    // [Project] Texture added
    static Texture objectTex("../resources/2-desert/pyramid/sandstone_diff.jpg");
    static Texture groundTex("../resources/2-desert/pyramid/desert_sand_floor.jpg");

    static Model houseModel("../resources/0-base/room/Warehouse.obj");
    static Model fireExtModel("../resources/0-base/FireExt/FireExt.obj");
    fireExtModel.setDiffuse("../resources/0-base/FireExt/FireExt_d.jpg");
    fireExtModel.setSpecular("../resources/0-base/FireExt/FireExt_s.jpg");
    fireExtModel.setNormal("../resources/0-base/FireExt/FireExt_n.jpg");
    static Model sofaModel("../resources/0-base/sofa/sofa.obj");
    static Model tableModel("../resources/0-base/table/Center Table.obj");

    static std::vector<Entity*> houseEntities;
    houseEntities.clear();
    fadeForegroundEntities = &houseEntities;
    const glm::vec3 mainCameraStart = glm::vec3(0.0f, 1.5f, 0.5f);
    const glm::vec3 mainSceneOffset = glm::vec3(-2.0f, 0.0f, 4.0f);
    const glm::vec3 mainHousePosition = glm::vec3(2.0f, 0.0f, -4.0f) + mainSceneOffset;
    static const glm::vec3 housePosition = camera.Position + (mainHousePosition - mainCameraStart);
    const float furnitureTurnY = 180.0f;
    auto mapMainPosition = [mainHousePosition](glm::vec3 mainPosition) {
        return housePosition + (mainPosition - mainHousePosition);
    };
    auto rotateInHouse = [](glm::vec3 position) {
        glm::vec3 local = position - housePosition;
        return housePosition + glm::vec3(-local.x, local.y, -local.z);
    };
    fadeForegroundHousePosition = housePosition;

    houseEntities.push_back(new Entity(&houseModel, housePosition, 0.0f, 90.0f, 0.0f, 1.0f));
    houseEntities.push_back(new Entity(&fireExtModel, rotateInHouse(mapMainPosition(glm::vec3(-1.5f, 0.0f, -2.5f) + mainSceneOffset)), 0.0f, 180.0f + furnitureTurnY, 0.0f, 0.001f));
    houseEntities.push_back(new Entity(&sofaModel, rotateInHouse(mapMainPosition(glm::vec3(-0.5f, 0.1f, -3.5f) + mainSceneOffset)), 0.0f, furnitureTurnY, 0.0f, 0.5f));
    houseEntities.push_back(new Entity(&tableModel, rotateInHouse(mapMainPosition(glm::vec3(4.5f, 0.0f, -3.0f) + mainSceneOffset)), 0.0f, furnitureTurnY, 0.0f, 1.2f));

    static DepthMapTexture depth = DepthMapTexture(SHADOW_WIDTH, SHADOW_HEIGHT);
    fadeForegroundShader = &lightingShader;
    fadeForegroundDepth = &depth;

    unsigned int vs_ubo = glGetUniformBlockIndex(rayTracingShader.ID, "mesh_vertices_ubo");
    glUniformBlockBinding(rayTracingShader.ID, vs_ubo, 0);

    rayTracingShader.use();
    rayTracingShader.setFloat("H", framebufferHeight);
    rayTracingShader.setFloat("W", framebufferWidth);
    rayTracingShader.setFloat("fovY", glm::radians(camera.Zoom));
    rayTracingShader.setInt("accumPrev", 1);
    rayTracingShader.setInt("displayOnly", 0);
    rayTracingShader.setInt("frameCountWithoutMove", 0);

    // [Project]
    rayTracingShader.setInt("skybox", 0);
    rayTracingShader.setInt("groundTexture", 2);
    rayTracingShader.setInt("objectTexture", 3);

    // Set materials. You can change this.
    rayTracingShader.setVec3("material_ground.albedo", glm::vec3(0.8, 0.8, 0.0));

    rayTracingShader.setVec3("material_sphere_middle.albedo", glm::vec3(0.3, 0.3, 0.8));
    rayTracingShader.setInt("material_sphere_middle.material_type", 0); // diffuse

    heatHazeShader.use();
    heatHazeShader.setInt("sceneTexture", 0);
    heatHazeShader.setFloat("hazeAmount", 0.0f);
    heatHazeShader.setFloat("groundTemp", groundTemp);

    lightingShader.use();
    lightingShader.setInt("material.diffuseSampler", 0);
    lightingShader.setInt("material.specularSampler", 1);
    lightingShader.setInt("material.normalSampler", 2);
    lightingShader.setInt("depthMapSampler", 3);
    lightingShader.setFloat("material.shininess", 32.0f);
    static glm::vec3 houseLightDir = getDirectionalLightDir(-90.0f, 45.0f);
    fadeForegroundLightDir = houseLightDir;
    lightingShader.setVec3("light.dir", houseLightDir);
    lightingShader.setVec3("light.color", glm::vec3(1.0f));
    lightingShader.setFloat("useShadow", 1.0f);
    lightingShader.setFloat("useLighting", 1.0f);
    lightingShader.setFloat("usePCF", 1.0f);

    static glm::mat4 viewMatBefore = camera.GetViewMatrix();
    static float zoomBefore = camera.Zoom;

    // Added: For quick camera movement
    lastFrame = 0.0f;

    
    static float sceneStartTime = static_cast<float>(glfwGetTime());

    onEnterImpl = [&](GLFWwindow* window) {
        sceneStartTime = static_cast<float>(glfwGetTime());
        lastFrame = 0.0f;
        firstMouse = true;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        resizeSceneTarget(framebufferWidth, framebufferHeight);
    };

    renderFrameImpl = [&](GLFWwindow* window) {

        // For quick camera movement
        float absoluteTime = static_cast<float>(glfwGetTime());
        float elapsedTime = absoluteTime - sceneStartTime;
        float temperatureTime = getTemperatureRenderTime(elapsedTime);
        deltaTime = elapsedTime - lastFrame;
        lastFrame = elapsedTime;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);

        // [Project] Animate temperature change
        groundTemp = getGroundTempAtTime(temperatureTime);

        glm::mat4 lightProjection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 80.0f);
        glm::mat4 lightView = glm::lookAt(
            housePosition - houseLightDir * 30.0f,
            housePosition,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glEnable(GL_DEPTH_TEST);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depth.depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        shadowShader.use();
        shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        drawShadowModelEntities(shadowShader, houseEntities);
        glDisable(GL_DEPTH_TEST);

        rayTracingShader.use();
        rayTracingShader.setFloat("H", framebufferHeight);
        rayTracingShader.setFloat("W", framebufferWidth);
        rayTracingShader.setFloat("fovY", glm::radians(camera.Zoom));
        rayTracingShader.setVec3("cameraPosition", camera.Position);
        glm::mat4 viewMatNow = camera.GetViewMatrix();
        rayTracingShader.setMat3("cameraToWorldRotMatrix", glm::transpose(glm::mat3(viewMatNow)));

        // Temperature
        rayTracingShader.setFloat("groundTemp", groundTemp);
        rayTracingShader.setFloat("skyTemp", skyTemp);
        float noiseTime = elapsedTime * 2.4f;
        rayTracingShader.setFloat("time", noiseTime);

        
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture.textureID);
        // -- Texture
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, groundTex.ID);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, objectTex.ID);
        // --

        glBindVertexArray(quad->ID);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        heatHazeShader.use();
        heatHazeShader.setFloat("time", noiseTime);
        heatHazeShader.setVec2("resolution", static_cast<float>(framebufferWidth), static_cast<float>(framebufferHeight));
        heatHazeShader.setFloat("hazeAmount", getHazeAmountAtTime(temperatureTime, groundTemp));
        heatHazeShader.setFloat("groundTemp", groundTemp);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneColorTexture);

        glBindVertexArray(quad->ID);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);

        lightingShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight), 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setVec3("viewPos", camera.Position);
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, depth.ID);
        drawModelEntities(lightingShader, houseEntities);

        glDisable(GL_DEPTH_TEST);

        processInput(window);

        // -------------------------------------------------------------------------------
    };
}

void onEnter(GLFWwindow* window)
{
    if (onEnterImpl) onEnterImpl(window);
}

void renderFrame(GLFWwindow* window)
{
    if (renderFrameImpl) renderFrameImpl(window);
}

void renderFadeForeground(GLFWwindow* window)
{
    (void)window;
    if (!fadeForegroundShader || !fadeForegroundDepth || !fadeForegroundEntities) {
        return;
    }
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    beginFadeForegroundRender(framebufferWidth, framebufferHeight);

    glm::mat4 lightProjection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 80.0f);
    glm::mat4 lightView = glm::lookAt(
        fadeForegroundHousePosition - fadeForegroundLightDir * 30.0f,
        fadeForegroundHousePosition,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;
    glm::mat4 projection = glm::perspective(
        glm::radians(camera.Zoom),
        static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
        0.1f,
        100.0f
    );
    glm::mat4 view = camera.GetViewMatrix();

    fadeForegroundShader->use();
    fadeForegroundShader->setFloat("useLighting", 1.0f);
    fadeForegroundShader->setFloat("useShadow", 1.0f);
    fadeForegroundShader->setFloat("usePCF", 1.0f);
    fadeForegroundShader->setVec3("light.dir", fadeForegroundLightDir);
    fadeForegroundShader->setVec3("light.color", glm::vec3(1.0f));
    fadeForegroundShader->setVec3("viewPos", camera.Position);
    fadeForegroundShader->setMat4("projection", projection);
    fadeForegroundShader->setMat4("view", view);
    fadeForegroundShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, fadeForegroundDepth->ID);

    drawModelEntities(*fadeForegroundShader, *fadeForegroundEntities);
    endFadeForegroundRender();
}

SceneModule getModule()
{
    return { "desert", init, onEnter, renderFrame, renderFadeForeground, framebuffer_size_callback, mouse_callback, scroll_callback, getCameraPose, getDefaultCameraPose, setCameraPose };
}

int runStandalone()
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
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    init(window);
    onEnter(window);
    while (!glfwWindowShouldClose(window)) {
        renderFrame(window);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}


void getCameraPose(SceneCameraPose* pose)
{
    if (!pose) return;
    pose->positionX = camera.Position.x;
    pose->positionY = camera.Position.y;
    pose->positionZ = camera.Position.z;
    pose->yaw = camera.Yaw;
    pose->pitch = camera.Pitch;
    pose->zoom = camera.Zoom;
}

void getDefaultCameraPose(SceneCameraPose* pose)
{
    if (!pose) return;
    pose->positionX = CAMERA_INITIAL_POS.x;
    pose->positionY = CAMERA_INITIAL_POS.y;
    pose->positionZ = CAMERA_INITIAL_POS.z;
    pose->yaw = CAMERA_INITIAL_YAW;
    pose->pitch = CAMERA_INITIAL_PITCH;
    pose->zoom = ZOOM;
}

void setCameraPose(const SceneCameraPose& pose)
{
    camera.Position = glm::vec3(pose.positionX, pose.positionY, pose.positionZ);
    camera.Zoom = pose.zoom;
    camera.SetAngles(pose.yaw, pose.pitch);
    firstMouse = true;
}


void setToggle(GLFWwindow* window, unsigned int key, bool *value) {
    if (glfwGetKey(window, key) == GLFW_PRESS && !isKeyboardDone[key]) {
        *value = !*value;
        isKeyboardDone[key] = true;
    }
    if (glfwGetKey(window, key) == GLFW_RELEASE) {
        isKeyboardDone[key] = false;
    }
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    // Temperature control
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        groundTemp += 20.0f * deltaTime; // raise temp
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        groundTemp -= 20.0f * deltaTime; // lower temp
        if (groundTemp < 20.0f) groundTemp = 20.0f; // minimum temp
    }

}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    framebufferWidth = width;
    framebufferHeight = height;
    resizeSceneTarget(width, height);
    glViewport(0, 0, width, height);
}


// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}

void initAccumTargets(int width, int height) {
    glGenFramebuffers(1, &accumFBO);
    glGenTextures(2, accumTex);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, accumTex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, accumFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTex[0], 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Accumulation framebuffer is not complete." << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void resizeAccumTargets(int width, int height) {
    if (width <= 0 || height <= 0 || accumTex[0] == 0) {
        return;
    }
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, accumTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void initSceneTarget(int width, int height) {
    if (sceneFBO == 0) {
        glGenFramebuffers(1, &sceneFBO);
    }
    if (sceneColorTexture == 0) {
        glGenTextures(1, &sceneColorTexture);
    }

    glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Scene framebuffer is not complete." << std::endl;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void resizeSceneTarget(int width, int height) {
    if (width <= 0 || height <= 0 || sceneColorTexture == 0) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace desert

#ifndef COMBINED_SCENE_APP
int main()
{
    return desert::runStandalone();
}
#endif
