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
#include <map>
#include "camera.h"
#include "texture.h"
#include "texture_cube.h"
#include "model.h"
#include "mesh.h"
#include "scene.h"
#include "math_utils.h"
#include "light.h"

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
float deltaTime = 0.0f;	
float lastFrame = 0.0f;

bool useNormalMap = true;
bool useSpecular = false;
bool useLighting = true;
bool useShadow = true;
bool usePCF = false;
bool useCSM = false;

unsigned int lightFBO;
unsigned int lightDepthMaps;
constexpr unsigned int depthMapResolution = 4096;
std::vector<float> shadowCascadeLevels{4.0f, 10.0f, 50.0f};

std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& projview) {
    const auto inv = glm::inverse(projview);
    std::vector<glm::vec4> frustumCorners;
    for (unsigned int x = 0; x < 2; ++x) {
        for (unsigned int y = 0; y < 2; ++y) {
            for (unsigned int z = 0; z < 2; ++z) {
                const glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }
    return frustumCorners;
}

std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
    return getFrustumCornersWorldSpace(proj * view);
}

glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane, DirectionalLight* light) {
    const auto proj = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, nearPlane, farPlane);
    const auto corners = getFrustumCornersWorldSpace(proj, camera.GetViewMatrix());

    glm::vec3 center = glm::vec3(0, 0, 0);
    for (const auto& v : corners) center += glm::vec3(v);
    center /= corners.size();

    const auto lightView = glm::lookAt(center - light->lightDir, center, glm::vec3(0.0f, 1.0f, 0.0f));

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const auto& v : corners) {
        const auto trf = lightView * v;
        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
    }

    constexpr float zMult = 10.0f;
    if (minZ < 0) minZ *= zMult; else minZ /= zMult;
    if (maxZ < 0) maxZ /= zMult; else maxZ *= zMult;

    const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    return lightProjection * lightView;
}

std::vector<glm::mat4> getLightSpaceMatrices(DirectionalLight* light) {
    std::vector<glm::mat4> ret;
    float cameraNearPlane = 0.1f;
    float cameraFarPlane = 100.0f;
    for (size_t i = 0; i < shadowCascadeLevels.size() + 1; ++i) {
        if (i == 0) ret.push_back(getLightSpaceMatrix(cameraNearPlane, shadowCascadeLevels[i], light));
        else if (i < shadowCascadeLevels.size()) ret.push_back(getLightSpaceMatrix(shadowCascadeLevels[i - 1], shadowCascadeLevels[i], light));
        else ret.push_back(getLightSpaceMatrix(shadowCascadeLevels[i - 1], cameraFarPlane, light));
    }
    return ret;
}


