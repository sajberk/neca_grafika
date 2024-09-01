#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void renderQuad();
unsigned int loadCubemap(vector<std::string> faces);
unsigned int loadTexture(char const * path);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
bool bloom = true;
bool bloomKeyPressed = false;
bool hdr = true; //todo: ovo je uvek true sad a, al pronadji kul vrednost za exposure
float exposure = 0.07f;

// camera
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct Spotlight {
    glm::vec3 position;
    glm::vec3 direction;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;
};

struct PointLight {
    glm::vec3 position;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);//glm::vec3(0.4);
    bool ImGuiEnabled = true;
    Camera worldCamera;
    Camera drivingCamera;
    bool CameraMouseMovementUpdateEnabled = true;
    bool isDrivingMode = true;
    glm::vec3 truckPosition = glm::vec3(0.0f);
    glm::vec3 truckForward = glm::vec3(0.0f, 0.0f, -1.0f);
    float currentTruckSpeed = 0.0f;
    float currentTruckSteer = 0.0f;
    Spotlight leftHeadlight;
    Spotlight rightHeadlight;
    ProgramState()
            : worldCamera(glm::vec3(4.0f, 4.0f, 2.0f), glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, -35.0f),
              drivingCamera(glm::vec3(0.0f, 1.1f, -0.8f), glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f) {}
};

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSwapInterval(1);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(false);

    programState = new ProgramState;
    //programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;



    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile shaders
    // -------------------------
    Shader ourShader("resources/shaders/2.model_lighting.vs", "resources/shaders/2.model_lighting.fs");
    Shader windshieldShader("resources/shaders/2.model_lighting.vs", "resources/shaders/windshieldShader.fs");
    Shader hdrShader("resources/shaders/hdrShader.vs", "resources/shaders/hdrShader.fs");
    Shader blurShader("resources/shaders/hdrShader.vs", "resources/shaders/blurShader.fs");
    Shader bloomFinalShader("resources/shaders/hdrShader.vs", "resources/shaders/bloomFinalShader.fs");
    Shader skyboxShader("resources/shaders/skyboxShader.vs", "resources/shaders/skyboxShader.fs");

    // load models
    // ---------
    Model truck("resources/objects/truck/truck.obj");
    truck.SetShaderTextureNamePrefix("material.");
    Model wall("resources/objects/wall/10061_Wall_SG_V2_Iterations-2.obj");
    wall.SetShaderTextureNamePrefix("material.");
    Model oshawott("resources/objects/oshawott/model.obj");
    oshawott.SetShaderTextureNamePrefix("material.");

    // hdr stuff?
    // -----------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    // Create two color buffers (one for normal rendering, other for bright colors)
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    // Create and attach a renderbuffer object for depth and stencil attachment (we won't be sampling these)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    // Tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }


    // lighting info
    // -------------
    Spotlight leftHeadlight, rightHeadlight;
    leftHeadlight.position = glm::vec3(0);
    leftHeadlight.direction = glm::vec3(0.0f, 0.0f, -1.0f);

    leftHeadlight.ambient = glm::vec3(0.05f);
    leftHeadlight.diffuse = glm::vec3(1.2f);
    leftHeadlight.specular = glm::vec3(2.0f);

    leftHeadlight.constant = 1.0f;
    leftHeadlight.linear = 0.09f;
    leftHeadlight.quadratic = 0.032f;

    leftHeadlight.cutOff = glm::cos(glm::radians(25.0f));
    leftHeadlight.outerCutOff = glm::cos(glm::radians(40.0f));

    rightHeadlight.position = glm::vec3(0);
    rightHeadlight.direction = glm::vec3(0.0f, 0.0f, -1.0f);

    rightHeadlight.ambient = glm::vec3(0.05f);
    rightHeadlight.diffuse = glm::vec3(1.2f);
    rightHeadlight.specular = glm::vec3(2.0f);

    rightHeadlight.constant = 1.00f;
    rightHeadlight.linear = 0.09f;
    rightHeadlight.quadratic = 0.032f;

    rightHeadlight.cutOff = glm::cos(glm::radians(25.0f));
    rightHeadlight.outerCutOff = glm::cos(glm::radians(40.0f));

    PointLight tempSvetlo;
    // poludecu od ovih svetala i sve cu ih promeniti cim skejl daunujem modele
    tempSvetlo.position = glm::vec3(0.0f, 10.0f, 0.0f);
    tempSvetlo.ambient = glm::vec3(0.01f);
    tempSvetlo.diffuse = glm::vec3(0.05f);
    tempSvetlo.specular = glm::vec3(0.1f);

    tempSvetlo.constant = 0.1f;
    tempSvetlo.linear = 0.0045f;
    tempSvetlo.quadratic = 0.00032f;

    // load skybox and stuff
    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    vector<std::string> faces
    {
        FileSystem::getPath("resources/textures/skybox/sky_night_right.png"),
        FileSystem::getPath("resources/textures/skybox/sky_night_left.png"),
        FileSystem::getPath("resources/textures/skybox/sky_night_up.png"),
        FileSystem::getPath("resources/textures/skybox/sky_night_bottom.png"),
        FileSystem::getPath("resources/textures/skybox/sky_night_front.png"),
        FileSystem::getPath("resources/textures/skybox/sky_night_back.png")
    };

    unsigned int cubemapTexture = loadCubemap(faces);

    float groundVertices[] = {
        // positions          // normals          // texture coordinates
        -10.0f, 0.0f, -10.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
        10.0f, 0.0f, -10.0f,  0.0f, 1.0f, 0.0f,  10.0f, 0.0f,
        10.0f, 0.0f,  10.0f,  0.0f, 1.0f, 0.0f,  10.0f, 10.0f,

        -10.0f, 0.0f, -10.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,
        10.0f, 0.0f,  10.0f,  0.0f, 1.0f, 0.0f,  10.0f, 10.0f,
        -10.0f, 0.0f,  10.0f,  0.0f, 1.0f, 0.0f,  0.0f, 10.0f
    };

    unsigned int groundTextureID = loadTexture("resources/textures/dirt/30.png");
    unsigned int groundVAO, groundVBO;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), &groundVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));


    // shader configuration
    // --------------------
    hdrShader.use();
    hdrShader.setInt("hdrBuffer", 0);
    blurShader.use();
    blurShader.setInt("image", 0);
    bloomFinalShader.use();
    bloomFinalShader.setInt("scene", 0);
    bloomFinalShader.setInt("bloomBlur", 1);
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    // pokemoni
    // --------
    int pokemonCount = 50;
    std::vector<glm::mat4> pokemoni;
    srand(static_cast<unsigned>(time(0)));

    for (int i = 0; i < pokemonCount; i++) {
        float pokemonSpawnZone = 10.0f;
        float x = -pokemonSpawnZone + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (2 * pokemonSpawnZone)));
        float z = -pokemonSpawnZone + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (2 * pokemonSpawnZone)));
        glm::mat4 pokemon = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z));
        pokemoni.push_back(pokemon);
    }

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // novo
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Camera& activeCamera = programState->isDrivingMode ? programState->drivingCamera : programState->worldCamera;

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(activeCamera.Zoom), (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.2f, 100.0f);
        glm::mat4 view = activeCamera.GetViewMatrix();

        // draw skybox
        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(activeCamera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // set depth function back to default

        // now normal shader time
        // view/projection transformations
        projection = glm::perspective(glm::radians(activeCamera.Zoom), (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.2f, 100.0f);
        view = activeCamera.GetViewMatrix();
        ourShader.use();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setMat4("model", glm::mat4(1.0f));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, groundTextureID);
        ourShader.setInt("texture1", 0);

        glBindVertexArray(groundVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        // loading models
        // --------------
        // wott
        glm::mat4 oshawottModel = glm::mat4(1.0f);
        ourShader.setMat4("model", oshawottModel);
        oshawott.Draw(ourShader);

        // wottotachi
        for (glm::mat4& pokemon : pokemoni) {
            ourShader.setMat4("model", pokemon);
            oshawott.Draw(ourShader);
        }


        // truck
        glm::mat4 truckModel = glm::mat4(1.0f);

        // model je blesav pa ga rotiramo da bude lepo orijentisan
        const float truckRotOffsetX = -M_PI * 0.5f;
        const float truckRotOffsetZ = M_PI * 0.5f;

        truckModel = glm::translate(truckModel, programState -> truckPosition);
        truckModel = glm::scale(truckModel, glm::vec3(0.1f));

        truckModel = glm::rotate(truckModel, programState -> currentTruckSteer, glm::vec3(0, 1, 0));
        truckModel = glm::rotate(truckModel, truckRotOffsetX, glm::vec3(1, 0, 0));
        truckModel = glm::rotate(truckModel, truckRotOffsetZ, glm::vec3(0, 0, 1));

        // kamionov forward vector je 1 0 0 iz nekog razloga nemam pojma mnogo su haoticne rotacije i ne sredjuje mi se to
        programState -> truckForward = glm::normalize(glm::vec3(truckModel * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));

        ourShader.setMat4("model", truckModel);
        truck.Draw(ourShader);

        // farovi
        glm::mat4 headlightModel = glm::mat4(1.0f);

        // ubijemo originalne kamionove rotacije koje cemo ispod primeniti na svetla
        headlightModel = glm::rotate(headlightModel, -truckRotOffsetZ, glm::vec3(0, 0, 1));
        headlightModel = glm::rotate(headlightModel, -truckRotOffsetX, glm::vec3(1, 0, 0));

        // al takodje rotiramo malo dole jer su ovo farovi pa kao gledaju u put
        headlightModel = glm::rotate(headlightModel, -0.5f, glm::vec3(1, 0, 0));

        // ovo je kao radilo
        leftHeadlight.position = glm::vec3(truckModel * headlightModel * glm::vec4(-5.0f, 7.0f, -15.0f, 1.0f));
        rightHeadlight.position = glm::vec3(truckModel * headlightModel * glm::vec4(5.0f, 7.0f, -15.0f, 1.0f));

        //leftHeadlight.position = programState -> camera.Position;
        leftHeadlight.direction = programState -> truckForward;
        rightHeadlight.direction = programState -> truckForward;

        // sad ih i renderujemo
        float leftHeadlightVertices[] = {
            // Front face (two triangles)
            -0.45f, 0.6f,  -1.5f,  // Bottom left
            -0.25f, 0.6f,  -1.5f,  // Bottom right
            -0.45f,  0.8f,  -1.45f,  // Top left
            -0.25f,  0.8f,  -1.45f,  // Top right

            // Back face (two triangles)
            -0.45f, 0.6f,  -1.4f,  // Bottom left
            -0.25f, 0.6f,  -1.4f,  // Bottom right
            -0.45f,  0.8f,  -1.35f,  // Top left
            -0.25f,  0.8f,  -1.35f,  // Top right

            // Left side face (two triangles)
            -0.45f, 0.6f,  -1.5f,  // Front bottom left
            -0.45f, 0.8f,  -1.45f,  // Front top left
            -0.45f, 0.6f,  -1.4f,  // Back bottom left
            -0.45f, 0.8f,  -1.35f,  // Back top left

            // Right side face (two triangles)
            -0.25f, 0.6f,  -1.45f,  // Front bottom right
            -0.25f, 0.8f,  -1.4f,  // Front top right
            -0.25f, 0.6f,  -1.35f,  // Back bottom right
            -0.25f, 0.8f,  -1.3f,  // Back top right

            // Top face (two triangles)
            -0.45f,  0.8f,  -1.4f,  // Front left
            -0.25f,  0.8f,  -1.4f,  // Front right
            -0.45f,  0.8f,  -1.3f,  // Back left
            -0.25f,  0.8f,  -1.3f,  // Back right

            // Bottom face (two triangles)
            -0.45f, 0.6f,  -1.5f,  // Front left
            -0.25f, 0.6f,  -1.5f,  // Front right
            -0.45f, 0.6f,  -1.4f,  // Back left
            -0.25f, 0.6f,  -1.4f   // Back right
        };

        float rightHeadlightVertices[] = {
            // Front face (two triangles)
            0.45f, 0.6f,  -1.5f,  // Bottom left
            0.25f, 0.6f,  -1.5f,  // Bottom right
            0.45f,  0.8f,  -1.45f,  // Top left
            0.25f,  0.8f,  -1.45f,  // Top right

            // Back face (two triangles)
            0.45f, 0.6f,  -1.4f,  // Bottom left
            0.25f, 0.6f,  -1.4f,  // Bottom right
            0.45f,  0.8f,  -1.35f,  // Top left
            0.25f,  0.8f,  -1.35f,  // Top right

            // Left side face (two triangles)
            0.45f, 0.6f,  -1.5f,  // Front bottom left
            0.45f, 0.8f,  -1.45f,  // Front top left
            0.45f, 0.6f,  -1.4f,  // Back bottom left
            0.45f, 0.8f,  -1.35f,  // Back top left

            // Right side face (two triangles)
            0.25f, 0.6f,  -1.45f,  // Front bottom right
            0.25f, 0.8f,  -1.4f,  // Front top right
            0.25f, 0.6f,  -1.35f,  // Back bottom right
            0.25f, 0.8f,  -1.3f,  // Back top right

            // Top face (two triangles)
            0.45f,  0.8f,  -1.4f,  // Front left
            0.25f,  0.8f,  -1.4f,  // Front right
            0.45f,  0.8f,  -1.3f,  // Back left
            0.25f,  0.8f,  -1.3f,  // Back right

            // Bottom face (two triangles)
            0.45f, 0.6f,  -1.5f,  // Front left
            0.25f, 0.6f,  -1.5f,  // Front right
            0.45f, 0.6f,  -1.4f,  // Back left
            0.25f, 0.6f,  -1.4f   // Back right
        };

        glm::mat4 headlightPhysical = glm::mat4(1.0f);
        headlightPhysical = glm::rotate(headlightPhysical, -truckRotOffsetZ, glm::vec3(0, 0, 1));
        headlightPhysical = glm::rotate(headlightPhysical, -truckRotOffsetX, glm::vec3(1, 0, 0));
        headlightPhysical = glm::scale(headlightPhysical, glm::vec3(10.0f));
        headlightPhysical = truckModel * headlightPhysical;
//        leftHeadlightPhysical = glm::translate(leftHeadlightPhysical, glm::vec3(-5.0f, 7.0f, -15.0f));
  //      leftHeadlightPhysical = glm::rotate(leftHeadlightPhysical, glm::radians(leftHeadlight.direction.x), glm::vec3(1.0f, 0.0f, 0.0f));
    //    leftHeadlightPhysical = glm::rotate(leftHeadlightPhysical, glm::radians(leftHeadlight.direction.y), glm::vec3(0.0f, 1.0f, 0.0f));
      //  leftHeadlightPhysical = glm::rotate(leftHeadlightPhysical, glm::radians(leftHeadlight.direction.z), glm::vec3(0.0f, 0.0f, 1.0f));


        windshieldShader.use();
        windshieldShader.setMat4("projection", projection);
        windshieldShader.setMat4("view", view);
        windshieldShader.setMat4("model", headlightPhysical);
        windshieldShader.setVec4("windshieldColor", glm::vec4(5.0f));

        unsigned int VAO, VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(leftHeadlightVertices), leftHeadlightVertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);

        glBindVertexArray(VAO);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);   // Front face
        glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);   // Back face
        glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);   // Left side
        glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);  // Right side
        glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);  // Top face
        glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);  // Bottom face

        glBufferData(GL_ARRAY_BUFFER, sizeof(rightHeadlightVertices), rightHeadlightVertices, GL_STATIC_DRAW);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);   // Front face
        glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);   // Back face
        glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);   // Left side
        glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);  // Right side
        glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);  // Top face
        glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);  // Bottom face

        //std::cout << leftHeadlight.position.x << " " << leftHeadlight.position.y << " " << leftHeadlight.position.z << std::endl;;
        //std::cout << rightHeadlight.position.x << " " <<rightHeadlight.position.y << " " << rightHeadlight.position.z << std::endl<<std::endl;;
        ourShader.use();
        ourShader.setVec3("leftHeadlight.position", leftHeadlight.position);
        ourShader.setVec3("leftHeadlight.direction", leftHeadlight.direction);
        ourShader.setVec3("leftHeadlight.ambient", leftHeadlight.ambient);
        ourShader.setVec3("leftHeadlight.diffuse", leftHeadlight.diffuse);
        ourShader.setVec3("leftHeadlight.specular", leftHeadlight.specular);
        ourShader.setFloat("leftHeadlight.constant", leftHeadlight.constant);
        ourShader.setFloat("leftHeadlight.linear", leftHeadlight.linear);
        ourShader.setFloat("leftHeadlight.quadratic", leftHeadlight.quadratic);
        ourShader.setFloat("leftHeadlight.cutOff", leftHeadlight.cutOff);
        ourShader.setFloat("leftHeadlight.outerCutOff", leftHeadlight.outerCutOff);

        ourShader.setVec3("rightHeadlight.position", rightHeadlight.position);
        ourShader.setVec3("rightHeadlight.direction", rightHeadlight.direction);
        ourShader.setVec3("rightHeadlight.ambient", rightHeadlight.ambient);
        ourShader.setVec3("rightHeadlight.diffuse", rightHeadlight.diffuse);
        ourShader.setVec3("rightHeadlight.specular", rightHeadlight.specular);
        ourShader.setFloat("rightHeadlight.constant", rightHeadlight.constant);
        ourShader.setFloat("rightHeadlight.linear", rightHeadlight.linear);
        ourShader.setFloat("rightHeadlight.quadratic", rightHeadlight.quadratic);
        ourShader.setFloat("rightHeadlight.cutOff", rightHeadlight.cutOff);
        ourShader.setFloat("rightHeadlight.outerCutOff", rightHeadlight.outerCutOff);

        // ovo treba i za farove da vratim ovde lmao (sem pozicije i smera)
        ourShader.setVec3("tempSvetlo.position", tempSvetlo.position);
        ourShader.setVec3("tempSvetlo.ambient", tempSvetlo.ambient);
        ourShader.setVec3("tempSvetlo.diffuse", tempSvetlo.diffuse);
        ourShader.setVec3("tempSvetlo.specular", tempSvetlo.specular);
        ourShader.setFloat("tempSvetlo.constant", tempSvetlo.constant);
        ourShader.setFloat("tempSvetlo.linear", tempSvetlo.linear);
        ourShader.setFloat("tempSvetlo.quadratic", tempSvetlo.quadratic);

        ourShader.setVec3("viewPosition", activeCamera.Position);

        ourShader.setFloat("material.shininess", 32.0f);

        // nz gde planiram ovo da stavim ako izbacim zid jaoo
        glEnable(GL_CULL_FACE);

        // wall
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, -2.5f));
        model = glm::scale(model, glm::vec3(0.01));
        model = glm::rotate(model, -3.14f*0.5f, glm::vec3(1, 0, 0));

        ourShader.setMat4("model", model);
        wall.Draw(ourShader);

        glDisable(GL_CULL_FACE);

        // sofersajbna
        std::vector<glm::vec3> windshieldVertices = {
                glm::vec3(-0.35f, 0.9f, -1.35f),  // dole levo
                glm::vec3(0.3f, 0.9f, -1.35f),   // dole desno
                glm::vec3(0.3f, 1.35f, -1.2f),  // gore desno
                glm::vec3(-0.35f, 1.35f, -1.2f)  // gore levo
        };

        unsigned int windshieldVAO, windshieldVBO;
        glGenVertexArrays(1, &windshieldVAO);
        glGenBuffers(1, &windshieldVBO);
        glBindVertexArray(windshieldVAO);
        glBindBuffer(GL_ARRAY_BUFFER, windshieldVBO);
        glBufferData(GL_ARRAY_BUFFER, windshieldVertices.size() * sizeof(glm::vec3), &windshieldVertices[0], GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glm::mat4 windshieldModel = glm::mat4(1.0f);
        windshieldModel = glm::rotate(windshieldModel, -truckRotOffsetZ, glm::vec3(0, 0, 1));
        windshieldModel = glm::rotate(windshieldModel, -truckRotOffsetX, glm::vec3(1, 0, 0));
        windshieldModel = glm::scale(windshieldModel, glm::vec3(10.0f));
        windshieldModel = truckModel * windshieldModel;

        // cam // boze nemam pojma zasto je ovo na ovom mestu al plasim se da pomeram bilo sta vise tkd todo:
        if (programState -> isDrivingMode)  {
            glm::mat4 steeringRotation = glm::rotate(glm::mat4(1.0f), programState->currentTruckSteer, glm::vec3(0, 1, 0));
            glm::vec3 targetPosition = programState->truckPosition + glm::vec3(steeringRotation * glm::vec4(0.0f, 1.1f, -0.8f, 1.0f));

            programState->drivingCamera.Position = glm::mix(programState->drivingCamera.Position, targetPosition, 0.3f);
            programState->drivingCamera.Front = glm::normalize(programState->truckForward);
            programState->drivingCamera.Up = glm::vec3(0, 1, 0);
        }

        windshieldShader.use();
        windshieldShader.setMat4("projection", projection);
        windshieldShader.setMat4("view", view);
        windshieldShader.setMat4("model", windshieldModel);
        windshieldShader.setVec4("windshieldColor", glm::vec4(0.7f, 0.7f, 0.9f, 0.2f));

        glBindVertexArray(windshieldVAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);

        glDisable(GL_BLEND);

        // tone mapping vreme
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ourShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[1]);
        renderQuad();

        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        blurShader.use();
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.setBool("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        bloomFinalShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        bloomFinalShader.setBool("bloom", bloom);
        bloomFinalShader.setFloat("exposure", exposure);
        renderQuad();

        if (programState->ImGuiEnabled)
            DrawImGui(programState);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (programState -> isDrivingMode) {
        // constant
        const float truckMaxSpeed = 6.0f;
        const float truckAcceleration = 2.0f;
        const float truckSteerSpeed = 1.0f;

        // brm brm
        //std::cout<<"Speed " << programState -> currentTruckSpeed <<std::endl;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            programState -> currentTruckSpeed += truckAcceleration * deltaTime;
        }
        else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            if (programState -> currentTruckSpeed > 0) {
                programState -> currentTruckSpeed -= 3.0f * truckAcceleration * deltaTime; // kocnice redovno servisirane
            }
            else {
                programState -> currentTruckSpeed -= truckAcceleration * deltaTime;
            }
        }
        else if (programState -> currentTruckSpeed > 0) {
            programState -> currentTruckSpeed -= truckAcceleration * deltaTime; // uspori ako ga ne diramo
        }
        // ne sme brzo u rikverc to niko ne radi
        programState -> currentTruckSpeed = glm::clamp(programState -> currentTruckSpeed, -truckMaxSpeed/5, truckMaxSpeed);

        if (glm::abs(programState->currentTruckSpeed) > 0.1f) { // simpl fiks da se ne vrti u mestu
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                programState->currentTruckSteer += truckSteerSpeed * (programState -> currentTruckSpeed / truckMaxSpeed) * deltaTime;
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                programState->currentTruckSteer -= truckSteerSpeed * (programState -> currentTruckSpeed / truckMaxSpeed) * deltaTime;
            }

        }
        glm::vec3 truckMovement = programState -> currentTruckSpeed * programState -> truckForward * deltaTime;
        programState -> truckPosition += truckMovement;
        programState -> drivingCamera.Position += truckMovement;
        //programState -> drivingCamera.Front = programState -> truckForward;
    } else {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            programState->worldCamera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            programState->worldCamera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            programState->worldCamera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            programState->worldCamera.ProcessKeyboard(RIGHT, deltaTime);
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !bloomKeyPressed)
    {
        bloom = !bloom;
        bloomKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE)
    {
        bloomKeyPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        if (exposure > 0.0f)
            exposure -= 0.001f;
        else
            exposure = 0.0f;
    }
    else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        exposure += 0.001f;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled) {
        Camera& activeCamera = programState->isDrivingMode ? programState->drivingCamera : programState->worldCamera;
        activeCamera.ProcessMouseMovement(xoffset, yoffset);
    }
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    Camera& activeCamera = programState->isDrivingMode ? programState->drivingCamera : programState->worldCamera;
    activeCamera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    {
        static float f = 0.0f;
        ImGui::Begin("Hello window");
        ImGui::Text("Hello text");
        ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
        ImGui::ColorEdit3("Background color", (float *) &programState->clearColor);

        ImGui::DragFloat("leftHeadlight.constant", &programState->leftHeadlight.constant, 0.05, 0.0, 1.0);
        ImGui::DragFloat("leftHeadlight.linear", &programState->leftHeadlight.linear, 0.05, 0.0, 1.0);
        ImGui::DragFloat("leftHeadlight.quadratic", &programState->leftHeadlight.quadratic, 0.05, 0.0, 1.0);
        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->isDrivingMode ? programState->drivingCamera : programState->worldCamera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        programState -> isDrivingMode = !(programState -> isDrivingMode);
    }
}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

// loads a cubemap texture from 6 individual texture faces
// order:
// +X (right)
// -X (left)
// +Y (top)
// -Y (bottom)
// +Z (front)
// -Z (back)
// -------------------------------------------------------
unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
