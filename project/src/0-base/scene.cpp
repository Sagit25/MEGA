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
#include "shared/geometry_primitives.h"
#include <iostream>
#include <vector>
#include <functional>
#include "shared/camera.h"
#include "shared/texture.h"
#include "shared/texture_cube.h"
#include "shared/model.h"
#include "shared/mesh.h"
#include "shared/scene.h"
#include "shared/math_utils.h"
#include "shared/light.h"
#include "shared/scene_module.h"
#include "shared/fade_foreground.h"

namespace base {

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window, DirectionalLight* sun);
void getCameraPose(SceneCameraPose* pose);
void getDefaultCameraPose(SceneCameraPose* pose);
void setCameraPose(const SceneCameraPose& pose);
void renderFadeForeground(GLFWwindow* window);

bool isWindowed = true;
bool isKeyboardDone[1024] = { 0 };

static bool initialized = false;
static std::function<void(GLFWwindow*)> renderFrameImpl;
static std::function<void(GLFWwindow*)> onEnterImpl;
static std::vector<Entity*> fadeForegroundEntities;
static Shader* fadeForegroundShader = nullptr;
static DepthMapTexture* fadeForegroundDepth = nullptr;
static DirectionalLight* fadeForegroundSun = nullptr;

// setting
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;
const float planeSize = 15.f;

int framebufferWidth = SCR_WIDTH;
int framebufferHeight = SCR_HEIGHT;

// camera
const glm::vec3 CAMERA_INITIAL_POS = glm::vec3(0.0f, 1.5f, 0.5f);
const float CAMERA_INITIAL_YAW = -90.0f;
const float CAMERA_INITIAL_PITCH = 0.0f;

Camera camera(CAMERA_INITIAL_POS, glm::vec3(0.0f, 1.0f, 0.0f), CAMERA_INITIAL_YAW, CAMERA_INITIAL_PITCH);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

