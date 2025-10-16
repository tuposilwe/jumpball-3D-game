#include <iostream>
#include <vector>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ImGui includes
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Window dimensions
unsigned int SCR_WIDTH = 800;
unsigned int SCR_HEIGHT = 600;

// Player properties
glm::vec3 playerPos = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 playerTargetPos = playerPos;
float playerSpeed = 8.0f;
float playerRadius = 1.0f;
float playerRotation = 0.0f;
float playerRotationTarget = 0.0f;

// Smooth damping properties
float positionSmoothTime = 0.1f;
float rotationSmoothTime = 0.05f;
float cameraSmoothTime = 0.1f;

// Camera properties
glm::vec3 cameraPos = glm::vec3(0.0f, 3.0f, 8.0f);
glm::vec3 cameraTargetPos = cameraPos;
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 cameraForward = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
float cameraDistance = 6.0f;
float cameraTargetDistance = cameraDistance;
float cameraHeight = 3.0f;
float cameraTargetHeight = cameraHeight;
float cameraAngle = 0.0f;
float cameraTargetAngle = cameraAngle;

// Joystick properties
bool joystickPresent = false;
int joystickId = GLFW_JOYSTICK_1;
float joystickDeadzone = 0.2f;
float joystickSensitivity = 2.0f;

// Mouse look
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
float mouseSensitivity = 0.1f;
bool mouseCaptured = false;  // Start with mouse visible for ImGui

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Vertex shader source
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

// Fragment shader source
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * vec3(1.0);
    
    // Diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * vec3(1.0);
    
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
}
)";

// Smooth damping functions
glm::vec3 smoothDamp(glm::vec3 current, glm::vec3 target, glm::vec3& currentVelocity, float smoothTime, float deltaTime) {
    float omega = 2.0f / smoothTime;
    float x = omega * deltaTime;
    float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    glm::vec3 change = current - target;
    glm::vec3 temp = (currentVelocity + omega * change) * deltaTime;
    currentVelocity = (currentVelocity - omega * temp) * exp;
    glm::vec3 output = target + (change + temp) * exp;

    return output;
}

float smoothDamp(float current, float target, float& currentVelocity, float smoothTime, float deltaTime) {
    float omega = 2.0f / smoothTime;
    float x = omega * deltaTime;
    float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    float change = current - target;
    float temp = (currentVelocity + omega * change) * deltaTime;
    currentVelocity = (currentVelocity - omega * temp) * exp;
    float output = target + (change + temp) * exp;

    return output;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    SCR_WIDTH = width;
    SCR_HEIGHT = height;
    glViewport(0, 0, width, height);
}

// Update camera vectors based on current camera angle
void updateCameraVectors() {
    cameraForward = glm::vec3(sin(cameraAngle), 0.0f, cos(cameraAngle));
    cameraRight = glm::vec3(cos(cameraAngle), 0.0f, -sin(cameraAngle));
}

void updateCamera() {
    // Calculate camera position based on player position and camera angle
    float camX = sin(cameraAngle) * cameraDistance;
    float camZ = cos(cameraAngle) * cameraDistance;

    cameraTargetPos = playerPos + glm::vec3(camX, cameraHeight, camZ);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || !mouseCaptured) return;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
        return;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    // Horizontal mouse movement rotates camera around player
    cameraTargetAngle += xoffset * 0.01f;

    // Vertical mouse movement adjusts camera height
    cameraTargetHeight -= yoffset * 0.1f;
    if (cameraTargetHeight < 1.0f) cameraTargetHeight = 1.0f;
    if (cameraTargetHeight > 8.0f) cameraTargetHeight = 8.0f;

    updateCamera();
    updateCameraVectors();
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || !mouseCaptured) return;

    // Scroll wheel adjusts camera distance
    cameraTargetDistance -= yoffset * 0.5f;
    if (cameraTargetDistance < 3.0f) cameraTargetDistance = 3.0f;
    if (cameraTargetDistance > 15.0f) cameraTargetDistance = 15.0f;
    updateCamera();
}

