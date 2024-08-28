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

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

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

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0.4);
    bool ImGuiEnabled = true;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    glm::vec3 truckPosition = glm::vec3(0.0f);
    Spotlight leftHeadlight;
    Spotlight rightHeadlight;
    ProgramState()
            : camera(glm::vec3(40.0f, 40.0f, 20.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

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

    // load models
    // -----------
    Model truck("resources/objects/truck/truck.obj");
    Model wall("resources/objects/wall/10061_Wall_SG_V2_Iterations-2.obj");
    truck.SetShaderTextureNamePrefix("material.");
    wall.SetShaderTextureNamePrefix("material.");


    Spotlight leftHeadlight, rightHeadlight;
    leftHeadlight.position = glm::vec3(0);
    leftHeadlight.direction = glm::vec3(0.0f, 0.0f, -1.0f);
    leftHeadlight.ambient = glm::vec3(0.5f, 0.5f, 0.5f);
    leftHeadlight.diffuse = glm::vec3(8.0f, 8.0f, 8.0f);
    leftHeadlight.specular = glm::vec3(10, 10, 10);

    leftHeadlight.constant = 1.0f;
    leftHeadlight.linear = 0.09f;
    leftHeadlight.quadratic = 0.032f;

    leftHeadlight.cutOff = glm::cos(glm::radians(30.0f));
    leftHeadlight.outerCutOff = glm::cos(glm::radians(60.0f));


    rightHeadlight.position = glm::vec3(0);
    rightHeadlight.direction = glm::vec3(0.0f, 0.0f, -1.0f);
    rightHeadlight.ambient = glm::vec3(0.5f, 0.5f, 0.5f);
    rightHeadlight.diffuse = glm::vec3(8.0f, 8.0f, 8.0f);
    rightHeadlight.specular = glm::vec3(10, 10, 10);

    rightHeadlight.constant = 1.0f;
    rightHeadlight.linear = 0.09f;
    rightHeadlight.quadratic = 0.032f;

    rightHeadlight.cutOff = glm::cos(glm::radians(30.0f));
    rightHeadlight.outerCutOff = glm::cos(glm::radians(60.0f));





    // draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

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

        // don't forget to enable shader before setting uniforms
        ourShader.use();

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        // truck
        glm::mat4 truckModel = glm::mat4(1.0f);

        // model je blesav pa ga rotiramo da bude lepo orijentisan
        const float truckRotOffsetX = -M_PI * 0.5f;
        const float truckRotOffsetZ = M_PI * 0.5f;

        truckModel = glm::translate(truckModel, glm::vec3(0.0f, 0.0f, 0.0f));
        truckModel = glm::scale(truckModel, glm::vec3(1.0f));
        truckModel = glm::rotate(truckModel, truckRotOffsetX, glm::vec3(1, 0, 0));
        truckModel = glm::rotate(truckModel, truckRotOffsetZ, glm::vec3(0, 0, 1));
        ourShader.setMat4("model", truckModel);
        truck.Draw(ourShader);

        // lights
        glm::mat4 headlightModel = glm::mat4(1.0f);

        // ubijemo originalne kamionove rotacije koje cemo ispod primeniti na svetla
        headlightModel = glm::rotate(headlightModel, -truckRotOffsetZ, glm::vec3(0, 0, 1)); // Undo Z-axis rotation
        headlightModel = glm::rotate(headlightModel, -truckRotOffsetX, glm::vec3(1, 0, 0));  // Undo X-axis rotation

        // al takodje rotiramo malo dole jer su ovo farovi pa kao gledaju u put
        headlightModel = glm::rotate(headlightModel, -0.3f, glm::vec3(1, 0, 0)); // Tilt down around X-axis



        leftHeadlight.position = glm::vec3(truckModel * headlightModel * glm::vec4(-4.0f, 7.0f, -16.0f, 1.0f));
        rightHeadlight.position = glm::vec3(truckModel * headlightModel * glm::vec4(4.0f, 7.0f, -16.0f, 1.0f));

        //leftHeadlight.position = programState -> camera.Position;
        //leftHeadlight.direction = programState -> camera.Front;

        std::cout << leftHeadlight.position.x << " " <<leftHeadlight.position.y << " " << leftHeadlight.position.z << std::endl;;
        std::cout << rightHeadlight.position.x << " " <<rightHeadlight.position.y << " " << rightHeadlight.position.z << std::endl;;

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

        ourShader.setVec3("viewPosition", programState->camera.Position);
        ourShader.setFloat("material.shininess", 32.0f);

        glEnable(GL_CULL_FACE);

        // wall
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, -25.0f));
        model = glm::scale(model, glm::vec3(0.1));
        model = glm::rotate(model, -3.14f*0.5f, glm::vec3(1, 0, 0));
        //model = glm::rotate(model, 3.14f*0.5f, glm::vec3(0, 0, 1));
        ourShader.setMat4("model", model);
        wall.Draw(ourShader);

        if (programState->ImGuiEnabled)
            DrawImGui(programState);



        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);
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

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
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
        const Camera& c = programState->camera;
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
}