void init(GLFWwindow* window)
{
    if (initialized) return;
    initialized = true;
    glEnable(GL_DEPTH_TEST);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    // build and compile our shader program
    // ------------------------------------
    static Shader lightingShader("../shaders/shared/shader_lighting.vs", "../shaders/shared/shader_lighting.fs"); // you can name your shader files however you like
    static Shader shadowShader("../shaders/shared/shadow.vs", "../shaders/shared/shadow.fs");
    static Shader skyboxShader("../shaders/shared/shader_skybox.vs", "../shaders/shared/shader_skybox.fs");


    // define models
    static Model brickCubeModel("../resources/0-base/brickcube/brickcube.obj");
    brickCubeModel.setDiffuse("../resources/0-base/brickcube/brickcube_d.png");
    brickCubeModel.setSpecular("../resources/0-base/brickcube/brickcube_s.png");
    brickCubeModel.setNormal("../resources/0-base/brickcube/brickcube_n.png");

    static Model boulderModel("../resources/0-base/boulder/boulder.obj");
    boulderModel.setDiffuse("../resources/0-base/boulder/boulder_d.png");
    boulderModel.setNormal("../resources/0-base/boulder/boulder_n.png");

    static Model grassGroundModel = Model("../resources/0-base/plane.obj", true);
    grassGroundModel.setDiffuse("../resources/0-base/grass_ground.jpg");

    static Model barrelModel = Model("../resources/0-base/barrel/barrel.obj");
    barrelModel.setDiffuse("../resources/0-base/barrel/barrel_d.png");
    barrelModel.setSpecular("../resources/0-base/barrel/barrel_s.png");
    barrelModel.setNormal("../resources/0-base/barrel/barrel_n.png");

    static Model fireExtModel = Model("../resources/0-base/FireExt/FireExt.obj");
    fireExtModel.setDiffuse("../resources/0-base/FireExt/FireExt_d.jpg");
    fireExtModel.setSpecular("../resources/0-base/FireExt/FireExt_s.jpg");
    fireExtModel.setNormal("../resources/0-base/FireExt/FireExt_n.jpg");

    static Model catModel = Model("../resources/0-base/cat/12221_Cat_v1_l3.obj");
    catModel.setDiffuse("../resources/0-base/cat/Cat_diffuse.jpg");

    static Model roomModel = Model("../resources/0-base/room/small_house_obj.obj");
    static Model houseModel = Model("../resources/0-base/room/Warehouse.obj");
    static Model sofaModel = Model("../resources/0-base/sofa/sofa.obj");
    static Model tableModel = Model("../resources/0-base/table/Center Table.obj");



    // Add entities to scene.
    // you can change the position/orientation.
    static Scene scene;
    fadeForegroundEntities.clear();
    

    for (float x = -150.0f + planeSize * 0.5f; x < 150.0f; x += planeSize) {
        for (float z = -150.0f + planeSize * 0.5f; z < 150.0f; z += planeSize) {
            glm::mat4 planeWorldTransform = glm::mat4(1.0f);
            planeWorldTransform = glm::translate(glm::vec3(x, 0.0f, z)) * glm::scale(glm::mat4(1.0f), glm::vec3(planeSize));
            scene.addEntity(new Entity(&grassGroundModel, planeWorldTransform));
        }
    }

    const glm::vec3 sceneOffset = glm::vec3(-2.0f, 0.0f, 4.0f);
    const glm::vec3 housePosition = glm::vec3(2.0f, 0.0f, -4.0f) + sceneOffset;
    const float furnitureTurnY = 180.0f;
    auto rotateInHouse = [housePosition](glm::vec3 position) {
        glm::vec3 local = position - housePosition;
        return housePosition + glm::vec3(-local.x, local.y, -local.z);
    };

    addFadeForegroundEntity(scene, fadeForegroundEntities, new Entity(&fireExtModel, rotateInHouse(glm::vec3(-1.5f, 0.0f, -2.5f) + sceneOffset), 0.0f, 180.0f + furnitureTurnY, 0.0f, 0.001f));
    // scene.addEntity(new Entity(&barrelModel, rotateInHouse(glm::vec3(-2.0f, 0.54f, 5.0f) + sceneOffset), 0, 0 + furnitureTurnY, 0, 0.05f));
    // scene.addEntity(new Entity(&catModel, rotateInHouse(glm::vec3(3.8f, 0.0f, -5.0f) + sceneOffset), -90.0f, 0.0f + furnitureTurnY, 0.0f, 0.02f));
    // scene.addEntity(new Entity(&roomModel, rotateInHouse(glm::vec3(-4.0f, 0.0f, 4.0f) + sceneOffset), 0.0f, 90.0f + furnitureTurnY, 0.0f, 0.02f));
    addFadeForegroundEntity(scene, fadeForegroundEntities, new Entity(&houseModel, housePosition, 0.0f, 90.0f, 0.0f, 1.0f));
    addFadeForegroundEntity(scene, fadeForegroundEntities, new Entity(&sofaModel, rotateInHouse(glm::vec3(-0.5f, 0.1f, -3.5f) + sceneOffset), 0.0f, 0.0f + furnitureTurnY, 0.0f, 0.5f));
    addFadeForegroundEntity(scene, fadeForegroundEntities, new Entity(&tableModel, rotateInHouse(glm::vec3(4.5f, 0.0f, -3.0f) + sceneOffset), 0.0f, 0.0f + furnitureTurnY, 0.0f, 1.2f));

    // define depth texture
    static DepthMapTexture depth = DepthMapTexture(SHADOW_WIDTH, SHADOW_HEIGHT);
    fadeForegroundShader = &lightingShader;
    fadeForegroundDepth = &depth;


    // skybox
    std::vector<std::string> faces
    {
        "../resources/0-base/skybox/right.jpg",
        "../resources/0-base/skybox/left.jpg",
        "../resources/0-base/skybox/top.jpg",
        "../resources/0-base/skybox/bottom.jpg",
        "../resources/0-base/skybox/front.jpg",
        "../resources/0-base/skybox/back.jpg"
    };
    static CubemapTexture skyboxTexture = CubemapTexture(faces);
    static unsigned int VAOskybox = 0, VBOskybox = 0;
    getPositionVAO(skybox_positions, sizeof(skybox_positions), VAOskybox, VBOskybox);

    lightingShader.use();
    lightingShader.setInt("material.diffuseSampler", 0);
    lightingShader.setInt("material.specularSampler", 1);
    lightingShader.setInt("material.normalSampler", 2);
    lightingShader.setInt("depthMapSampler", 3);
    lightingShader.setFloat("material.shininess", 64.f);    // set shininess to constant value.

    skyboxShader.use();
    skyboxShader.setInt("skyboxTexture1", 0);

    static DirectionalLight sun(-90.0f, 45.0f, glm::vec3(1.0f));
    fadeForegroundSun = &sun;

    static float oldTime = static_cast<float>(glfwGetTime());
    

    onEnterImpl = [&](GLFWwindow* window) {
        oldTime = static_cast<float>(glfwGetTime());
        firstMouse = true;
    };

    renderFrameImpl = [&](GLFWwindow* window) {

        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - oldTime;
        deltaTime = dt;
        oldTime = currentTime;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
        processInput(window, &sun);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        
        // TODO : 
        // (1) render shadow map!
            // framebuffer: shadow frame buffer(depth.depthMapFBO)
            // shader : shadow.fs/vs

        // I referenced this part from learnopengl shadow code
        

        glm::mat4 lightProjection = sun.getProjectionMatrix();
        glm::mat4 lightView = sun.getViewMatrix(camera.Position);
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depth.depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        shadowShader.use();
        shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        // Iterate using map<Model*, vector<Entity*>>::iterator it = scene.entities.begin()
        for(map<Model*, vector<Entity*>>::iterator it = scene.entities.begin(); it != scene.entities.end(); it++) {
            Model* model = it->first;
            if (!model) continue;
            if (model->ignoreShadow) continue;

            // bind meshes
            for (const auto& subMesh : model->subMeshes) {
                glBindVertexArray(subMesh.mesh.VAO);
                for(Entity* entity : it->second) {
                    glm::mat4 modelMatrix = entity->getModelMatrix();
                    shadowShader.setMat4("model", modelMatrix);
                    glDrawElements(GL_TRIANGLES, subMesh.mesh.indices.size(), GL_UNSIGNED_INT, 0);
                }
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        int currentWidth = framebufferWidth;
        int currentHeight = framebufferHeight;
        glViewport(0, 0, currentWidth, currentHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        // (2) render objects in the scene!
            // framebuffer : default frame buffer(0)
            // shader : shader_lighting.fs/vs

        // I referenced this part from learnopengl lighting and shadow code
        // set use lighting, use shadow, usePCF to shader
        lightingShader.use();
        lightingShader.setFloat("useLighting", 1.0f);
        lightingShader.setFloat("useShadow", 1.0f);
        lightingShader.setFloat("usePCF", 1.0f);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)currentWidth / (float)currentHeight, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // set projection, view, camera position to lighting shader
        lightingShader.use();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setVec3("light.dir", sun.lightDir);
        lightingShader.setVec3("light.color", sun.lightColor);
        lightingShader.setVec3("viewPos", camera.Position);
        lightingShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        // bind depth map to texture unit 3
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, depth.ID);

        // Iterate using map<Model*, vector<Entity*>>::iterator it = scene.entities.begin()
        for(map<Model*, vector<Entity*>>::iterator it = scene.entities.begin(); it != scene.entities.end(); it++) {
            Model* model = it->first;
            if (!model) continue;

            for (const auto& subMesh : model->subMeshes) {
                lightingShader.setVec3("baseColor", subMesh.baseColor);
                glActiveTexture(GL_TEXTURE0);
                if (subMesh.diffuse) {
                    glBindTexture(GL_TEXTURE_2D, subMesh.diffuse->ID);
                    lightingShader.setFloat("useDiffuseMap", 1.0f);
                } else {
                    glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                    lightingShader.setFloat("useDiffuseMap", 0.0f);
                }

                glActiveTexture(GL_TEXTURE1);
                if (subMesh.specular) {
                    glBindTexture(GL_TEXTURE_2D, subMesh.specular->ID);
                    lightingShader.setFloat("useSpecularMap", 1.0f);
                } else {
                    glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                    lightingShader.setFloat("useSpecularMap", 0.0f);
                }

                glActiveTexture(GL_TEXTURE2);
                if (subMesh.normal) {
                    glBindTexture(GL_TEXTURE_2D, subMesh.normal->ID);
                    lightingShader.setFloat("useNormalMap", 1.0f);
                } else {
                    glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                    lightingShader.setFloat("useNormalMap", 0.0f);
                }

                glBindVertexArray(subMesh.mesh.VAO);
                for(Entity* entity : it->second) {
                    glm::mat4 modelMatrix = entity->getModelMatrix();
                    lightingShader.setMat4("world", modelMatrix);
                    glDrawElements(GL_TRIANGLES, subMesh.mesh.indices.size(), GL_UNSIGNED_INT, 0);
                }
            }
        }

        // use skybox Shader
        skyboxShader.use();
        glDepthFunc(GL_LEQUAL);
        view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);

        // render a skybox
        glBindVertexArray(VAOskybox);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture.textureID);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

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
    if (!fadeForegroundShader || !fadeForegroundDepth || !fadeForegroundSun) {
        return;
    }

    renderLitFadeForegroundEntities(
        *fadeForegroundShader,
        fadeForegroundEntities,
        camera,
        *fadeForegroundSun,
        *fadeForegroundDepth,
        framebufferWidth,
        framebufferHeight
    );
}

SceneModule getModule()
{
    return { "base", init, onEnter, renderFrame, renderFadeForeground, framebuffer_size_callback, mouse_callback, scroll_callback, getCameraPose, getDefaultCameraPose, setCameraPose };
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

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window, DirectionalLight* sun)
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

    (void)sun;
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
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
    (void)window;
    (void)xoffset;
    (void)yoffset;
}

} // namespace base

#ifndef COMBINED_SCENE_APP
int main()
{
    return base::runStandalone();
}
#endif