// Joystick input processing
void processJoystickInput() {
    if (!joystickPresent) return;

    int axesCount;
    const float* axes = glfwGetJoystickAxes(joystickId, &axesCount);
    int buttonCount;
    const unsigned char* buttons = glfwGetJoystickButtons(joystickId, &buttonCount);

    // Left stick for movement (axes 0 and 1)
    float leftX = 0.0f, leftY = 0.0f;
    if (axesCount >= 2) {
        leftX = axes[0];
        leftY = axes[1];

        // Apply deadzone
        if (fabs(leftX) < joystickDeadzone) leftX = 0.0f;
        if (fabs(leftY) < joystickDeadzone) leftY = 0.0f;
    }

    // For 4-axis gamepads, use buttons for camera control or alternative axes
    float cameraX = 0.0f, cameraY = 0.0f;

    // Try using axes 2 and 3 for camera (common in some gamepads)
    if (axesCount >= 4) {
        cameraX = axes[2];
        cameraY = axes[3];

        // Apply deadzone
        if (fabs(cameraX) < joystickDeadzone) cameraX = 0.0f;
        if (fabs(cameraY) < joystickDeadzone) cameraY = 0.0f;
    }

    // Process movement with left stick
    if (fabs(leftX) > 0.0f || fabs(leftY) > 0.0f) {
        glm::vec3 movement = glm::vec3(0.0f);

        // Convert joystick input to world space movement relative to camera
        // Note: Invert Y axis for intuitive forward/backward movement
        movement -= cameraForward * (-leftY);  // Inverted Y
        movement += cameraRight * leftX;

        // Normalize if diagonal to maintain consistent speed
        if (glm::length(movement) > 0.0f) {
            movement = glm::normalize(movement);

            // Update player rotation to face movement direction
            playerRotationTarget = atan2(movement.x, movement.z);
        }

        // Apply movement with speed and delta time to target position
        playerTargetPos += movement * playerSpeed * joystickSensitivity * deltaTime;
    }

    // Process camera control - try multiple methods
    bool cameraMoved = false;

    // Method 1: Use axes 2 and 3 if available
    if (fabs(cameraX) > 0.0f || fabs(cameraY) > 0.0f) {
        cameraTargetAngle += cameraX * 0.05f;
        cameraTargetHeight -= cameraY * 0.5f;
        cameraMoved = true;
    }

    // Method 3: Use shoulder buttons for camera rotation
    if (buttonCount >= 6) {
        if (buttons[4] == GLFW_PRESS) {  // L1 or LB
            cameraTargetAngle -= 1.0f * deltaTime;
            cameraMoved = true;
        }
        if (buttons[5] == GLFW_PRESS) {  // R1 or RB
            cameraTargetAngle += 1.0f * deltaTime;
            cameraMoved = true;
        }

        // Use face buttons for camera height
        if (buttons[0] == GLFW_PRESS) {  // A or Cross
            cameraTargetHeight -= 2.0f * deltaTime;
            cameraMoved = true;
        }
        if (buttons[1] == GLFW_PRESS) {  // B or Circle
            cameraTargetHeight += 2.0f * deltaTime;
            cameraMoved = true;
        }
    }

    // Apply camera limits
    if (cameraTargetHeight < 1.0f) cameraTargetHeight = 1.0f;
    if (cameraTargetHeight > 8.0f) cameraTargetHeight = 8.0f;

    // Camera zoom with available buttons
    if (buttonCount >= 4) {
        if (buttons[2] == GLFW_PRESS) {  // X or Square
            cameraTargetDistance -= 3.0f * deltaTime;
        }
        if (buttons[3] == GLFW_PRESS) {  // Y or Triangle
            cameraTargetDistance += 3.0f * deltaTime;
        }

        if (cameraTargetDistance < 3.0f) cameraTargetDistance = 3.0f;
        if (cameraTargetDistance > 15.0f) cameraTargetDistance = 15.0f;
    }

    updateCamera();
}

