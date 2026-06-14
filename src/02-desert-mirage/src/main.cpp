#define GLM_ENABLE_EXPERIMENTAL
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../../00-main/src/shared/shader.h"
#include "../../00-main/src/shared/opengl_utils.h"
#include <iostream>
#include <vector>
#include "../../00-main/src/shared/camera.h"
#include "../../00-main/src/shared/texture.h"
#include "../../00-main/src/shared/texture_cube.h"
#include "model.h"
#include "../../00-main/src/shared/mesh.h"
#include <algorithm>
#include <cmath>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void initAccumTargets(int width, int height);
void resizeAccumTargets(int width, int height);
void initSceneTarget(int width, int height);
void resizeSceneTarget(int width, int height);
float getGroundTempAtTime(float renderTime);
float getTemperatureRenderTime(float elapsedTime);
float getHazeAmountAtTime(float renderTime, float currentGroundTemp);
void drawModelEntities(Shader& shader, const std::vector<Entity*>& entities);
glm::vec3 getDirectionalLightDir(float azimuth, float elevation);

bool isWindowed = true;
bool isKeyboardDone[1024] = { 0 };

// setting
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;

int framebufferWidth = SCR_WIDTH;
int framebufferHeight = SCR_HEIGHT;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f);
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

void test()
{
    std::cout << groundTemp << std::endl;
}

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

