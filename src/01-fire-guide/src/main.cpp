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
#include "geometry_primitives.h"
#include <iostream>
#include <vector>
#include "camera.h"
#include "texture.h"
#include "texture_cube.h"
#include "model.h"
#include "mesh.h"
#include "scene.h"
#include "math_utils.h"
#include "light.h"
#include "particle_system.h"
#include "FreeImage.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
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

const glm::vec3 fireSceneOffset = glm::vec3(-4.0f, 0.0f, -45.0f);
const float flightSpeed = 1.5f;

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
unsigned int fireSpawnRate = 100;

struct OfflineRenderConfig {
    bool enabled = false;
    int fps = 30;
    int frameCount = 300;
    int tileSize = 128;
    float startTime = 0.0f;
    const char* outputDir = "offline_frames";
};

struct Meteor {
    Entity* entity = nullptr;
    bool active = false;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 rotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);
    float rotationAngle = 0.0f;
    float rotationSpeed = 0.0f;
    float scale = 0.18f;
};

struct Airplane {
    Entity* entity = nullptr;
    bool active = false;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 angularVelocity = glm::vec3(0.0f);
    float scale = 0.004f;
};

float randomRange(float minValue, float maxValue)
{
    float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return minValue + (maxValue - minValue) * t;
}

glm::vec3 rotateAroundX(const glm::vec3& value, float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    return glm::vec3(
        value.x,
        value.y * c - value.z * s,
        value.y * s + value.z * c
    );
}

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

glm::mat4 getMeteorModelMatrix(const Meteor& meteor)
{
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, meteor.position);
    transform = glm::rotate(transform, meteor.rotationAngle, meteor.rotationAxis);
    transform = glm::scale(transform, glm::vec3(meteor.scale));
    return transform;
}