void processInput(GLFWwindow* window) {
    ImGuiIO& io = ImGui::GetIO();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Toggle mouse capture with Tab key
    static bool tabPressed = false;
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !tabPressed) {
        mouseCaptured = !mouseCaptured;
        if (mouseCaptured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true; // Reset mouse position for smooth transition
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        tabPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_RELEASE) {
        tabPressed = false;
    }

    // Only process game input if ImGui isn't using keyboard AND mouse is captured
    if (!io.WantCaptureKeyboard && mouseCaptured) {
        // Reset player movement
        glm::vec3 movement = glm::vec3(0.0f);

        // Player movement relative to camera direction using pre-calculated vectors
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            movement -= cameraForward;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            movement += cameraForward;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            movement -= cameraRight;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            movement += cameraRight;

        // Normalize movement if diagonal to maintain consistent speed
        if (glm::length(movement) > 0.0f) {
            movement = glm::normalize(movement);

            // Update player rotation to face movement direction
            playerRotationTarget = atan2(movement.x, movement.z);

            // Apply movement with speed and delta time to target position
            playerTargetPos += movement * playerSpeed * deltaTime;
        }
    }

    // Simple ground collision for target position
    if (playerTargetPos.y < playerRadius) {
        playerTargetPos.y = playerRadius;
    }

    // Process joystick input (always processed)
    processJoystickInput();

    updateCamera();
}

// Check for joystick connection
void checkJoystickConnection() {
    joystickPresent = glfwJoystickPresent(joystickId);

    if (joystickPresent) {
        const char* name = glfwGetJoystickName(joystickId);
        std::cout << "Joystick connected: " << (name ? name : "Unknown") << std::endl;

        int axesCount;
        glfwGetJoystickAxes(joystickId, &axesCount);
        std::cout << "Axes count: " << axesCount << std::endl;

        int buttonCount;
        glfwGetJoystickButtons(joystickId, &buttonCount);
        std::cout << "Buttons count: " << buttonCount << std::endl;
    }
    else {
        std::cout << "No joystick detected. Using keyboard controls only." << std::endl;
    }
}

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Function to generate sphere vertices and indices
void generateSphere(float radius, int sectors, int stacks, std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    const float PI = 3.14159265359f;

    vertices.clear();
    indices.clear();

    float x, y, z, xy;
    float nx, ny, nz, lengthInv = 1.0f / radius;

    float sectorStep = 2 * PI / sectors;
    float stackStep = PI / stacks;
    float sectorAngle, stackAngle;

    // Generate vertices
    for (int i = 0; i <= stacks; ++i) {
        stackAngle = PI / 2 - i * stackStep;
        xy = radius * cosf(stackAngle);
        z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            sectorAngle = j * sectorStep;

            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);

            // Position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Normal
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
        }
    }

    // Generate indices
    unsigned int k1, k2;
    for (int i = 0; i < stacks; ++i) {
        k1 = i * (sectors + 1);
        k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}

