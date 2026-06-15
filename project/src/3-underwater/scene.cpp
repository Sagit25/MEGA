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
#include <algorithm>
#include <iostream>
#include <vector>
#include <functional>
#include "shared/camera.h"
#include "shared/texture.h"
#include "shared/model.h"
#include "shared/mesh.h"
#include "shared/scene.h"
#include "shared/math_utils.h"
#include "shared/light.h"
#include "animation/model_animation.h"
#include "animation/animation.h"
#include "animation/animator.h"
// #include "animation/spline_path.h"
#include "animation/boid.h"
#include <cstdlib>
#include <ctime>
#include "shared/scene_module.h"
#include "shared/fade_foreground.h"

namespace underwater {

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
static CausticTexture* fadeForegroundCaustics = nullptr;
static int fadeForegroundCausticFrameCount = 0;

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
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glEnable(GL_DEPTH_TEST);

    // build and compile our shader program
    // ------------------------------------
    static Shader lightingShader("../shaders/3-underwater/shader_lighting.vs", "../shaders/3-underwater/shader_lighting.fs"); // you can name your shader files however you like
    static Shader shadowShader("../shaders/shared/shadow.vs", "../shaders/shared/shadow.fs");


    // define models
    // There can be three types
    // (1) diffuse, specular, normal : brickCubeModel
    // (2) diffuse, normal only : boulderModel
    // (3) diffuse only : grassGroundModel
    static AnimationModel bassModel = AnimationModel("../resources/3-underwater/fish/bass/bass.dae", true, false);
    static Animation bassAnimation("../resources/3-underwater/fish/bass/bass.dae", &bassModel);
	static Animator bassAnimator(&bassAnimation);
    bassModel.animator = &bassAnimator;
    bassModel.radius *= 0.7;
    bassModel.length *= 0.7;
    bassAnimation.SetDuration(1670.0);

    static AnimationModel sharkModel = AnimationModel("../resources/3-underwater/fish/shark/shark.dae", true, false);
    static Animation sharkAnimation("../resources/3-underwater/fish/shark/shark.dae", &sharkModel);
	static Animator sharkAnimator(&sharkAnimation);
    sharkModel.animator = &sharkAnimator;
    sharkModel.radius *= 2;
    sharkModel.length *= 2;

    static Model shellModel = Model("../resources/3-underwater/seashell/seashell1/seashell1.obj", false, true, true);
    static Model shell2Model = Model("../resources/3-underwater/seashell/seashell2/seashell2.obj", false, true, true);
    static Model pebbleModel = Model("../resources/3-underwater/seashell/pebble/pebble.obj", false, true, true);
    static Model boatModel = Model("../resources/3-underwater/wooden_boat/wooden_boat.obj", false, true, true);

    static Model floorModel = Model("../resources/3-underwater/mountain/mountain.obj", true, true, true);
    static Model fireExtModel = Model("../resources/0-base/FireExt/FireExt.obj");
    fireExtModel.setDiffuse("../resources/0-base/FireExt/FireExt_d.jpg");
    fireExtModel.setSpecular("../resources/0-base/FireExt/FireExt_s.jpg");
    fireExtModel.setNormal("../resources/0-base/FireExt/FireExt_n.jpg");
    static Model houseModel = Model("../resources/0-base/room/Warehouse.obj");
    static Model sofaModel = Model("../resources/0-base/sofa/sofa.obj");
    static Model tableModel = Model("../resources/0-base/table/Center Table.obj");

    // Add entities to scene.
    // you can change the position/orientation.
    static Scene scene;
    fadeForegroundEntities.clear();
    static std::vector<Boid*> allBoids;
    static std::vector<Boid*> sharkBoids;
    static std::vector<Boid*> bassBoids;