glm::mat4 getAirplaneModelMatrix(const Airplane& airplane)
{
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, airplane.position);
    transform = glm::rotate(transform, airplane.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    transform = glm::rotate(transform, airplane.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::rotate(transform, airplane.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    transform = glm::scale(transform, glm::vec3(airplane.scale));
    return transform;
}

glm::mat4 getToothlessFlightModelMatrix(float time)
{
    const float straightDuration = 10.0f / 1.5f;
    const float turnDuration = 10.0f;
    const float pi = static_cast<float>(M_PI);
    const float radiusX = 28.0f;
    const float radiusZ = 10.0f;
    const float straightStartBackOffset = 10.0f;
    const float turnStartAngle = pi * 0.5f;
    const float bankRoll = glm::radians(14.0f);

    glm::vec3 center = glm::vec3(0.0f, 2.0f, 0.5f) + fireSceneOffset;
    glm::vec3 turnStartPosition = center + rotateAroundX(
        glm::vec3(std::cos(turnStartAngle) * radiusX, 0.0f, std::sin(turnStartAngle) * radiusZ),
        bankRoll
    );
    glm::vec3 position;
    glm::vec3 tangent;
    float roll = bankRoll;

    if (time < straightDuration) {
        float progress = time / straightDuration;
        float startX = turnStartPosition.x + radiusX + straightStartBackOffset;
        position = glm::vec3(
            startX - progress * (startX - turnStartPosition.x),
            turnStartPosition.y,
            turnStartPosition.z
        );
        tangent = glm::vec3(-(startX - turnStartPosition.x) / straightDuration, 0.0f, 0.0f);
    }
    else {
        float turnTime = time - straightDuration;
        float turnProgress = turnTime / turnDuration;
        float angle = turnStartAngle + turnProgress * pi;
        glm::vec3 orbitOffset = rotateAroundX(
            glm::vec3(
                std::cos(angle) * radiusX,
                std::sin(angle * 2.0f) * 0.7f,
                std::sin(angle) * radiusZ
            ),
            bankRoll
        );

        position = center + orbitOffset;

        tangent = rotateAroundX(
            glm::vec3(
                -std::sin(angle) * radiusX,
                std::cos(angle * 2.0f) * 0.45f,
                std::cos(angle) * radiusZ
            ),
            bankRoll
        );
        roll = bankRoll;
    }

    glm::vec3 forward = glm::normalize(tangent);
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));
    glm::vec3 up = glm::normalize(glm::cross(forward, right));

    glm::mat4 direction = glm::mat4(1.0f);
    direction[0] = glm::vec4(right, 0.0f);
    direction[1] = glm::vec4(up, 0.0f);
    direction[2] = glm::vec4(forward, 0.0f);

    glm::mat4 glideRoll = glm::rotate(glm::mat4(1.0f), roll, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 modelCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    transform = transform * direction * modelCorrection * glideRoll;
    transform = glm::scale(transform, glm::vec3(0.05f));
    return transform;
}

struct DragonFlightPose {
    glm::vec3 position;
    glm::vec3 tangent;
    float angle;
};

DragonFlightPose getDragonFlightPose(float time)
{
    const float straightDuration = 12.0f * flightSpeed / 2.0f;
    const float turnDuration = 14.0f;
    const float pi = static_cast<float>(M_PI);
    const float radiusX = 27.3f;
    const float radiusZ = 21.6f;
    const float straightStartBackOffset = 10.0f;
    const float turnStartAngle = pi * 0.5f;
    const float bankRoll = glm::radians(12.0f);
    const float dragonYOffset = 0.5f;
    const float zWaveFrequency = 2.35f;
    const float zWaveAmplitude = 1.5f;
    const float yWaveFrequency = 1.65f;
    const float yWaveAmplitude = 0.85f;
    const float yDriftFrequency = 0.7f;
    const float yDriftAmplitude = 0.25f;

    const float toothlessTurnRadiusZ = 10.0f;
    const float toothlessBankRoll = glm::radians(14.0f);
    glm::vec3 toothlessTurnStartPosition =
        glm::vec3(0.0f, -1.0f, 0.5f) + fireSceneOffset
        + rotateAroundX(glm::vec3(0.0f, 0.0f, toothlessTurnRadiusZ), toothlessBankRoll);
    glm::vec3 turnStartPosition = toothlessTurnStartPosition + glm::vec3(0.0f, dragonYOffset, 0.0f);
    glm::vec3 center = turnStartPosition - rotateAroundX(
        glm::vec3(std::cos(turnStartAngle) * radiusX, 0.0f, std::sin(turnStartAngle) * radiusZ),
        bankRoll
    );

    if (time < straightDuration) {
        float progress = time / straightDuration;
        float startX = turnStartPosition.x + radiusX + straightStartBackOffset;
        glm::vec3 position(
            startX - progress * (startX - turnStartPosition.x),
            turnStartPosition.y,
            turnStartPosition.z
        );
        glm::vec3 tangent(-(startX - turnStartPosition.x) / straightDuration, 0.0f, 0.0f);
        return { position, tangent, turnStartAngle };
    }

    float turnTime = time - straightDuration;
    float turnProgress = turnTime / turnDuration;
    float angle = turnStartAngle + turnProgress * pi;
    float yWaveStart =
        std::sin(turnStartAngle * yWaveFrequency + 0.4f) * yWaveAmplitude
        + std::sin(turnStartAngle * yDriftFrequency + 1.1f) * yDriftAmplitude;
    float zWaveStart = std::sin(turnStartAngle * zWaveFrequency + 0.8f) * zWaveAmplitude;
    float yWave =
        std::sin(angle * yWaveFrequency + 0.4f) * yWaveAmplitude
        + std::sin(angle * yDriftFrequency + 1.1f) * yDriftAmplitude;
    float zWave = std::sin(angle * zWaveFrequency + 0.8f) * zWaveAmplitude;
    glm::vec3 orbitOffset = rotateAroundX(
        glm::vec3(
            std::cos(angle) * radiusX,
            yWave - yWaveStart,
            std::sin(angle) * radiusZ + zWave - zWaveStart
        ),
        bankRoll
    );
    glm::vec3 position = center + orbitOffset;

    glm::vec3 tangent = rotateAroundX(
        glm::vec3(
            -std::sin(angle) * radiusX,
            std::cos(angle * yWaveFrequency + 0.4f) * yWaveFrequency * yWaveAmplitude
                + std::cos(angle * yDriftFrequency + 1.1f) * yDriftFrequency * yDriftAmplitude,
            std::cos(angle) * radiusZ
                + std::cos(angle * zWaveFrequency + 0.8f) * zWaveFrequency * zWaveAmplitude
        ),
        bankRoll
    );

    return { position, tangent, angle };
}

glm::vec3 getDragonFlightVelocity(float time)
{
    return glm::normalize(getDragonFlightPose(time).tangent);
}

glm::mat4 getDragonFlightModelMatrix(float time)
{
    DragonFlightPose pose = getDragonFlightPose(time);
    glm::vec3 forward = glm::normalize(pose.tangent);
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));
    glm::vec3 up = glm::normalize(glm::cross(forward, right));

    glm::mat4 direction = glm::mat4(1.0f);
    direction[0] = glm::vec4(right, 0.0f);
    direction[1] = glm::vec4(up, 0.0f);
    direction[2] = glm::vec4(forward, 0.0f);

    float roll = glm::radians(12.0f);
    glm::mat4 glideRoll = glm::rotate(glm::mat4(1.0f), roll, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 modelCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, pose.position);
    transform = transform * direction * modelCorrection * glideRoll;
    transform = glm::scale(transform, glm::vec3(0.09f));
    return transform;
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

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (offline.enabled) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 
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
    Shader skyboxShader("../shaders/shader_skybox.vs", "../shaders/shader_skybox.fs");
    Shader particleShader("../shaders/particle.vs", "../shaders/particle.fs");

    // define models
    Model toothlessModel = Model("../resources/toothless/toothless.obj");
    Model boulderModel("../resources/boulder/boulder.obj");
    boulderModel.setDiffuse("../resources/boulder/boulder_d.png");
    boulderModel.setNormal("../resources/boulder/boulder_n.png");
    Model airplane1Model("../resources/airplane1/11803_Airplane_v1_l1.obj");
    Model airplane2Model("../resources/airplane2/11804_Airplane_v2_l2.obj");
    Model dragonModel("../resources/dragon/dragon.obj");
    dragonModel.setDiffuse("../resources/dragon/textures/Dragon_Bump_Col2.jpg");
    dragonModel.setNormal("../resources/dragon/textures/Dragon_Nor.jpg");
    Model houseModel = Model("../../00-main/resources/room/Warehouse.obj");
    Model sofaModel = Model("../../00-main/resources/sofa/sofa.obj");
    Model tableModel = Model("../../00-main/resources/table/Center Table.obj");

    // Add entities to scene.
    Scene scene;

    const glm::vec3 housePosition = glm::vec3(0.0f, 0.0f, 0.0f);
    const float furnitureTurnY = 180.0f;
    auto rotateInHouse = [housePosition](glm::vec3 position) {
        glm::vec3 local = position - housePosition;
        return housePosition + glm::vec3(-local.x, local.y, -local.z);
    };

    scene.addEntity(new Entity(&houseModel, housePosition, 0.0f, -90.0f + furnitureTurnY, 0.0f, 1.0f));
    scene.addEntity(new Entity(&sofaModel, rotateInHouse(glm::vec3(-2.5f, 0.1f, 0.5f)), 0.0f, furnitureTurnY, 0.0f, 0.5f));
    scene.addEntity(new Entity(&tableModel, rotateInHouse(glm::vec3(2.5f, 0.0f, 1.0f)), 0.0f, furnitureTurnY, 0.0f, 1.2f));

    Entity* toothlessEntity = new Entity(&toothlessModel, glm::vec3(1.0f, -3.0f, 1.0f) + fireSceneOffset, -90.0f, 180.0f, 0.0f, 0.05f);
    scene.addEntity(toothlessEntity);

    Entity* dragonEntity = new Entity(&dragonModel, glm::mat4(1.0f));
    scene.addEntity(dragonEntity);

    FireParticleSystem fireParticles(particleShader, 17000);
    MeteorParticleSystem meteorParticles(particleShader, 5000);

    std::vector<Meteor> meteors;
    const unsigned int maxMeteors = 8;
    for (unsigned int i = 0; i < maxMeteors; ++i) {
        Entity* meteorEntity = new Entity(&boulderModel, glm::mat4(1.0f));
        meteorEntity->visible = false;
        scene.addEntity(meteorEntity);

        Meteor meteor;
        meteor.entity = meteorEntity;
        meteors.push_back(meteor);
    }
    float nextMeteorSpawnTime = 0.5f;

    std::vector<Airplane> airplanes;
    Entity* airplane1Entity = new Entity(&airplane1Model, glm::mat4(1.0f));
    airplane1Entity->visible = false;
    scene.addEntity(airplane1Entity);
    Airplane airplane1;
    airplane1.entity = airplane1Entity;
    airplane1.scale = 0.003f;
    airplanes.push_back(airplane1);

    Entity* airplane2Entity = new Entity(&airplane2Model, glm::mat4(1.0f));
    airplane2Entity->visible = false;
    scene.addEntity(airplane2Entity);
    Airplane airplane2;
    airplane2.entity = airplane2Entity;
    airplane2.scale = 0.009f;
    airplanes.push_back(airplane2);
    float nextAirplaneSpawnTime = 3.0f;

    // define depth texture
    DepthMapTexture depth = DepthMapTexture(SHADOW_WIDTH, SHADOW_HEIGHT);

    // skybox (fire)
    std::vector<std::string> faces {
        "../resources/fireskybox/vulcan_bk.jpg",
        "../resources/fireskybox/vulcan_ft.jpg",
        "../resources/fireskybox/vulcan_up.jpg",
        "../resources/fireskybox/vulcan_dn.jpg",
        "../resources/fireskybox/vulcan_lf.jpg",
        "../resources/fireskybox/vulcan_rt.jpg"
    };
    CubemapTexture skyboxTexture = CubemapTexture(faces);
    unsigned int VAOskybox, VBOskybox;
    getPositionVAO(skybox_positions, sizeof(skybox_positions), VAOskybox, VBOskybox);

    lightingShader.use();
    lightingShader.setInt("material.diffuseSampler", 0);
    lightingShader.setInt("material.specularSampler", 1);
    lightingShader.setInt("material.normalSampler", 2);
    lightingShader.setInt("depthMapSampler", 3);
    lightingShader.setFloat("material.shininess", 64.f);

    skyboxShader.use();
    skyboxShader.setInt("skyboxTexture1", 0);

    DirectionalLight sun(-90.0f, 45.0f, glm::vec3(1.0f));

    float oldTime = offline.enabled ? offline.startTime : static_cast<float>(glfwGetTime());
    float flightTime = 0.0f;
    int offlineFrameIndex = 0;
    while (!glfwWindowShouldClose(window)) // render loop
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
            float yaw = -90.0f;
            if (offlineFrameIndex >= 120 && offlineFrameIndex <= 220) {
                float progress = static_cast<float>(offlineFrameIndex - 120) / 100.0f;
                yaw = -90.0f - 10.0f * progress;
            }
            else if (offlineFrameIndex > 220 && offlineFrameIndex <= 320) {
                float progress = static_cast<float>(offlineFrameIndex - 220) / 100.0f;
                yaw = -100.0f + 10.0f * progress;
            }
            camera.SetYaw(yaw);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 lightProjection = sun.getProjectionMatrix();
        glm::mat4 lightView = sun.getViewMatrix(camera.Position);
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        toothlessEntity->modelMatrix = getToothlessFlightModelMatrix(flightTime);
        dragonEntity->modelMatrix = getDragonFlightModelMatrix(flightTime);
        meteorParticles.EmitSurfaceFire(&dragonModel, dragonEntity->getModelMatrix(), getDragonFlightVelocity(flightTime), 48, 0.16f, 0.9f, 0.55f, 1.2f);

        if (currentTime >= nextMeteorSpawnTime) {
            int spawnCount = 1 + rand() % 2;
            for (int i = 0; i < spawnCount; ++i) {
                for (Meteor& meteor : meteors) {
                    if (!meteor.active) {
                        meteor.active = true;
                        float meteorX = randomRange(-30.0f, 30.0f);
                        meteor.position = glm::vec3(
                            meteorX,
                            26.f,
                            randomRange(-31.0f, -23.0f)
                        ) + fireSceneOffset;
                        float meteorFallSpeed = randomRange(3.4f, 4.3f);
                        float meteorFallAngle = glm::radians(60.0f);
                        meteor.velocity = glm::vec3(
                            std::cos(meteorFallAngle) * meteorFallSpeed,
                            -std::sin(meteorFallAngle) * meteorFallSpeed,
                            randomRange(-0.8f, 0.8f)
                        );
                        meteor.rotationAxis = glm::normalize(glm::vec3(
                            randomRange(-1.0f, 1.0f),
                            randomRange(0.2f, 1.0f),
                            randomRange(-1.0f, 1.0f)
                        ));
                        meteor.rotationAngle = randomRange(0.0f, 6.28f);
                        meteor.rotationSpeed = randomRange(0.835f, 2.0f);
                        meteor.scale = randomRange(0.12f, 0.24f);
                        meteor.entity->modelMatrix = getMeteorModelMatrix(meteor);
                        meteor.entity->visible = true;
                        break;
                    }
                }
            }
            nextMeteorSpawnTime = currentTime + randomRange(1.4f, 2.1f);
        }

        for (Meteor& meteor : meteors) {
            if (!meteor.active) continue;

            meteor.position += meteor.velocity * deltaTime;
            meteor.rotationAngle += meteor.rotationSpeed * deltaTime;
            glm::vec3 localMeteorPosition = meteor.position - fireSceneOffset;
            if (localMeteorPosition.y < -30.0f) {
                meteor.active = false;
                meteor.entity->visible = false;
                continue;
            }

            meteor.entity->modelMatrix = getMeteorModelMatrix(meteor);
            meteorParticles.EmitSurfaceFire(&boulderModel, meteor.entity->getModelMatrix(), meteor.velocity, 32, 0.14f, 0.55f, 0.45f, 1.0f);
        }

        if (currentTime >= nextAirplaneSpawnTime) {
            std::vector<int> inactiveAirplanes;
            for (int i = 0; i < static_cast<int>(airplanes.size()); ++i) {
                if (!airplanes[i].active) {
                    inactiveAirplanes.push_back(i);
                }
            }

            if (!inactiveAirplanes.empty()) {
                Airplane& airplane = airplanes[inactiveAirplanes[rand() % inactiveAirplanes.size()]];
                airplane.active = true;
                airplane.position = glm::vec3(
                    randomRange(-14.0f, 14.0f),
                    26.f,
                    randomRange(-43.0f, -25.0f)
                ) + fireSceneOffset;
                airplane.velocity = glm::vec3(
                    randomRange(-2.8f, 2.8f),
                    randomRange(-2.5f, -2.0f),
                    randomRange(-2.0f, 2.4f)
                );
                airplane.rotation = glm::vec3(
                    randomRange(-0.4f, 0.4f),
                    randomRange(0.0f, 6.28f),
                    randomRange(-0.9f, 0.9f)
                );
                airplane.angularVelocity = glm::vec3(
                    randomRange(-0.235f, 0.235f),
                    randomRange(-0.135f, 0.135f),
                    randomRange(0.5f, 1.135f)
                );
                airplane.entity->modelMatrix = getAirplaneModelMatrix(airplane);
                airplane.entity->visible = true;
            }
            nextAirplaneSpawnTime = currentTime + randomRange(10.0f, 15.0f);
        }

        for (Airplane& airplane : airplanes) {
            if (!airplane.active) continue;

            airplane.position += airplane.velocity * deltaTime;
            airplane.rotation += airplane.angularVelocity * deltaTime;
            glm::vec3 localAirplanePosition = airplane.position - fireSceneOffset;
            if (localAirplanePosition.y < -30.0f) {
                airplane.active = false;
                airplane.entity->visible = false;
                continue;
            }

            airplane.entity->modelMatrix = getAirplaneModelMatrix(airplane);
            meteorParticles.EmitSurfaceFire(airplane.entity->model, airplane.entity->getModelMatrix(), airplane.velocity, 36, 0.2f, 1.1f, 0.6f, 1.35f);
        }

        meteorParticles.Update(deltaTime);
        flightTime += dt * (flightSpeed / 1.5f) * 0.5f;

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depth.depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        shadowShader.use();
        shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        for(map<Model*, vector<Entity*>>::iterator it = scene.entities.begin(); it != scene.entities.end(); it++) {
            Model* model = it->first;
            if (!model || model->ignoreShadow) continue;
            for(Entity* entity : it->second) {
                if (!entity->visible) continue;
                glm::mat4 modelMatrix = entity->getModelMatrix();
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

        lightingShader.use();
        lightingShader.setFloat("useLighting", useLighting ? 1.0f : 0.0f);
        lightingShader.setFloat("useShadow", useShadow ? 1.0f : 0.0f);
        lightingShader.setFloat("usePCF", usePCF ? 1.0f : 0.0f);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)currentWidth / (float)currentHeight, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setVec3("light.dir", sun.lightDir);
        lightingShader.setVec3("light.color", sun.lightColor);
        lightingShader.setVec3("viewPos", camera.Position);
        lightingShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, depth.ID);

        for(map<Model*, vector<Entity*>>::iterator it = scene.entities.begin(); it != scene.entities.end(); it++) {
            Model* model = it->first;
            if (!model) continue;

            for(Entity* entity : it->second) {
                if (!entity->visible) continue;
                glm::mat4 modelMatrix = entity->getModelMatrix();
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

        // Skybox
        skyboxShader.use();
        glDepthFunc(GL_LEQUAL);
        view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);

        glBindVertexArray(VAOskybox);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture.textureID);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // Particle system rendering
        fireParticles.Update(deltaTime, currentTime, &toothlessModel, toothlessEntity->getModelMatrix(), fireSpawnRate);
        fireParticles.Draw(camera);
        meteorParticles.Draw(camera);

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

    if (offlineColorTexture) glDeleteTextures(1, &offlineColorTexture);
    if (offlineDepthRBO) glDeleteRenderbuffers(1, &offlineDepthRBO);
    if (offlineFBO) glDeleteFramebuffers(1, &offlineFBO);
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

    // key F/G: adjust Toothless fire spawn rate
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_F] == false) {
        isKeyboardDone[GLFW_KEY_F] = true;
        fireSpawnRate = std::min(fireSpawnRate + 25, 1000u);
        std::cout << "Toothless fire spawn rate: " << fireSpawnRate << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_F] = false;
    }

    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_G] == false) {
        isKeyboardDone[GLFW_KEY_G] = true;
        fireSpawnRate = fireSpawnRate >= 25 ? fireSpawnRate - 25 : 0;
        std::cout << "Toothless fire spawn rate: " << fireSpawnRate << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_G] = false;
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