// Function to generate a simple ground plane with grid
void generateGround(std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    const float size = 20.0f;
    const int divisions = 20;
    const float step = size / divisions;

    vertices.clear();
    indices.clear();

    // Generate grid vertices
    for (int i = 0; i <= divisions; ++i) {
        for (int j = 0; j <= divisions; ++j) {
            float x = -size / 2 + i * step;
            float z = -size / 2 + j * step;
            float y = 0.0f;

            // Position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Normal (always up)
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);
        }
    }

    // Generate indices
    for (int i = 0; i < divisions; ++i) {
        for (int j = 0; j < divisions; ++j) {
            unsigned int topLeft = i * (divisions + 1) + j;
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = (i + 1) * (divisions + 1) + j;
            unsigned int bottomRight = bottomLeft + 1;

            // First triangle
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            // Second triangle
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
}

// Function to create ImGui camera settings window
void createCameraSettingsWindow() {
    ImGui::Begin("Camera Settings");

    ImGui::Text("Camera Controls");
    ImGui::Separator();

    // Camera parameters
    ImGui::SliderFloat("Camera Distance", &cameraTargetDistance, 3.0f, 15.0f);
    ImGui::SliderFloat("Camera Height", &cameraTargetHeight, 1.0f, 8.0f);
    ImGui::SliderAngle("Camera Angle", &cameraTargetAngle, -180.0f, 180.0f);

    // Sensitivity settings
    ImGui::Separator();
    ImGui::Text("Sensitivity");
    ImGui::SliderFloat("Mouse Sensitivity", &mouseSensitivity, 0.01f, 1.0f);
    ImGui::SliderFloat("Joystick Sensitivity", &joystickSensitivity, 0.1f, 5.0f);

    // Smoothing settings
    ImGui::Separator();
    ImGui::Text("Smoothing");
    ImGui::SliderFloat("Position Smooth Time", &positionSmoothTime, 0.01f, 0.5f);
    ImGui::SliderFloat("Rotation Smooth Time", &rotationSmoothTime, 0.01f, 0.3f);
    ImGui::SliderFloat("Camera Smooth Time", &cameraSmoothTime, 0.01f, 0.5f);

    // Mouse control status
    ImGui::Separator();
    ImGui::Text("Mouse Control (TAB to toggle)");
    if (mouseCaptured) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Mouse: CAPTURED (Game Control)");
        ImGui::Text("Use mouse to look around");
        ImGui::Text("Press TAB to release mouse for UI");
    }
    else {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Mouse: RELEASED (UI Control)");
        ImGui::Text("Use mouse to interact with UI");
        ImGui::Text("Press TAB to capture mouse for game");
    }

    // Joystick settings
    ImGui::Separator();
    ImGui::Text("Joystick Settings");
    ImGui::SliderFloat("Joystick Deadzone", &joystickDeadzone, 0.0f, 0.5f);

    // Joystick status
    if (joystickPresent) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Joystick Connected");

        int axesCount;
        glfwGetJoystickAxes(joystickId, &axesCount);
        int buttonCount;
        glfwGetJoystickButtons(joystickId, &buttonCount);

        ImGui::Text("Axes: %d, Buttons: %d", axesCount, buttonCount);
    }
    else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No Joystick Detected");
    }

    // Reset buttons
    ImGui::Separator();
    if (ImGui::Button("Reset Camera")) {
        cameraTargetDistance = 6.0f;
        cameraTargetHeight = 3.0f;
        cameraTargetAngle = 0.0f;
        cameraAngle = 0.0f;
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset Smoothing")) {
        positionSmoothTime = 0.1f;
        rotationSmoothTime = 0.05f;
        cameraSmoothTime = 0.1f;
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset Player")) {
        playerTargetPos = glm::vec3(0.0f, 1.0f, 0.0f);
        playerPos = playerTargetPos;
    }

    // Display current values
    ImGui::Separator();
    ImGui::Text("Current Values");
    ImGui::Text("Player Pos: (%.2f, %.2f, %.2f)", playerPos.x, playerPos.y, playerPos.z);
    ImGui::Text("Camera Pos: (%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);
    ImGui::Text("Player Rotation: %.2f", playerRotation);

    ImGui::End();
}