int main()
{
    std::cout << "Current main.cpp: hw5_real_final" << std::endl;
    camera.MovementSpeed = 0.2f;

    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    // configure global opengl state
    // -----------------------------
    // glEnable(GL_DEPTH_TEST);

    // build and compile our shader program
    // ------------------------------------
    Shader rayTracingShader("../../00-main/shaders/2-desert/shader_ray_tracing.vs", "../../00-main/shaders/2-desert/shader_ray_tracing.fs");
    Shader heatHazeShader("../../00-main/shaders/2-desert/shader_ray_tracing.vs", "../../00-main/shaders/2-desert/shader_heat_haze.fs");
    Shader lightingShader("../../00-main/shaders/shared/shader_lighting.vs", "../../00-main/shaders/shared/shader_lighting.fs");
    Shader shadowShader("../../00-main/shaders/shared/shadow.vs", "../../00-main/shaders/shared/shadow.fs");

    std::vector<float> quad_data({
        // positions         // uvs
        1.0f, 1.0f, 0.0f,  1.0f, 1.0f,  // top right
        1.0f, -1.0f, 0.0f,  1.0f, 0.0f,  // top left
        -1.0f,  -1.0f, 0.0f, 0.0f, 0.0f,   // bottom left
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f  // bottom right
        });
    std::vector<unsigned int> quad_indices_vec({ 0,1,3,1,2,3 });
    std::vector<unsigned int> attrib_sizes({ 3, 2 });

    VAO* quad = getVAOFromAttribData(quad_data, attrib_sizes, quad_indices_vec);
    initSceneTarget(framebufferWidth, framebufferHeight);

    // Original skybox data (works well)
    //std::vector<std::string> faces
    //{
    //    "../../00-main/resources/2-desert/skybox/right.jpg",
    //    "../../00-main/resources/2-desert/skybox/left.jpg",
    //    "../../00-main/resources/2-desert/skybox/top.jpg",
    //    "../../00-main/resources/2-desert/skybox/bottom.jpg",
    //    "../../00-main/resources/2-desert/skybox/front.jpg",
    //    "../../00-main/resources/2-desert/skybox/back.jpg"
    //};
    std::vector<std::string> faces
    {
        "../../00-main/resources/2-desert/desert_day/sky_posx.jpg",
        "../../00-main/resources/2-desert/desert_day/sky_negx.jpg",
        "../../00-main/resources/2-desert/desert_day/sky_posy.jpg",
        "../../00-main/resources/2-desert/desert_day/sky_negy.jpg",
        "../../00-main/resources/2-desert/desert_day/sky_posz.jpg",
        "../../00-main/resources/2-desert/desert_day/sky_negz.jpg"
    };
    CubemapTexture skyboxTexture = CubemapTexture(faces);

    


    // [Project] Texture added
    Texture objectTex("../../00-main/resources/2-desert/pyramid/sandstone_diff.jpg");
    Texture groundTex("../../00-main/resources/2-desert/pyramid/desert_sand_floor.jpg");

    Model houseModel("../../00-main/resources/0-main/room/Warehouse.obj");
    Model sofaModel("../../00-main/resources/0-main/sofa/sofa.obj");
    Model tableModel("../../00-main/resources/0-main/table/Center Table.obj");

    std::vector<Entity*> houseEntities;
    const glm::vec3 mainCameraStart = glm::vec3(0.0f, 1.5f, 0.5f);
    const glm::vec3 mainSceneOffset = glm::vec3(-2.0f, 0.0f, 4.0f);
    const glm::vec3 mainHousePosition = glm::vec3(2.0f, 0.0f, -4.0f) + mainSceneOffset;
    const glm::vec3 housePosition = camera.Position + (mainHousePosition - mainCameraStart);
    const float furnitureTurnY = 180.0f;
    auto mapMainPosition = [housePosition, mainHousePosition](glm::vec3 mainPosition) {
        return housePosition + (mainPosition - mainHousePosition);
    };
    auto rotateInHouse = [housePosition](glm::vec3 position) {
        glm::vec3 local = position - housePosition;
        return housePosition + glm::vec3(-local.x, local.y, -local.z);
    };

    houseEntities.push_back(new Entity(&houseModel, housePosition, 0.0f, 90.0f, 0.0f, 1.0f));
    houseEntities.push_back(new Entity(&sofaModel, rotateInHouse(mapMainPosition(glm::vec3(-0.5f, 0.1f, -3.5f) + mainSceneOffset)), 0.0f, furnitureTurnY, 0.0f, 0.5f));
    houseEntities.push_back(new Entity(&tableModel, rotateInHouse(mapMainPosition(glm::vec3(4.5f, 0.0f, -3.0f) + mainSceneOffset)), 0.0f, furnitureTurnY, 0.0f, 1.2f));

    DepthMapTexture depth = DepthMapTexture(SHADOW_WIDTH, SHADOW_HEIGHT);

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
    glm::vec3 houseLightDir = getDirectionalLightDir(-90.0f, 45.0f);
    lightingShader.setVec3("light.dir", houseLightDir);
    lightingShader.setVec3("light.color", glm::vec3(1.0f));
    lightingShader.setFloat("useShadow", 1.0f);
    lightingShader.setFloat("useLighting", 1.0f);
    lightingShader.setFloat("usePCF", 1.0f);

    glm::mat4 viewMatBefore = camera.GetViewMatrix();
    float zoomBefore = camera.Zoom;

    // Added: For quick camera movement
    lastFrame = glfwGetTime();

    while (!glfwWindowShouldClose(window))// render loop
    {
        // For quick camera movement
        float elapsedTime = static_cast<float>(glfwGetTime());
        float temperatureTime = getTemperatureRenderTime(elapsedTime);
        deltaTime = elapsedTime - lastFrame;
        lastFrame = elapsedTime;

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

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    if (sceneColorTexture) glDeleteTextures(1, &sceneColorTexture);
    if (sceneFBO) glDeleteFramebuffers(1, &sceneFBO);
    //glDeleteVertexArrays(1,&VAOcube);
    //glDeleteBuffers(1, VBOcube);
    //glDeleteVertexArrays(1, &VAOquad);
    //glDeleteBuffers(1, &VBOquad);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
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

    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
        std::cout << "Position" << camera.Position.x << "," << camera.Position.y << "," << camera.Position.z << std::endl;
        std::cout << "Yaw" << camera.Yaw << std::endl;
    }

    // Temperature control
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        groundTemp += 20.0f * deltaTime; // raise temp
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        groundTemp -= 20.0f * deltaTime; // lower temp
        if (groundTemp < 20.0f) groundTemp = 20.0f; // minimum temp
    }

    // Printing temperature (For debugging)
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS && !isKeyboardDone[GLFW_KEY_T]) {
        std::cout << "Current Ground Temp: " << groundTemp << std::endl;
        isKeyboardDone[GLFW_KEY_T] = true;
    }
    else if (glfwGetKey(window, GLFW_KEY_T) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_T] = false;
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
