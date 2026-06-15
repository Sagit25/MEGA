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
#include "particle_system.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include "shared/scene_module.h"
#include "shared/fade_foreground.h"
#include "volcano_trajectory.h"


namespace volcano {

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

void init(GLFWwindow* window)
{
    if (initialized) return;
    initialized = true;
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glEnable(GL_DEPTH_TEST);

    // build and compile our shader program
    // ------------------------------------
    static Shader lightingShader("../shaders/shared/shader_lighting.vs", "../shaders/shared/shader_lighting.fs"); // you can name your shader files however you like
    static Shader shadowShader("../shaders/shared/shadow.vs", "../shaders/shared/shadow.fs");
    static Shader skyboxShader("../shaders/shared/shader_skybox.vs", "../shaders/shared/shader_skybox.fs");
    static Shader particleShader("../shaders/1-volcano/particle.vs", "../shaders/1-volcano/particle.fs");

    // define models
    static Model toothlessModel = Model("../resources/1-volcano/toothless/toothless.obj");
    static Model boulderModel("../resources/1-volcano/boulder/boulder.obj");
    boulderModel.setDiffuse("../resources/1-volcano/boulder/boulder_d.png");
    boulderModel.setNormal("../resources/1-volcano/boulder/boulder_n.png");
    static Model airplane1Model("../resources/1-volcano/airplane1/11803_Airplane_v1_l1.obj");
    static Model airplane2Model("../resources/1-volcano/airplane2/11804_Airplane_v2_l2.obj");
    static Model dragonModel("../resources/1-volcano/dragon/dragon.obj");
    dragonModel.setDiffuse("../resources/1-volcano/dragon/textures/Dragon_Bump_Col2.jpg");
    dragonModel.setNormal("../resources/1-volcano/dragon/textures/Dragon_Nor.jpg");
    static Model fireExtModel = Model("../resources/0-base/FireExt/FireExt.obj");
    fireExtModel.setDiffuse("../resources/0-base/FireExt/FireExt_d.jpg");
    fireExtModel.setSpecular("../resources/0-base/FireExt/FireExt_s.jpg");
    fireExtModel.setNormal("../resources/0-base/FireExt/FireExt_n.jpg");
    static Model houseModel = Model("../resources/0-base/room/Warehouse.obj", false, false);
    static Model sofaModel = Model("../resources/0-base/sofa/sofa.obj");
    static Model tableModel = Model("../resources/0-base/table/Center Table.obj");

    // Add entities to scene.
    static Scene scene;
    fadeForegroundEntities.clear();

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

    static Entity* toothlessEntity = new Entity(&toothlessModel, glm::vec3(1.0f, -3.0f, 1.0f) + fireSceneOffset, -90.0f, 180.0f, 0.0f, 0.05f);
    scene.addEntity(toothlessEntity);

    static Entity* dragonEntity = new Entity(&dragonModel, glm::mat4(1.0f));
    scene.addEntity(dragonEntity);

    static FireParticleSystem fireParticles(particleShader, 17000);
    static MeteorParticleSystem meteorParticles(particleShader, 5000);

    static std::vector<Meteor> meteors;
    const unsigned int maxMeteors = 8;
    for (unsigned int i = 0; i < maxMeteors; ++i) {
        Entity* meteorEntity = new Entity(&boulderModel, glm::mat4(1.0f));
        meteorEntity->visible = false;
        scene.addEntity(meteorEntity);

        Meteor meteor;
        meteor.entity = meteorEntity;
        meteors.push_back(meteor);
    }
    static float nextMeteorSpawnTime = 0.5f;

    static std::vector<Airplane> airplanes;
    static Entity* airplane1Entity = new Entity(&airplane1Model, glm::mat4(1.0f));
    airplane1Entity->visible = false;
    scene.addEntity(airplane1Entity);
    Airplane airplane1;
    airplane1.entity = airplane1Entity;
    airplane1.scale = 0.003f;
    airplanes.push_back(airplane1);

    static Entity* airplane2Entity = new Entity(&airplane2Model, glm::mat4(1.0f));
    airplane2Entity->visible = false;
    scene.addEntity(airplane2Entity);
    Airplane airplane2;
    airplane2.entity = airplane2Entity;
    airplane2.scale = 0.009f;
    airplanes.push_back(airplane2);
    static float nextAirplaneSpawnTime = 3.0f;

    // define depth texture
    static DepthMapTexture depth = DepthMapTexture(SHADOW_WIDTH, SHADOW_HEIGHT);
    fadeForegroundShader = &lightingShader;
    fadeForegroundDepth = &depth;

    // skybox (fire)
    std::vector<std::string> faces {
        "../resources/1-volcano/fireskybox/vulcan_bk.jpg",
        "../resources/1-volcano/fireskybox/vulcan_ft.jpg",
        "../resources/1-volcano/fireskybox/vulcan_up.jpg",
        "../resources/1-volcano/fireskybox/vulcan_dn.jpg",
        "../resources/1-volcano/fireskybox/vulcan_lf.jpg",
        "../resources/1-volcano/fireskybox/vulcan_rt.jpg"
    };
    static CubemapTexture skyboxTexture = CubemapTexture(faces);
    static unsigned int VAOskybox = 0, VBOskybox = 0;
    getPositionVAO(skybox_positions, sizeof(skybox_positions), VAOskybox, VBOskybox);

    lightingShader.use();
    lightingShader.setInt("material.diffuseSampler", 0);
    lightingShader.setInt("material.specularSampler", 1);
    lightingShader.setInt("material.normalSampler", 2);
    lightingShader.setInt("depthMapSampler", 3);
    lightingShader.setFloat("material.shininess", 64.f);

    skyboxShader.use();
    skyboxShader.setInt("skyboxTexture1", 0);

    static DirectionalLight sun(-90.0f, 45.0f, glm::vec3(1.0f));
    fadeForegroundSun = &sun;

    static float oldTime = static_cast<float>(glfwGetTime());
    static float flightTime = 0.0f;
    

    onEnterImpl = [&](GLFWwindow* window) {
        oldTime = static_cast<float>(glfwGetTime());
        firstMouse = true;
        flightTime = 0.0f;
        nextMeteorSpawnTime = oldTime + 0.5f;
        nextAirplaneSpawnTime = oldTime + 3.0f;
        for (Meteor& meteor : meteors) { meteor.active = false; if (meteor.entity) meteor.entity->visible = false; }
        for (Airplane& airplane : airplanes) { airplane.active = false; if (airplane.entity) airplane.entity->visible = false; }
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

        // input
        processInput(window, &sun);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 lightProjection = sun.getProjectionMatrix();
        glm::mat4 lightView = sun.getViewMatrix(camera.Position);
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        toothlessEntity->modelMatrix = getToothlessFlightModelMatrix(flightTime);
        dragonEntity->modelMatrix = getDragonFlightModelMatrix(flightTime);
        meteorParticles.EmitSurfaceFire(&dragonModel, dragonEntity->getModelMatrix(), getDragonFlightVelocity(flightTime), 48, 0.16f, 0.9f, 0.55f, 1.2f);

        // Spawn meteors at random intervals with random properties
        if (currentTime >= nextMeteorSpawnTime) {
            int spawnCount = 1 + rand() % 2;
            for (int i = 0; i < spawnCount; ++i) {
                for (Meteor& meteor : meteors) {
                    // Find the first inactive meteor and activate it with random properties
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

        // Update meteor positions and spawn fire particles along their trajectory
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

        // Spawn airplanes at random intervals with random properties
        if (currentTime >= nextAirplaneSpawnTime) {
            std::vector<int> inactiveAirplanes;
            for (int i = 0; i < static_cast<int>(airplanes.size()); ++i) {
                if (!airplanes[i].active) {
                    inactiveAirplanes.push_back(i);
                }
            }

            // If there are any inactive airplanes, activate one with random properties
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

        // Update airplane positions and spawn fire particles along their trajectory
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

        lightingShader.use();
        lightingShader.setFloat("useLighting", 1.0f);
        lightingShader.setFloat("useShadow", 1.0f);
        lightingShader.setFloat("usePCF", 1.0f);

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
                    lightingShader.setVec3("baseColor", subMesh.baseColor);
                    lightingShader.setFloat("useDiffuseMap", subMesh.diffuse ? 1.0f : 0.0f);
                    lightingShader.setFloat("useSpecularMap", subMesh.specular ? 1.0f : 0.0f);
                    lightingShader.setFloat("useNormalMap", subMesh.normal ? 1.0f : 0.0f);

                    glActiveTexture(GL_TEXTURE0);
                    if (subMesh.diffuse) {
                        glBindTexture(GL_TEXTURE_2D, subMesh.diffuse->ID);
                    } else {
                        glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                    }
                    glActiveTexture(GL_TEXTURE1);
                    if (subMesh.specular) {
                        glBindTexture(GL_TEXTURE_2D, subMesh.specular->ID);
                    } else {
                        glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
                    }
                    glActiveTexture(GL_TEXTURE2);
                    if (subMesh.normal) {
                        glBindTexture(GL_TEXTURE_2D, subMesh.normal->ID);
                    } else {
                        glBindTexture(GL_TEXTURE_2D, Texture::GetDummyTexture());
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
    return { "volcano", init, onEnter, renderFrame, renderFadeForeground, framebuffer_size_callback, mouse_callback, scroll_callback, getCameraPose, getDefaultCameraPose, setCameraPose };
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

    // key F/G: adjust Toothless fire spawn rate
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_F] == false) {
        isKeyboardDone[GLFW_KEY_F] = true;
        fireSpawnRate = std::min(fireSpawnRate + 25, 1000u);
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE) {
        isKeyboardDone[GLFW_KEY_F] = false;
    }

    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_G] == false) {
        isKeyboardDone[GLFW_KEY_G] = true;
        fireSpawnRate = fireSpawnRate >= 25 ? fireSpawnRate - 25 : 0;
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
    (void)window;
    (void)xoffset;
    (void)yoffset;
}

} // namespace volcano

#ifndef COMBINED_SCENE_APP
int main()
{
    return volcano::runStandalone();
}
#endif
