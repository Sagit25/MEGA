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

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window, DirectionalLight* sun);

bool isWindowed = true;
bool isKeyboardDone[1024] = { 0 };

// setting
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;
const float planeSize = 15.f;

// camera
Camera camera(glm::vec3(0.0f, 2.0f, 0.0f));
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
bool usePCF = false;
bool useCSM = false;


// I obtained this part from learnopengl Cascaded Shadow Mapping code
unsigned int lightFBO;
unsigned int lightDepthMaps;
constexpr unsigned int depthMapResolution = 4096;
std::vector<float> shadowCascadeLevels{4.0f, 10.0f, 50.0f};

std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& projview)
{
    const auto inv = glm::inverse(projview);

    std::vector<glm::vec4> frustumCorners;
    for (unsigned int x = 0; x < 2; ++x)
    {
        for (unsigned int y = 0; y < 2; ++y)
        {
            for (unsigned int z = 0; z < 2; ++z)
            {
                const glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }

    return frustumCorners;
}

std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
{
    return getFrustumCornersWorldSpace(proj * view);
}

glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane, DirectionalLight* light)
{
    const auto proj = glm::perspective(
        glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, nearPlane,
        farPlane);
    const auto corners = getFrustumCornersWorldSpace(proj, camera.GetViewMatrix());

    glm::vec3 center = glm::vec3(0, 0, 0);
    for (const auto& v : corners)
    {
        center += glm::vec3(v);
    }
    center /= corners.size();

    const auto lightView = glm::lookAt(center - light->lightDir, center, glm::vec3(0.0f, 1.0f, 0.0f));

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const auto& v : corners)
    {
        const auto trf = lightView * v;
        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
    }

    // Tune this parameter according to the scene
    constexpr float zMult = 10.0f;
    if (minZ < 0)
    {
        minZ *= zMult;
    }
    else
    {
        minZ /= zMult;
    }
    if (maxZ < 0)
    {
        maxZ /= zMult;
    }
    else
    {
        maxZ *= zMult;
    }

    const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    return lightProjection * lightView;
}

std::vector<glm::mat4> getLightSpaceMatrices(DirectionalLight* light)
{
    std::vector<glm::mat4> ret;
    float cameraNearPlane = 0.1f;
    float cameraFarPlane = 100.0f;
    for (size_t i = 0; i < shadowCascadeLevels.size() + 1; ++i)
    {
        if (i == 0)
        {
            ret.push_back(getLightSpaceMatrix(cameraNearPlane, shadowCascadeLevels[i], light));
        }
        else if (i < shadowCascadeLevels.size())
        {
            ret.push_back(getLightSpaceMatrix(shadowCascadeLevels[i - 1], shadowCascadeLevels[i], light));
        }
        else
        {
            ret.push_back(getLightSpaceMatrix(shadowCascadeLevels[i - 1], cameraFarPlane, light));
        }
    }
    return ret;
}

