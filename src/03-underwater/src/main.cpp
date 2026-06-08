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
#include "FreeImage.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window, DirectionalLight* sun);
void saveImage(const char* filename);
void createDirectoryIfNeeded(const char* path);

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
bool usePCF = false;

struct OfflineRenderConfig {
    bool enabled = false;
    int fps = 30;
    int frameCount = 300;
    int tileSize = 128;
    float startTime = 0.0f;
    const char* outputDir = "offline_frames";
};

void saveImage(const char* filename)
{
    int width = framebufferWidth;
    int height = framebufferHeight;
    BYTE* pixels = new BYTE[3 * width * height];
    glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, pixels);

    FIBITMAP* image = FreeImage_ConvertFromRawBits(
        pixels,
        width,
        height,
        3 * width,
        24,
        0xFF0000,
        0x00FF00,
        0x0000FF,
        false
    );
    FreeImage_Save(FIF_PNG, image, filename, 0);
    FreeImage_Unload(image);
    delete[] pixels;
}

void createDirectoryIfNeeded(const char* path)
{
    mkdir(path, 0755);
}

int main(int argc, char** argv)
{
    OfflineRenderConfig offline;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--offline") == 0) {
            offline.enabled = true;
        }
        else if (std::strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            offline.fps = std::max(1, std::atoi(argv[++i]));
        }
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            offline.frameCount = std::max(1, std::atoi(argv[++i]));
        }
        else if (std::strcmp(argv[i], "--tile-size") == 0 && i + 1 < argc) {
            offline.tileSize = std::max(16, std::atoi(argv[++i]));
        }
        else if (std::strcmp(argv[i], "--start-time") == 0 && i + 1 < argc) {
            offline.startTime = std::max(0.0f, static_cast<float>(std::atof(argv[++i])));
        }
        else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            offline.outputDir = argv[++i];
        }
    }

    if (offline.enabled) {
        createDirectoryIfNeeded(offline.outputDir);
        std::cout << "Offline rendering: " << offline.frameCount
                  << " frames at " << offline.fps
                  << " fps from t=" << offline.startTime
                  << ", tile " << offline.tileSize
                  << " -> " << offline.outputDir << std::endl;
    }

    std::srand(offline.enabled ? 1u : static_cast<unsigned int>(std::time(nullptr)));

    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (offline.enabled) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
    if (offline.enabled) {
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
    }
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
    if (!offline.enabled) {
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);

        // tell GLFW to capture our mouse
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

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

    unsigned int offlineFBO = 0;
    unsigned int offlineColorTexture = 0;
    unsigned int offlineDepthRBO = 0;
    if (offline.enabled) {
        glGenFramebuffers(1, &offlineFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, offlineFBO);

        glGenTextures(1, &offlineColorTexture);
        glBindTexture(GL_TEXTURE_2D, offlineColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, offlineColorTexture, 0);

        glGenRenderbuffers(1, &offlineDepthRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, offlineDepthRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, offlineDepthRBO);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "Failed to create offline framebuffer" << std::endl;
            glfwTerminate();
            return -1;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        framebufferWidth = SCR_WIDTH;
        framebufferHeight = SCR_HEIGHT;
    }

    // build and compile our shader program
    // ------------------------------------
    Shader lightingShader("../shaders/shader_lighting.vs", "../shaders/shader_lighting.fs"); // you can name your shader files however you like
    Shader shadowShader("../shaders/shadow.vs", "../shaders/shadow.fs");


    // define models
    // There can be three types
    // (1) diffuse, specular, normal : brickCubeModel
    // (2) diffuse, normal only : boulderModel
    // (3) diffuse only : grassGroundModel
    AnimationModel bassModel = AnimationModel("../resources/fish/bass/bass.dae", true, false);
    Animation bassAnimation("../resources/fish/bass/bass.dae", &bassModel);
	Animator bassAnimator(&bassAnimation);
    bassModel.animator = &bassAnimator;
    bassModel.radius *= 0.7;
    bassModel.length *= 0.7;
    bassAnimation.SetDuration(1670.0);

    AnimationModel sharkModel = AnimationModel("../resources/fish/shark/shark.dae", true, false);
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
    Model houseModel = Model("../../00-main/resources/room/Warehouse.obj", false, false);
    Model sofaModel = Model("../../00-main/resources/sofa/sofa.obj", false, false);
    Model tableModel = Model("../../00-main/resources/table/Center Table.obj", false, false);

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
    const char* causticFrameDirectory = "../resources/caustics/caustic_frames";
    CausticTexture causticTexture(causticFrameDirectory, causticFrameCount);

    DirectionalLight sun(-90.0f, 45.0f, glm::vec3(1.0f));

    const glm::vec3 offlineCameraBasePosition = camera.Position;
    auto frameProgress = [](int frame, int startFrame, int endFrame) {
        return glm::clamp(
            static_cast<float>(frame - startFrame) / static_cast<float>(endFrame - startFrame),
            0.0f,
            1.0f
        );
    };

    float oldTime = offline.enabled ? offline.startTime : static_cast<float>(glfwGetTime());
    int offlineFrameIndex = 0;
    while (!glfwWindowShouldClose(window))// render loop
    {
        float currentTime = offline.enabled
            ? offline.startTime + static_cast<float>(offlineFrameIndex) / static_cast<float>(offline.fps)
            : static_cast<float>(glfwGetTime());
        float dt = offline.enabled ? 1.0f / static_cast<float>(offline.fps) : currentTime - oldTime;
        deltaTime = dt;
        oldTime = currentTime;

        // input
        if (!offline.enabled) {
            processInput(window, &sun);
        }
        else {
            float cameraZ = 0.5f;
            if (offlineFrameIndex >= 30 && offlineFrameIndex <= 60) {
                cameraZ = glm::mix(0.5f, -1.5f, frameProgress(offlineFrameIndex, 30, 60));
            }
            else if (offlineFrameIndex > 60 && offlineFrameIndex < 540) {
                cameraZ = -1.5f;
            }
            else if (offlineFrameIndex >= 540 && offlineFrameIndex <= 570) {
                cameraZ = glm::mix(-1.5f, 0.5f, frameProgress(offlineFrameIndex, 540, 570));
            }

            float pitch = 0.0f;
            if (offlineFrameIndex >= 60 && offlineFrameIndex <= 90) {
                pitch = glm::mix(0.0f, 15.0f, frameProgress(offlineFrameIndex, 60, 90));
            }
            else if (offlineFrameIndex > 90 && offlineFrameIndex < 480) {
                pitch = 15.0f;
            }
            else if (offlineFrameIndex >= 480 && offlineFrameIndex <= 540) {
                pitch = glm::mix(15.0f, 0.0f, frameProgress(offlineFrameIndex, 480, 540));
            }

            float yaw = -90.0f;
            if (offlineFrameIndex >= 210 && offlineFrameIndex <= 240) {
                yaw = glm::mix(-90.0f, -110.0f, frameProgress(offlineFrameIndex, 210, 240));
            }
            else if (offlineFrameIndex > 240 && offlineFrameIndex < 330) {
                yaw = -110.0f;
            }
            else if (offlineFrameIndex >= 330 && offlineFrameIndex <= 360) {
                yaw = glm::mix(-110.0f, -70.0f, frameProgress(offlineFrameIndex, 330, 360));
            }
            else if (offlineFrameIndex > 360 && offlineFrameIndex < 450) {
                yaw = -70.0f;
            }
            else if (offlineFrameIndex >= 450 && offlineFrameIndex <= 480) {
                yaw = glm::mix(-70.0f, -90.0f, frameProgress(offlineFrameIndex, 450, 480));
            }

            camera.Position = glm::vec3(offlineCameraBasePosition.x, offlineCameraBasePosition.y, cameraZ);
            camera.SetAngles(yaw, pitch);
        }
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

        if (offline.enabled) {
            glBindFramebuffer(GL_FRAMEBUFFER, offlineFBO);
            framebufferWidth = SCR_WIDTH;
            framebufferHeight = SCR_HEIGHT;
        }
        else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            int windowFramebufferWidth = 0;
            int windowFramebufferHeight = 0;
            glfwGetFramebufferSize(window, &windowFramebufferWidth, &windowFramebufferHeight);
            framebufferWidth = windowFramebufferWidth;
            framebufferHeight = windowFramebufferHeight;
        }

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

        if (offline.enabled) {
            glFinish();

            char filename[256];
            std::snprintf(filename, sizeof(filename), "%s/frame_%04d.png", offline.outputDir, offlineFrameIndex);
            saveImage(filename);

            if (offlineFrameIndex % offline.fps == 0) {
                std::cout << "Saved " << filename << std::endl;
            }

            ++offlineFrameIndex;
            if (offlineFrameIndex >= offline.frameCount) {
                glfwSetWindowShouldClose(window, true);
            }
            glfwPollEvents();
        }
        else {
            // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
            // -------------------------------------------------------------------------------
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------

    if (offlineColorTexture) glDeleteTextures(1, &offlineColorTexture);
    if (offlineDepthRBO) glDeleteRenderbuffers(1, &offlineDepthRBO);
    if (offlineFBO) glDeleteFramebuffers(1, &offlineFBO);

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