    for (int i = 0; i < 12; i++) {
        Entity* sharkEntity = new Entity(&sharkModel, glm::scale(glm::vec3(2.0f)));
        scene.addEntity(sharkEntity);
        sharkEntity->boid = new Boid(sharkModel.radius, sharkModel.length, allBoids, 0.35f);
        sharkBoids.push_back(sharkEntity->boid);
        allBoids.push_back(sharkEntity->boid);
    }

    for (int i = 0; i < 45; i++) {
        Entity* bassEntity = new Entity(&bassModel, glm::rotate(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
        scene.addEntity(bassEntity);
        bassEntity->boid = new Boid(bassModel.radius, bassModel.length, allBoids);
        bassBoids.push_back(bassEntity->boid);
        allBoids.push_back(bassEntity->boid);
    }

    auto propTransform = [](glm::vec3 position, float yaw, float pitch, float roll, float scale) {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
        transform = glm::rotate(transform, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(roll), glm::vec3(0.0f, 0.0f, 1.0f));
        return glm::scale(transform, glm::vec3(scale));
    };
    auto boatTransform = [](glm::vec3 position, float yaw, float pitch, float scale) {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
        transform = glm::rotate(transform, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        return glm::scale(transform, glm::vec3(scale));
    };
    const glm::vec3 housePosition = glm::vec3(0.0f, 0.0f, 0.0f);
    const float furnitureTurnY = 180.0f;
    auto rotateInHouse = [housePosition](glm::vec3 position) {
        glm::vec3 local = position - housePosition;
        return housePosition + glm::vec3(-local.x, local.y, -local.z);
    };

    addFadeForegroundEntity(scene, fadeForegroundEntities, new Entity(&houseModel, housePosition, 0.0f, -90.0f + furnitureTurnY, 0.0f, 1.0f));
    addFadeForegroundEntity(scene, fadeForegroundEntities, new Entity(&fireExtModel, rotateInHouse(glm::vec3(-3.5f, 0.0f, 1.5f)), 0.0f, 180.0f + furnitureTurnY, 0.0f, 0.001f));
    addFadeForegroundEntity(scene, fadeForegroundEntities, new Entity(&sofaModel, rotateInHouse(glm::vec3(-2.5f, 0.1f, 0.5f)), 0.0f, furnitureTurnY, 0.0f, 0.5f));
    addFadeForegroundEntity(scene, fadeForegroundEntities, new Entity(&tableModel, rotateInHouse(glm::vec3(2.5f, 0.0f, 1.0f)), 0.0f, furnitureTurnY, 0.0f, 1.2f));

    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(-7.6f, -1.05f, -24.0f), -26.0f, -8.0f, 14.0f, 0.28f)));
    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(-6.2f, -1.04f, -25.5f), 24.0f, 6.0f, -18.0f, 0.34f)));
    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(-4.6f, -1.18f, -27.0f), -58.0f, -5.0f, 10.0f, 0.28f)));
    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(-8.7f, -1.22f, -21.6f), 42.0f, -6.0f, 12.0f, 0.26f)));
    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(-9.2f, -0.48f, -26.8f), -18.0f, 7.0f, -16.0f, 0.30f)));
    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(-7.0f, -0.50f, -30.5f), 68.0f, -4.0f, 9.0f, 0.24f)));
    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(-9.8f, -1.06f, -22.8f), -12.0f, 5.0f, -13.0f, 0.25f)));
    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(-17.0f, -0.18f, -21.0f), -20.0f, -7.0f, 12.0f, 0.22f)));
    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(9.0f, -1.18f, -29.6f), 36.0f, -5.0f, 12.0f, 0.22f)));
    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(-14.5f, -0.15f, -22.5f), 38.0f, 6.0f, -15.0f, 0.26f)));
    scene.addEntity(new Entity(&shellModel, propTransform(glm::vec3(-12.2f, -0.52f, -24.0f), -58.0f, -5.0f, 10.0f, 0.22f)));
    scene.addEntity(new Entity(&shell2Model, propTransform(glm::vec3(-15.5f, -0.25f, -26.2f), -36.0f, 7.0f, -12.0f, 0.10f)));
    scene.addEntity(new Entity(&shell2Model, propTransform(glm::vec3(-10.8f, -0.78f, -26.8f), 24.0f, -7.0f, 13.0f, 0.10f)));
    scene.addEntity(new Entity(&shell2Model, propTransform(glm::vec3(-8.4f, -0.52f, -28.2f), -34.0f, 8.0f, -12.0f, 0.11f)));
    scene.addEntity(new Entity(&shell2Model, propTransform(glm::vec3(-6.4f, -0.88f, -29.2f), 18.0f, -7.0f, 16.0f, 0.12f)));
    scene.addEntity(new Entity(&shell2Model, propTransform(glm::vec3(-5.1f, -1.00f, -31.0f), -46.0f, 6.0f, -14.0f, 0.10f)));
    scene.addEntity(new Entity(&shell2Model, propTransform(glm::vec3(-4.4f, -1.48f, -28.8f), -72.0f, 5.0f, -9.0f, 0.09f)));
    scene.addEntity(new Entity(&shell2Model, propTransform(glm::vec3(-3.8f, -1.65f, -24.6f), -64.0f, 5.0f, -10.0f, 0.10f)));
    scene.addEntity(new Entity(&shell2Model, propTransform(glm::vec3(-3.0f, -1.5f, -33.8f), 54.0f, -6.0f, 10.0f, 0.09f)));
    scene.addEntity(new Entity(&shell2Model, propTransform(glm::vec3(-2.8f, -2.1f, -26.2f), 28.0f, -8.0f, 12.0f, 0.11f)));
    scene.addEntity(new Entity(&shell2Model, propTransform(glm::vec3(12.2f, -0.76f, -32.0f), -50.0f, 7.0f, -11.0f, 0.09f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(-16.4f, -0.44f, -24.2f), 18.0f, 0.0f, 0.0f, 0.034f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(-13.4f, -0.66f, -25.6f), -32.0f, 0.0f, 0.0f, 0.032f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(-11.5f, -1.30f, -22.6f), 68.0f, 0.0f, 0.0f, 0.034f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(-9.6f, -1.04f, -25.0f), -16.0f, 0.0f, 0.0f, 0.030f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(-18.2f, 0.05f, -23.8f), 44.0f, 0.0f, 0.0f, 0.032f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(-12.8f, 0.08f, -28.0f), -74.0f, 0.0f, 0.0f, 0.030f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(7.0f, -1.87f, -28.5f), 18.0f, 0.0f, 0.0f, 0.048f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(8.4f, -1.73f, -29.2f), -12.0f, 0.0f, 0.0f, 0.040f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(9.8f, -1.58f, -28.8f), 72.0f, 0.0f, 0.0f, 0.044f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(11.2f, -1.40f, -30.0f), -36.0f, 0.0f, 0.0f, 0.050f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(12.6f, -1.26f, -30.8f), 46.0f, 0.0f, 0.0f, 0.038f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(8.0f, -1.59f, -31.0f), -18.0f, 0.0f, 0.0f, 0.042f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(9.4f, -1.40f, -32.0f), 28.0f, 0.0f, 0.0f, 0.040f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(10.8f, -1.37f, -31.7f), -64.0f, 0.0f, 0.0f, 0.036f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(12.0f, -1.19f, -32.4f), 86.0f, 0.0f, 0.0f, 0.044f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(13.4f, -1.11f, -33.2f), -42.0f, 0.0f, 0.0f, 0.036f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(9.8f, -1.58f, -30.5f), 12.0f, 0.0f, 0.0f, 0.038f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(10.5f, -1.25f, -30.8f), 55.0f, 0.0f, 0.0f, 0.036f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(9.2f, -1.29f, -30.9f), -28.0f, 0.0f, 0.0f, 0.034f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(10.0f, -1.20f, -30.6f), 104.0f, 0.0f, 0.0f, 0.032f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(6.4f, -1.81f, -29.8f), -8.0f, 0.0f, 0.0f, 0.036f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(7.2f, -1.44f, -33.5f), 38.0f, 0.0f, 0.0f, 0.040f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(8.8f, -1.33f, -34.0f), -76.0f, 0.0f, 0.0f, 0.034f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(11.5f, -1.08f, -34.2f), 68.0f, 0.0f, 0.0f, 0.038f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(14.0f, -1.08f, -31.4f), -22.0f, 0.0f, 0.0f, 0.036f)));
    scene.addEntity(new Entity(&pebbleModel, propTransform(glm::vec3(12.8f, -1.29f, -28.4f), 92.0f, 0.0f, 0.0f, 0.034f)));
    scene.addEntity(new Entity(&boatModel, boatTransform(glm::vec3(13.0f, -1.05f, -36.0f), 22.0f, 5.0f, 6.2f)));
    scene.addEntity(new Entity(&floorModel, glm::translate(glm::vec3(-40.0f, -3.05f, -40.0f)) * glm::scale(glm::vec3(0.018f, 0.008f, 0.018f))));
    scene.addEntity(new Entity(&floorModel, glm::translate(glm::vec3(40.0f, -3.05f, -40.0f)) * glm::scale(glm::vec3(0.018f, 0.008f, 0.018f))));
    scene.addEntity(new Entity(&floorModel, glm::translate(glm::vec3(-40.0f, -3.05f, 40.0f)) * glm::scale(glm::vec3(0.018f, 0.008f, 0.018f))));
    scene.addEntity(new Entity(&floorModel, glm::translate(glm::vec3(40.0f, -3.05f, 40.0f)) * glm::scale(glm::vec3(0.018f, 0.008f, 0.018f))));

    // define depth texture
    static DepthMapTexture depth = DepthMapTexture(SHADOW_WIDTH, SHADOW_HEIGHT);
    fadeForegroundShader = &lightingShader;
    fadeForegroundDepth = &depth;


    glClearColor(0.15f, 0.52f, 0.73f, 1.0f);

    lightingShader.use();
    lightingShader.setInt("material.diffuseSampler", 0);
    lightingShader.setInt("material.specularSampler", 1);
    lightingShader.setInt("material.normalSampler", 2);
    lightingShader.setInt("depthMapSampler", 3);
    lightingShader.setInt("causticSampler", 5);
    lightingShader.setFloat("material.shininess", 64.f);    // set shininess to constant value.
    lightingShader.setVec2("houseEffectMin", -6.0f, -4.0f);
    lightingShader.setVec2("houseEffectMax", 6.0f, 4.0f);

    static const int causticFrameCount = 32;
    const char* causticFrameDirectory = "../resources/3-underwater/caustics/caustic_frames";
    static CausticTexture causticTexture(causticFrameDirectory, causticFrameCount);
    fadeForegroundCaustics = &causticTexture;
    fadeForegroundCausticFrameCount = causticFrameCount;

    static DirectionalLight sun(-90.0f, 45.0f, glm::vec3(1.0f));
    fadeForegroundSun = &sun;

    static float oldTime = static_cast<float>(glfwGetTime());
    

    onEnterImpl = [&](GLFWwindow* window) {
        oldTime = static_cast<float>(glfwGetTime());
        firstMouse = true;
        glClearColor(0.15f, 0.52f, 0.73f, 1.0f);
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

        glClearColor(0.15f, 0.52f, 0.73f, 1.0f);

        // input
        processInput(window, &sun);
        bassAnimator.UpdateAnimation(2*deltaTime);
        sharkAnimator.UpdateAnimation(deltaTime);

        for (auto boid: sharkBoids) {
            boid->advance(deltaTime, sharkBoids, allBoids);
        }
        for (auto boid: bassBoids) {
            boid->advance(deltaTime, bassBoids, allBoids);
        }

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

        for(map<Model*, vector<Entity*>>::iterator it = scene.entities.begin(); it != scene.entities.end(); it++) {
            Model* model = it->first;
            if (!model || model->ignoreShadow) continue;
            for(Entity* entity : it->second) {
                glm::mat4 modelMatrix = entity->getModelMatrix();
                if (entity->boid) {
                    modelMatrix = entity->boid->calculateBoid() * modelMatrix;
                }
                shadowShader.setMat4("model", modelMatrix);
                for (const SubMesh& subMesh : model->subMeshes) {
                    glBindVertexArray(subMesh.mesh.VAO);
                    glDrawElements(GL_TRIANGLES, subMesh.mesh.indices.size(), GL_UNSIGNED_INT, 0);
                }
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int windowFramebufferWidth = 0;
        int windowFramebufferHeight = 0;
        glfwGetFramebufferSize(window, &windowFramebufferWidth, &windowFramebufferHeight);
        framebufferWidth = windowFramebufferWidth;
        framebufferHeight = windowFramebufferHeight;

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
        lightingShader.setFloat("currentTime", currentTime);
        lightingShader.setFloat("causticFrameCount", causticFrameCount);

        // bind depth map to texture unit 3
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, depth.ID);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D_ARRAY, causticTexture.ID);

        // Iterate using map<Model*, vector<Entity*>>::iterator it = scene.entities.begin()
        for(map<Model*, vector<Entity*>>::iterator it = scene.entities.begin(); it != scene.entities.end(); it++) {
            Model* model = it->first;
            if (!model) continue;

            if (model->IsAnimated()) {
                Animator *animator = dynamic_cast<AnimationModel*>(model)->animator;
                auto transforms = animator->GetFinalBoneMatrices();
                for (int i = 0; i < transforms.size(); ++i) {
                    lightingShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);
                }
            }

            for(Entity* entity : it->second) {
                glm::mat4 modelMatrix = entity->getModelMatrix();
                if (entity->boid) {
                    modelMatrix = entity->boid->calculateBoid() * modelMatrix;
                }
                lightingShader.setMat4("world", modelMatrix);

                for (const SubMesh& subMesh : model->subMeshes) {
                    lightingShader.setVec3("baseColor", subMesh.baseColor);

                    glActiveTexture(GL_TEXTURE0);
                    if (subMesh.diffuse) {
                        glBindTexture(GL_TEXTURE_2D, subMesh.diffuse->ID);
                        lightingShader.setFloat("useDiffuseMap", 1.0f);
                    }
                    else {
                        glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                        lightingShader.setFloat("useDiffuseMap", 0.0f);
                    }

                    glActiveTexture(GL_TEXTURE1);
                    if (subMesh.specular) {
                        glBindTexture(GL_TEXTURE_2D, subMesh.specular->ID);
                        lightingShader.setFloat("useSpecularMap", 1.0f);
                    }
                    else {
                        glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                        lightingShader.setFloat("useSpecularMap", 0.0f);
                    }

                    glActiveTexture(GL_TEXTURE2);
                    if (subMesh.normal) {
                        glBindTexture(GL_TEXTURE_2D, subMesh.normal->ID);
                        lightingShader.setFloat("useNormalMap", 1.0f);
                    }
                    else {
                        glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                        lightingShader.setFloat("useNormalMap", 0.0f);
                    }

                    glBindVertexArray(subMesh.mesh.VAO);
                    glDrawElements(GL_TRIANGLES, subMesh.mesh.indices.size(), GL_UNSIGNED_INT, 0);
                }
            }
        }

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
        framebufferHeight,
        fadeForegroundCaustics,
        fadeForegroundCausticFrameCount,
        static_cast<float>(glfwGetTime())
    );
}

SceneModule getModule()
{
    return { "underwater", init, onEnter, renderFrame, renderFadeForeground, framebuffer_size_callback, mouse_callback, scroll_callback, getCameraPose, getDefaultCameraPose, setCameraPose };
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

} // namespace underwater

#ifndef COMBINED_SCENE_APP
int main()
{
    return underwater::runStandalone();
}
#endif