int main()
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

    glEnable(GL_DEPTH_TEST);

    Shader lightingShader("../shaders/shader_lighting.vs", "../shaders/shader_lighting.fs"); 
    Shader shadowShader("../shaders/shadow.vs", "../shaders/shadow.fs");
    Shader skyboxShader("../shaders/shader_skybox.vs", "../shaders/shader_skybox.fs");
    Shader csmShader("../shaders/csm.vs", "../shaders/csm.fs", "../shaders/csm.gs");

    // Models Setup
    Model brickCubeModel = Model("../resources/brickcube/brickcube.obj");
    if(!brickCubeModel.subMeshes.empty()) {
        brickCubeModel.subMeshes[0].diffuse = new Texture("../resources/brickcube/brickcube_d.png");
        brickCubeModel.subMeshes[0].specular = new Texture("../resources/brickcube/brickcube_s.png");
        brickCubeModel.subMeshes[0].normal = new Texture("../resources/brickcube/brickcube_n.png");
    }

    Model boulderModel("../resources/boulder/boulder.obj");
    if(!boulderModel.subMeshes.empty()) {
        boulderModel.subMeshes[0].diffuse = new Texture("../resources/boulder/boulder_d.png");
        boulderModel.subMeshes[0].normal = new Texture("../resources/boulder/boulder_n.png");
    }

    Model grassGroundModel = Model("../resources/plane.obj", true); // ignoreShadow = true
    if(!grassGroundModel.subMeshes.empty()) {
        grassGroundModel.subMeshes[0].diffuse = new Texture("../resources/grass_ground.jpg");
    }
    
    Model barrelModel = Model("../resources/barrel/barrel.obj");
    if(!barrelModel.subMeshes.empty()) {
        barrelModel.subMeshes[0].diffuse = new Texture("../resources/barrel/barrel_d.png");
        barrelModel.subMeshes[0].specular = new Texture("../resources/barrel/barrel_s.png");
        barrelModel.subMeshes[0].normal = new Texture("../resources/barrel/barrel_n.png");
    }

    Model fireExtModel = Model("../resources/FireExt/FireExt.obj");
    if(!fireExtModel.subMeshes.empty()) {
        fireExtModel.subMeshes[0].diffuse = new Texture("../resources/FireExt/FireExt_d.jpg");
        fireExtModel.subMeshes[0].specular = new Texture("../resources/FireExt/FireExt_s.jpg");
        fireExtModel.subMeshes[0].normal = new Texture("../resources/FireExt/FireExt_n.jpg");
    }

    // Assimp가 내부의 여러 파트와 텍스처를 자동으로 로드합니다. (수동 할당 불필요)
    Model toothlessModel = Model("../resources/toothless/toothless.obj");

    // Scene
    Scene scene;
    // scene.addEntity(new Entity(&brickCubeModel, glm::mat4(1.0)));
    // scene.addEntity(new Entity(&brickCubeModel, glm::translate(glm::vec3(-3.5f, 0.0f, -2.0f)) * glm::rotate(glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f))));
    // scene.addEntity(new Entity(&brickCubeModel, glm::translate(glm::vec3(1.0f, 0.5f, -3.0f)) * glm::rotate(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f))));
    // scene.addEntity(new Entity(&barrelModel, glm::vec3(2.5f, 0.0f, -2.0f), 0, 0, 0, 0.1f));

    glm::mat4 planeWorldTransform = glm::mat4(1.0f);
    planeWorldTransform = glm::scale(planeWorldTransform, glm::vec3(planeSize));
    planeWorldTransform = glm::translate(glm::vec3(0.0f, -0.5f, 0.0f)) * planeWorldTransform;
    // scene.addEntity(new Entity(&grassGroundModel, planeWorldTransform));

    // scene.addEntity(new Entity(&fireExtModel, glm::vec3(2,-1,0), 0.0f, 180.0f, 0.0f, 0.002f));
    // scene.addEntity(new Entity(&boulderModel, glm::vec3(-5, 0, 2), 0.0f, 180.0f, 0.0f, 0.1));

    scene.addEntity(new Entity(&toothlessModel, glm::rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), -90.0f, 180.0f, 0.0f, 0.05f));

    // depth texture
    DepthMapTexture depth = DepthMapTexture(SHADOW_WIDTH, SHADOW_HEIGHT);

    // skybox
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
    while (!glfwWindowShouldClose(window)) 
    {
        float currentTime = glfwGetTime();
        float dt = currentTime - oldTime;
        deltaTime = dt;
        oldTime = currentTime;

        processInput(window, &sun);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // (1) 섀도우 맵 렌더링 패스
        glm::mat4 lightProjection = sun.getProjectionMatrix();
        glm::mat4 lightView = sun.getViewMatrix(camera.Position);
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

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
                    glm::mat4 modelMatrix = entity->getModelMatrix();
                    csmShader.setMat4("model", modelMatrix);
                    
                    // Assimp 구조: 모든 SubMesh를 순회
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

        // (2) 라이팅 렌더링 패스
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
                glm::mat4 modelMatrix = entity->getModelMatrix();
                lightingShader.setMat4("world", modelMatrix);

                // 파트별로 텍스처를 바인딩하며 그리기
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

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window, DirectionalLight* sun) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);

    float t = 20.0f * deltaTime;
    
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) sun->processKeyboard(0.0f, t);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) sun->processKeyboard(0.0f, -t);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) sun->processKeyboard(-t, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) sun->processKeyboard(t, 0.0f);

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_1] == false) {
        isKeyboardDone[GLFW_KEY_1] = true; useNormalMap = !useNormalMap;
    }
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE) isKeyboardDone[GLFW_KEY_1] = false;

    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_2] == false) {
        isKeyboardDone[GLFW_KEY_2] = true; useShadow = !useShadow;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE) isKeyboardDone[GLFW_KEY_2] = false;

    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_3] == false) {
        isKeyboardDone[GLFW_KEY_3] = true; useLighting = !useLighting;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_RELEASE) isKeyboardDone[GLFW_KEY_3] = false;

    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_4] == false) {
        isKeyboardDone[GLFW_KEY_4] = true; usePCF = !usePCF;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_RELEASE) isKeyboardDone[GLFW_KEY_4] = false;

    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS && isKeyboardDone[GLFW_KEY_5] == false) {
        isKeyboardDone[GLFW_KEY_5] = true; useCSM = !useCSM;
    }
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_RELEASE) isKeyboardDone[GLFW_KEY_5] = false;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos; lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(yoffset);
}