int main() {
    std::cout << "Sphere Controller with Joystick Support and ImGui" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - WASD: Move the sphere" << std::endl;
    std::cout << "  - Mouse: Look around (TAB to capture/release mouse)" << std::endl;
    std::cout << "  - Scroll: Zoom in/out" << std::endl;
    std::cout << "  - Joystick: Left stick to move, Right stick to look, Triggers to zoom" << std::endl;
    std::cout << "  - TAB: Toggle mouse capture" << std::endl;
    std::cout << "  - ESC: Exit" << std::endl;

    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Sphere Controller with ImGui Camera Settings", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Start with mouse visible for ImGui
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Check for joystick connection
    checkJoystickConnection();

    // Create shader program
    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // Generate sphere geometry
    std::vector<float> sphereVertices;
    std::vector<unsigned int> sphereIndices;
    generateSphere(playerRadius, 36, 18, sphereVertices, sphereIndices);

    // Generate ground geometry
    std::vector<float> groundVertices;
    std::vector<unsigned int> groundIndices;
    generateGround(groundVertices, groundIndices);

    // Set up sphere VAO, VBO, EBO
    unsigned int sphereVAO, sphereVBO, sphereEBO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &sphereEBO);

    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(float), sphereVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIndices.size() * sizeof(unsigned int), sphereIndices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Set up ground VAO, VBO, EBO
    unsigned int groundVAO, groundVBO, groundEBO;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glGenBuffers(1, &groundEBO);

    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, groundVertices.size() * sizeof(float), groundVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, groundIndices.size() * sizeof(unsigned int), groundIndices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Light position
    glm::vec3 lightPos = glm::vec3(10.0f, 10.0f, 10.0f);

    // Initialize camera and camera vectors
    updateCamera();
    updateCameraVectors();

    // Smooth damping velocity variables
    glm::vec3 playerPosVelocity = glm::vec3(0.0f);
    float playerRotationVelocity = 0.0f;
    glm::vec3 cameraPosVelocity = glm::vec3(0.0f);
    float cameraDistanceVelocity = 0.0f;
    float cameraHeightVelocity = 0.0f;
    float cameraAngleVelocity = 0.0f;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Periodically check for joystick connection
        static float lastJoystickCheck = 0.0f;
        if (currentFrame - lastJoystickCheck > 2.0f) { // Check every 2 seconds
            checkJoystickConnection();
            lastJoystickCheck = currentFrame;
        }

        processInput(window);

        // Apply smooth damping to player position
        playerPos = smoothDamp(playerPos, playerTargetPos, playerPosVelocity, positionSmoothTime, deltaTime);

        // Apply smooth damping to player rotation
        playerRotation = smoothDamp(playerRotation, playerRotationTarget, playerRotationVelocity, rotationSmoothTime, deltaTime);

        // Apply smooth damping to camera parameters
        cameraDistance = smoothDamp(cameraDistance, cameraTargetDistance, cameraDistanceVelocity, cameraSmoothTime, deltaTime);
        cameraHeight = smoothDamp(cameraHeight, cameraTargetHeight, cameraHeightVelocity, cameraSmoothTime, deltaTime);
        cameraAngle = smoothDamp(cameraAngle, cameraTargetAngle, cameraAngleVelocity, cameraSmoothTime, deltaTime);

        // Apply smooth damping to camera position
        cameraPos = smoothDamp(cameraPos, cameraTargetPos, cameraPosVelocity, cameraSmoothTime, deltaTime);

        // Update camera vectors after smooth damping
        updateCameraVectors();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create camera settings window
        createCameraSettingsWindow();

        // Rendering
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // View and projection matrices
        glm::mat4 view = glm::lookAt(cameraPos, playerPos, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        // Set shader uniforms
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));

        // Render ground
        glm::mat4 groundModel = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(groundModel));
        glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(glm::vec3(0.3f, 0.5f, 0.3f)));
        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLES, groundIndices.size(), GL_UNSIGNED_INT, 0);

        // Render player sphere
        glm::mat4 sphereModel = glm::mat4(1.0f);
        sphereModel = glm::translate(sphereModel, playerPos);
        sphereModel = glm::rotate(sphereModel, playerRotation, glm::vec3(0.0f, 1.0f, 0.0f));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(sphereModel));
        glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(glm::vec3(0.8f, 0.2f, 0.2f)));
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteVertexArrays(1, &groundVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &groundVBO);
    glDeleteBuffers(1, &sphereEBO);
    glDeleteBuffers(1, &groundEBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();

    std::cout << "Application terminated successfully!" << std::endl;
    return 0;
}