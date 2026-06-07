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
Camera camera(glm::vec3(0.0f, 1.5f, 0.0f));
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
    Shader skyboxShader("../shaders/shader_skybox.vs", "../shaders/shader_skybox.fs");


    // define models
    Model brickCubeModel("../resources/brickcube/brickcube.obj");
    brickCubeModel.setDiffuse("../resources/brickcube/brickcube_d.png");
    brickCubeModel.setSpecular("../resources/brickcube/brickcube_s.png");
    brickCubeModel.setNormal("../resources/brickcube/brickcube_n.png");

    Model boulderModel("../resources/boulder/boulder.obj");
    boulderModel.setDiffuse("../resources/boulder/boulder_d.png");
    boulderModel.setNormal("../resources/boulder/boulder_n.png");

    Model grassGroundModel = Model("../resources/plane.obj", true);
    grassGroundModel.setDiffuse("../resources/grass_ground.jpg");

    Model barrelModel = Model("../resources/barrel/barrel.obj");
    barrelModel.setDiffuse("../resources/barrel/barrel_d.png");
    barrelModel.setSpecular("../resources/barrel/barrel_s.png");
    barrelModel.setNormal("../resources/barrel/barrel_n.png");

    Model fireExtModel = Model("../resources/FireExt/FireExt.obj");
    fireExtModel.setDiffuse("../resources/FireExt/FireExt_d.jpg");
    fireExtModel.setSpecular("../resources/FireExt/FireExt_s.jpg");
    fireExtModel.setNormal("../resources/FireExt/FireExt_n.jpg");

    Model catModel = Model("../resources/cat/12221_Cat_v1_l3.obj");
    catModel.setDiffuse("../resources/cat/Cat_diffuse.jpg");

    Model roomModel = Model("../resources/room/small_house_obj.obj");
    Model houseModel = Model("../resources/room/Warehouse.obj");
    Model sofaModel = Model("../resources/sofa/sofa.obj");
    Model tableModel = Model("../resources/table/Center Table.obj");



    // Add entities to scene.
    // you can change the position/orientation.
    Scene scene;
    

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

    scene.addEntity(new Entity(&fireExtModel, rotateInHouse(glm::vec3(-1.5f, 0.0f, -2.5f) + sceneOffset), 0.0f, 180.0f + furnitureTurnY, 0.0f, 0.001f));
    // scene.addEntity(new Entity(&barrelModel, rotateInHouse(glm::vec3(-2.0f, 0.54f, 5.0f) + sceneOffset), 0, 0 + furnitureTurnY, 0, 0.05f));
    // scene.addEntity(new Entity(&catModel, rotateInHouse(glm::vec3(3.8f, 0.0f, -5.0f) + sceneOffset), -90.0f, 0.0f + furnitureTurnY, 0.0f, 0.02f));
    // scene.addEntity(new Entity(&roomModel, rotateInHouse(glm::vec3(-4.0f, 0.0f, 4.0f) + sceneOffset), 0.0f, 90.0f + furnitureTurnY, 0.0f, 0.02f));
    scene.addEntity(new Entity(&houseModel, housePosition, 0.0f, 90.0f, 0.0f, 1.0f));
    scene.addEntity(new Entity(&sofaModel, rotateInHouse(glm::vec3(-0.5f, 0.1f, -3.5f) + sceneOffset), 0.0f, 0.0f + furnitureTurnY, 0.0f, 0.5f));
    scene.addEntity(new Entity(&tableModel, rotateInHouse(glm::vec3(4.5f, 0.0f, -3.0f) + sceneOffset), 0.0f, 0.0f + furnitureTurnY, 0.0f, 1.2f));

    // define depth texture
    DepthMapTexture depth = DepthMapTexture(SHADOW_WIDTH, SHADOW_HEIGHT);


    // skybox
    std::vector<std::string> faces
    {
        "../resources/skybox/right.jpg",
        "../resources/skybox/left.jpg",
        "../resources/skybox/top.jpg",
        "../resources/skybox/bottom.jpg",
        "../resources/skybox/front.jpg",
        "../resources/skybox/back.jpg"
    };
    CubemapTexture skyboxTexture = CubemapTexture(faces);
    unsigned int VAOskybox, VBOskybox;
    getPositionVAO(skybox_positions, sizeof(skybox_positions), VAOskybox, VBOskybox);

    lightingShader.use();
    lightingShader.setInt("material.diffuseSampler", 0);
    lightingShader.setInt("material.specularSampler", 1);
    lightingShader.setInt("material.normalSampler", 2);
    lightingShader.setInt("depthMapSampler", 3);
    lightingShader.setFloat("material.shininess", 64.f);    // set shininess to constant value.

    skyboxShader.use();
    skyboxShader.setInt("skyboxTexture1", 0);

    DirectionalLight sun(-90.0f, 45.0f, glm::vec3(1.0f));

    float oldTime = 0;
    while (!glfwWindowShouldClose(window))// render loop
    {
        float currentTime = glfwGetTime();
        float dt = currentTime - oldTime;
        deltaTime = dt;
        oldTime = currentTime;

        // input
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
        // set use lighting, use shadow, usePCF to shader
        lightingShader.use();
        if (useLighting) lightingShader.setFloat("useLighting", 1.0f);
        else lightingShader.setFloat("useLighting", 0.0f);
        if (useShadow) lightingShader.setFloat("useShadow", 1.0f);
        else lightingShader.setFloat("useShadow", 0.0f);
        if (usePCF) lightingShader.setFloat("usePCF", 1.0f);
        else lightingShader.setFloat("usePCF", 0.0f);

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
                if (subMesh.normal && useNormalMap) {
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
