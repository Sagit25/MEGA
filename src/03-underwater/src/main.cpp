#define GLM_ENABLE_EXPERIMENTAL
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"
#include "opengl_utils.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include "camera.h"
#include "texture.h"
#include "model.h"
#include "mesh.h"
#include "scene.h"
#include "math_utils.h"
#include "light.h"
#include "animation/model_animation.h"
#include "animation/animation.h"
#include "animation/animator.h"
// #include "animation/spline_path.h"
#include "animation/boid.h"
#include <cstdlib>
#include <ctime>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window, DirectionalLight* sun);

bool isWindowed = true;
bool isKeyboardDone[1024] = { 0 };

// setting
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;
const float planeSize = 15.f;

int framebufferWidth = SCR_WIDTH;
int framebufferHeight = SCR_HEIGHT;

// camera
Camera camera(glm::vec3(0.0f, 1.5f, 0.5f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

bool useNormalMap = true;
bool useSpecular = false;

bool useLighting = true;
bool useShadow = true;
bool usePCF = true;

int main()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

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
    glEnable(GL_DEPTH_TEST);

    // build and compile our shader program
    // ------------------------------------
    Shader lightingShader("../shaders/shader_lighting.vs", "../shaders/shader_lighting.fs"); // you can name your shader files however you like
    Shader shadowShader("../shaders/shadow.vs", "../shaders/shadow.fs");


    // define models
    // There can be three types
    // (1) diffuse, specular, normal : brickCubeModel
    // (2) diffuse, normal only : boulderModel
    // (3) diffuse only : grassGroundModel
    AnimationModel bassModel = AnimationModel("../../00-main/resources/3-underwater/fish/bass/bass.dae", true, false);
    Animation bassAnimation("../../00-main/resources/3-underwater/fish/bass/bass.dae", &bassModel);
	Animator bassAnimator(&bassAnimation);
    bassModel.animator = &bassAnimator;
    bassModel.radius *= 0.7;
    bassModel.length *= 0.7;
    bassAnimation.SetDuration(1670.0);

    AnimationModel sharkModel = AnimationModel("../../00-main/resources/3-underwater/fish/shark/shark.dae", true, false);
    Animation sharkAnimation("../../00-main/resources/3-underwater/fish/shark/shark.dae", &sharkModel);
	Animator sharkAnimator(&sharkAnimation);
    sharkModel.animator = &sharkAnimator;
    sharkModel.radius *= 2;
    sharkModel.length *= 2;

    Model shellModel = Model("../../00-main/resources/3-underwater/seashell/seashell1/seashell1.obj");
    Model shell2Model = Model("../../00-main/resources/3-underwater/seashell/seashell2/seashell2.obj");
    Model pebbleModel = Model("../../00-main/resources/3-underwater/seashell/pebble/pebble.obj");
    Model boatModel = Model("../../00-main/resources/3-underwater/wooden_boat/wooden_boat.obj");

    Model floorModel = Model("../../00-main/resources/3-underwater/mountain/mountain.obj", true);
    Model houseModel = Model("../../00-main/resources/0-main/room/Warehouse.obj", false, false);
    Model sofaModel = Model("../../00-main/resources/0-main/sofa/sofa.obj", false, false);
    Model tableModel = Model("../../00-main/resources/0-main/table/Center Table.obj", false, false);

    // Add entities to scene.
    // you can change the position/orientation.
    Scene scene;
    std::vector<Boid*> allBoids;
    std::vector<Boid*> sharkBoids;
    std::vector<Boid*> bassBoids;

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

    scene.addEntity(new Entity(&houseModel, housePosition, 0.0f, -90.0f + furnitureTurnY, 0.0f, 1.0f));
    scene.addEntity(new Entity(&sofaModel, rotateInHouse(glm::vec3(-2.5f, 0.1f, 0.5f)), 0.0f, furnitureTurnY, 0.0f, 0.5f));
    scene.addEntity(new Entity(&tableModel, rotateInHouse(glm::vec3(2.5f, 0.0f, 1.0f)), 0.0f, furnitureTurnY, 0.0f, 1.2f));

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
    DepthMapTexture depth = DepthMapTexture(SHADOW_WIDTH, SHADOW_HEIGHT);


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

    const int causticFrameCount = 32;
    const char* causticFrameDirectory = "../../00-main/resources/3-underwater/caustics/caustic_frames";
    CausticTexture causticTexture(causticFrameDirectory, causticFrameCount);

    DirectionalLight sun(-90.0f, 45.0f, glm::vec3(1.0f));

    float oldTime = static_cast<float>(glfwGetTime());
    while (!glfwWindowShouldClose(window))// render loop
    {
        float currentTime = static_cast<float>(glfwGetTime());
        float dt = currentTime - oldTime;
        deltaTime = dt;
        oldTime = currentTime;

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
        if (useLighting) lightingShader.setFloat("useLighting", 1.0f);
        else lightingShader.setFloat("useLighting", 0.0f);
        if (useShadow) lightingShader.setFloat("useShadow", 1.0f);
        else lightingShader.setFloat("useShadow", 0.0f);
        if (usePCF) lightingShader.setFloat("usePCF", 1.0f);
        else lightingShader.setFloat("usePCF", 0.0f);

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
                    lightingShader.setFloat("useSpecularMap", subMesh.specular ? 1.0f : 0.0f);
                    lightingShader.setFloat("useNormalMap", (subMesh.normal && useNormalMap) ? 1.0f : 0.0f);

                    if (subMesh.diffuse) {
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, subMesh.diffuse->ID);
                    }
                    if (subMesh.specular) {
                        glActiveTexture(GL_TEXTURE1);
                        glBindTexture(GL_TEXTURE_2D, subMesh.specular->ID);
                    }
                    if (subMesh.normal && useNormalMap) {
                        glActiveTexture(GL_TEXTURE2);
                        glBindTexture(GL_TEXTURE_2D, subMesh.normal->ID);
                    }

                    glBindVertexArray(subMesh.mesh.VAO);
                    glDrawElements(GL_TRIANGLES, subMesh.mesh.indices.size(), GL_UNSIGNED_INT, 0);
                }
            }
        }

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
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


    float t = 20.0f * deltaTime;

    // TODO :
    // Arrow key : increase, decrease sun's azimuth, elevation with amount of t.
    // key 1 : toggle using normal map
    // key 2 : toggle using shadow
    // key 3 : toggle using whole lighting

    // arrowkeys
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) sun->processKeyboard(0.0f, t);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) sun->processKeyboard(0.0f, -t);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) sun->processKeyboard(-t, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) sun->processKeyboard(t, 0.0f);

    // key 1
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_1] == false) {
        isKeyboardDone[GLFW_KEY_1] = true;
        useNormalMap = !useNormalMap;
    }
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_1] = false;
    }

    // key 2
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_2] == false) {
        isKeyboardDone[GLFW_KEY_2] = true;
        useShadow = !useShadow;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_2] = false;
    }

    // key 3
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_3] == false) {
        isKeyboardDone[GLFW_KEY_3] = true;
        useLighting = !useLighting;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_3] = false;
    }

    // key 4: PCF
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_4] == false) {
        isKeyboardDone[GLFW_KEY_4] = true;
        usePCF = !usePCF;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_4] = false;
    }

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
    camera.ProcessMouseScroll(yoffset);
}
