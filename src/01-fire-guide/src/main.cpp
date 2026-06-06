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
#include <cstdlib>
#include <ctime>
#include <cmath>


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
Camera camera(glm::vec3(0.0f, 0.5f, 3.0f));
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
unsigned int fireSpawnRate = 100;

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
    const float loopDuration = 18.0f;
    const float angle = time / loopDuration * M_PI;
    const float radiusX = 7.0f;
    const float radiusZ = 5.0f;

    glm::vec3 center(0.0f, -1.0f, 4.5f);
    glm::vec3 position(
        center.x + std::cos(angle) * radiusX,
        center.y + std::sin(angle * 2.0f) * 0.7f,
        center.z + std::sin(angle) * radiusZ
    );

    glm::vec3 tangent(
        -std::sin(angle) * radiusX,
        std::cos(angle * 2.0f) * 0.45f,
        std::cos(angle) * radiusZ
    );
    glm::vec3 forward = glm::normalize(tangent);
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));
    glm::vec3 up = glm::normalize(glm::cross(forward, right));

    float roll = glm::radians(14.0f) * std::sin(angle);
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
    const float loopDuration = 29.0f;
    const float angle = time / loopDuration * M_PI + 1.2f;
    const float radiusX = 10.5f;
    const float radiusZ = 8.3f;
    const float zWaveFrequency = 2.35f;
    const float zWaveAmplitude = 1.5f;
    const float yWaveFrequency = 1.65f;
    const float yWaveAmplitude = 0.85f;
    const float yDriftFrequency = 0.7f;
    const float yDriftAmplitude = 0.25f;

    glm::vec3 center(0.0f, -1.7f, 5.0f);
    glm::vec3 position(
        center.x + std::cos(angle) * radiusX,
        center.y
            + std::sin(angle * yWaveFrequency + 0.4f) * yWaveAmplitude
            + std::sin(angle * yDriftFrequency + 1.1f) * yDriftAmplitude,
        center.z
            + std::sin(angle) * radiusZ
            + std::sin(angle * zWaveFrequency + 0.8f) * zWaveAmplitude
    );

    glm::vec3 tangent(
        -std::sin(angle) * radiusX,
        std::cos(angle * yWaveFrequency + 0.4f) * yWaveFrequency * yWaveAmplitude
            + std::cos(angle * yDriftFrequency + 1.1f) * yDriftFrequency * yDriftAmplitude,
        std::cos(angle) * radiusZ
            + std::cos(angle * zWaveFrequency + 0.8f) * zWaveFrequency * zWaveAmplitude
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

    float roll = glm::radians(12.0f) * std::sin(pose.angle);
    glm::mat4 glideRoll = glm::rotate(glm::mat4(1.0f), roll, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 modelCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, pose.position);
    transform = transform * direction * modelCorrection * glideRoll;
    transform = glm::scale(transform, glm::vec3(0.09f));
    return transform;
}


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
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 
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
    Shader skyboxShader("../shaders/shader_skybox.vs", "../shaders/shader_skybox.fs");
    Shader csmShader("../shaders/csm.vs", "../shaders/csm.fs", "../shaders/csm.gs");
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

    // Add entities to scene.
    Scene scene;

    Entity* toothlessEntity = new Entity(&toothlessModel, glm::vec3(1.0f, -3.0f, 5.0f), -90.0f, 180.0f, 0.0f, 0.05f);
    scene.addEntity(toothlessEntity);

    Entity* dragonEntity = new Entity(&dragonModel, glm::mat4(1.0f));
    scene.addEntity(dragonEntity);

    FireParticleSystem fireParticles(particleShader, 30000);
    MeteorParticleSystem meteorParticles(particleShader, 12000);

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
    airplane1.scale = 0.002f;
    airplanes.push_back(airplane1);

    Entity* airplane2Entity = new Entity(&airplane2Model, glm::mat4(1.0f));
    airplane2Entity->visible = false;
    scene.addEntity(airplane2Entity);
    Airplane airplane2;
    airplane2.entity = airplane2Entity;
    airplane2.scale = 0.006f;
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

    glGenFramebuffers(1, &lightFBO);
    glGenTextures(1, &lightDepthMaps);
    glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, depthMapResolution, depthMapResolution, int(shadowCascadeLevels.size()) + 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
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

    DirectionalLight sun(30.0f, 30.0f, glm::vec3(0.8f));

    float oldTime = 0;
    while (!glfwWindowShouldClose(window)) // render loop
    {
        float currentTime = glfwGetTime();
        float dt = currentTime - oldTime;
        deltaTime = dt;
        oldTime = currentTime;

        // input
        processInput(window, &sun);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 lightProjection = sun.getProjectionMatrix();
        glm::mat4 lightView = sun.getViewMatrix(camera.Position);
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        toothlessEntity->modelMatrix = getToothlessFlightModelMatrix(currentTime);
        dragonEntity->modelMatrix = getDragonFlightModelMatrix(currentTime);
        meteorParticles.EmitSurfaceFire(&dragonModel, dragonEntity->getModelMatrix(), getDragonFlightVelocity(currentTime), 80, 0.16f, 0.9f, 0.55f, 1.2f);

        if (currentTime >= nextMeteorSpawnTime) {
            int spawnCount = 1 + rand() % 3;
            for (int i = 0; i < spawnCount; ++i) {
                for (Meteor& meteor : meteors) {
                    if (!meteor.active) {
                        meteor.active = true;
                        meteor.position = glm::vec3(
                            randomRange(-13.0f, 13.0f),
                            randomRange(9.0f, 15.0f),
                            randomRange(-12.0f, 12.0f)
                        );
                        meteor.velocity = glm::vec3(
                            randomRange(-2.0f, 2.0f),
                            randomRange(-7.5f, -5.0f),
                            randomRange(-1.8f, 2.0f)
                        );
                        meteor.rotationAxis = glm::normalize(glm::vec3(
                            randomRange(-1.0f, 1.0f),
                            randomRange(0.2f, 1.0f),
                            randomRange(-1.0f, 1.0f)
                        ));
                        meteor.rotationAngle = randomRange(0.0f, 6.28f);
                        meteor.rotationSpeed = randomRange(2.5f, 6.0f);
                        meteor.scale = randomRange(0.08f, 0.16f);
                        meteor.entity->modelMatrix = getMeteorModelMatrix(meteor);
                        meteor.entity->visible = true;
                        break;
                    }
                }
            }
            nextMeteorSpawnTime = currentTime + randomRange(0.7f, 1.4f);
        }

        for (Meteor& meteor : meteors) {
            if (!meteor.active) continue;

            meteor.position += meteor.velocity * deltaTime;
            meteor.rotationAngle += meteor.rotationSpeed * deltaTime;
            if (meteor.position.y < -4.5f || glm::abs(meteor.position.x) > 20.0f || meteor.position.z > 20.0f || meteor.position.z < -20.0f) {
                meteor.active = false;
                meteor.entity->visible = false;
                continue;
            }

            meteor.entity->modelMatrix = getMeteorModelMatrix(meteor);
            meteorParticles.EmitSurfaceFire(&boulderModel, meteor.entity->getModelMatrix(), meteor.velocity, 45, 0.14f, 0.55f, 0.45f, 1.0f);
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
                    randomRange(8.0f, 13.0f),
                    randomRange(-12.0f, 10.0f)
                );
                airplane.velocity = glm::vec3(
                    randomRange(-2.8f, 2.8f),
                    randomRange(-4.2f, -2.6f),
                    randomRange(-2.0f, 2.4f)
                );
                airplane.rotation = glm::vec3(
                    randomRange(-0.4f, 0.4f),
                    randomRange(0.0f, 6.28f),
                    randomRange(-0.9f, 0.9f)
                );
                airplane.angularVelocity = glm::vec3(
                    randomRange(-0.7f, 0.7f),
                    randomRange(-0.4f, 0.4f),
                    randomRange(1.5f, 3.4f)
                );
                airplane.entity->modelMatrix = getAirplaneModelMatrix(airplane);
                airplane.entity->visible = true;
            }
            nextAirplaneSpawnTime = currentTime + randomRange(6.0f, 10.0f);
        }

        for (Airplane& airplane : airplanes) {
            if (!airplane.active) continue;

            airplane.position += airplane.velocity * deltaTime;
            airplane.rotation += airplane.angularVelocity * deltaTime;
            if (airplane.position.y < -5.0f || glm::abs(airplane.position.x) > 22.0f || airplane.position.z > 22.0f || airplane.position.z < -22.0f) {
                airplane.active = false;
                airplane.entity->visible = false;
                continue;
            }

            airplane.entity->modelMatrix = getAirplaneModelMatrix(airplane);
            meteorParticles.EmitSurfaceFire(airplane.entity->model, airplane.entity->getModelMatrix(), airplane.velocity, 55, 0.2f, 1.1f, 0.6f, 1.35f);
        }

        meteorParticles.Update(deltaTime);

        if (useCSM) {
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
                    if (!entity->visible) continue;
                    glm::mat4 modelMatrix = entity->getModelMatrix();
                    csmShader.setMat4("model", modelMatrix);
                    for (const SubMesh& subMesh : model->subMeshes) {
                        glBindVertexArray(subMesh.mesh.VAO);
                        glDrawElements(GL_TRIANGLES, subMesh.mesh.indices.size(), GL_UNSIGNED_INT, 0);
                    }
                }
            }
        }
        else {
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
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int currentWidth, currentHeight;
        glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
        glViewport(0, 0, currentWidth, currentHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        lightingShader.use();
        lightingShader.setFloat("useLighting", useLighting ? 1.0f : 0.0f);
        lightingShader.setFloat("useShadow", useShadow ? 1.0f : 0.0f);
        lightingShader.setFloat("usePCF", usePCF ? 1.0f : 0.0f);
        lightingShader.setFloat("useCSM", useCSM ? 1.0f : 0.0f);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        lightingShader.setVec3("light.dir", sun.lightDir);
        lightingShader.setVec3("light.color", sun.lightColor);
        lightingShader.setVec3("viewPos", camera.Position);
        lightingShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, depth.ID);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
        lightingShader.setInt("csmDepthMapSampler", 4);
        lightingShader.setInt("cascadeCount", shadowCascadeLevels.size());
        for (size_t i = 0; i < shadowCascadeLevels.size(); ++i) {
            lightingShader.setFloat("cascadePlaneDistances[" + std::to_string(i) + "]", shadowCascadeLevels[i]);
        }

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

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

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