int main()
{
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

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile our shader program
    // ------------------------------------
    Shader lightingShader("../shaders/shader_lighting.vs", "../shaders/shader_lighting.fs"); // you can name your shader files however you like
    Shader shadowShader("../shaders/shadow.vs", "../shaders/shadow.fs");
    Shader csmShader("../shaders/csm.vs", "../shaders/csm.fs", "../shaders/csm.gs");


    // define models
    // There can be three types 
    // (1) diffuse, specular, normal : brickCubeModel
    // (2) diffuse, normal only : boulderModel
    // (3) diffuse only : grassGroundModel
    AnimationModel bassModel = AnimationModel("../resources/fish/bass/bass.dae", false, false);
    Animation bassAnimation("../resources/fish/bass/bass.dae", &bassModel);
	Animator bassAnimator(&bassAnimation);
    bassModel.animator = &bassAnimator;
    bassModel.radius *= 0.7;
    bassModel.length *= 0.7;
    bassAnimation.SetDuration(1670.0);

    AnimationModel sharkModel = AnimationModel("../resources/fish/shark/shark.dae", false, false);
    Animation sharkAnimation("../resources/fish/shark/shark.dae", &sharkModel);
	Animator sharkAnimator(&sharkAnimation);
    sharkModel.animator = &sharkAnimator;
    sharkModel.radius *= 2;
    sharkModel.length *= 2;

    Model shellModel = Model("../resources/seashell/seashell1/seashell1.obj");
    Model shell2Model = Model("../resources/seashell/seashell2/seashell2.obj");
    Model pebbleModel = Model("../resources/seashell/pebble/pebble.obj");
    Model boatModel = Model("../resources/wooden_boat/wooden_boat.obj");

    Model floorModel = Model("../resources/mountain/mountain.obj", true);

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

    const int causticFrameCount = 32;
    const char* causticFrameDirectory = "../resources/caustics/caustic_frames";
    CausticTexture causticTexture(causticFrameDirectory, causticFrameCount);

    // I referenced this part from learnopengl Cascaded Shadow Mapping code
    glGenFramebuffers(1, &lightFBO);

    glGenTextures(1, &lightDepthMaps);
    glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
    glTexImage3D(
        GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, depthMapResolution, depthMapResolution, int(shadowCascadeLevels.size()) + 1,
        0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    constexpr float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, bordercolor);

    glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, lightDepthMaps, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned int matricesUBO;
    glGenBuffers(1, &matricesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * 16, nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, matricesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    DirectionalLight sun(90.0f, 30.0f, glm::vec3(0.8f));

    float oldTime = 0;
    while (!glfwWindowShouldClose(window))// render loop
    {
        float currentTime = glfwGetTime();
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

        if (useCSM) {
            // I referenced this part from learnopengl Cascaded Shadow Mapping code
            // set framebuffer to shadow framebuffer
            glViewport(0, 0, depthMapResolution, depthMapResolution);
            glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
            glClear(GL_DEPTH_BUFFER_BIT);
            
            csmShader.use();
            std::vector<glm::mat4> lightSpaceMatrices = getLightSpaceMatrices(&sun);
            glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
            for (size_t i = 0; i < lightSpaceMatrices.size(); ++i) {
                glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &lightSpaceMatrices[i]);
            }
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            for(map<Model*, vector<Entity*>>::iterator it = scene.entities.begin(); it != scene.entities.end(); it++) {
                Model* model = it->first;
                if (!model || model->ignoreShadow) continue;
                for(Entity* entity : it->second) {
                    glm::mat4 modelMatrix = entity->getModelMatrix();
                    if (entity->boid) {
                        modelMatrix = entity->boid->calculateBoid() * modelMatrix;
                    }
                    csmShader.setMat4("model", modelMatrix);
                    for (const SubMesh& subMesh : model->subMeshes) {
                        glBindVertexArray(subMesh.mesh.VAO);
                        glDrawElements(GL_TRIANGLES, subMesh.mesh.indices.size(), GL_UNSIGNED_INT, 0);
                    }
                }
            }
        }
        else {
            // I referenced this part from learnopengl shadow code
            // set framebuffer to shadow framebuffer
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
        }

        // reset framebuffer to default framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int currentWidth, currentHeight;
        glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
        glViewport(0, 0, currentWidth, currentHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        // (2) render objects in the scene!
            // framebuffer : default frame buffer(0)
            // shader : shader_lighting.fs/vs

        // I referenced this part from learnopengl lighting and shadow code
        // set use lighting, use shadow, usePCF, useCSM to shader
        lightingShader.use();
        if (useLighting) lightingShader.setFloat("useLighting", 1.0f);
        else lightingShader.setFloat("useLighting", 0.0f);
        if (useShadow) lightingShader.setFloat("useShadow", 1.0f);
        else lightingShader.setFloat("useShadow", 0.0f);
        if (usePCF) lightingShader.setFloat("usePCF", 1.0f);
        else lightingShader.setFloat("usePCF", 0.0f);
        if (useCSM) lightingShader.setFloat("useCSM", 1.0f);
        else lightingShader.setFloat("useCSM", 0.0f);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
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

        // I referenced this part from learnopengl Cascaded Shadow Mapping code
        // bind light depth map to texture unit 4
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
        lightingShader.setInt("csmDepthMapSampler", 4);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D_ARRAY, causticTexture.ID);
        lightingShader.setInt("cascadeCount", shadowCascadeLevels.size());
        for (size_t i = 0; i < shadowCascadeLevels.size(); ++i)
        {
            lightingShader.setFloat("cascadePlaneDistances[" + std::to_string(i) + "]", shadowCascadeLevels[i]);
        }

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

    // key 5: CSM
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_5] == false) {
        isKeyboardDone[GLFW_KEY_5] = true;
        useCSM = !useCSM;
    }
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_5] = false;
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
