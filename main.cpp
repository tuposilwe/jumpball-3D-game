#include <iostream>
#include <vector>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Window dimensions
unsigned int SCR_WIDTH = 800;
unsigned int SCR_HEIGHT = 600;

// Player properties
glm::vec3 playerPos = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 playerVelocity = glm::vec3(0.0f);
glm::vec3 playerForward = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 playerRight = glm::vec3(1.0f, 0.0f, 0.0f);
float playerSpeed = 8.0f;
float playerRadius = 1.0f;
float playerRotation = 0.0f;

// Smooth damping properties
glm::vec3 playerTargetPos = playerPos;
glm::vec3 playerTargetVelocity = glm::vec3(0.0f);
float playerRotationTarget = 0.0f;
float positionSmoothTime = 0.1f;
float rotationSmoothTime = 0.05f;
float cameraSmoothTime = 0.1f;

// Camera properties
glm::vec3 cameraPos = glm::vec3(0.0f, 3.0f, 8.0f);
glm::vec3 cameraTargetPos = cameraPos;
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraDistance = 6.0f;
float cameraTargetDistance = cameraDistance;
float cameraHeight = 3.0f;
float cameraTargetHeight = cameraHeight;
float cameraAngle = 0.0f;
float cameraTargetAngle = cameraAngle;

// Mouse look
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
float mouseSensitivity = 0.1f;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Simple vertex shader
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

// Simple fragment shader
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

// Smooth damping function
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

void updateCamera() {
    // Calculate camera position based on player position and camera angle
    float camX = sin(cameraAngle) * cameraDistance;
    float camZ = cos(cameraAngle) * cameraDistance;

    cameraTargetPos = playerPos + glm::vec3(camX, cameraHeight, camZ);
    cameraFront = glm::normalize(playerPos - cameraTargetPos);
}

void updatePlayerVectors() {
    // Update player forward and right vectors based on rotation
    playerForward = glm::normalize(glm::vec3(
        sin(playerRotation),
        0.0f,
        -cos(playerRotation)
    ));
    playerRight = glm::normalize(glm::cross(playerForward, glm::vec3(0.0f, 1.0f, 0.0f)));
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
        return;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
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
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    // Scroll wheel adjusts camera distance
    cameraTargetDistance -= yoffset * 0.5f;
    if (cameraTargetDistance < 3.0f) cameraTargetDistance = 3.0f;
    if (cameraTargetDistance > 15.0f) cameraTargetDistance = 15.0f;
    updateCamera();
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Reset player movement
    glm::vec3 movement = glm::vec3(0.0f);

    // Player movement relative to camera direction
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        movement += glm::vec3(sin(cameraAngle), 0.0f, cos(cameraAngle));
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        movement -= glm::vec3(sin(cameraAngle), 0.0f, cos(cameraAngle));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        movement -= glm::vec3(cos(cameraAngle), 0.0f, -sin(cameraAngle));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        movement += glm::vec3(cos(cameraAngle), 0.0f, -sin(cameraAngle));

    // Normalize movement if diagonal to maintain consistent speed
    if (glm::length(movement) > 0.0f) {
        movement = glm::normalize(movement);

        // Update player rotation target to face movement direction
        playerRotationTarget = atan2(movement.x, movement.z);
    }

    // Apply movement with speed and delta time to target position
    playerTargetVelocity = movement * playerSpeed;
    playerTargetPos += playerTargetVelocity * deltaTime;

    // Simple ground collision for target position
    if (playerTargetPos.y < playerRadius) {
        playerTargetPos.y = playerRadius;
    }

    updateCamera();
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
    float s, t;

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

// Function to generate a simple arrow for direction indicator
void generateArrow(std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    vertices = {
        // Arrow base (small pyramid)
        // Positions          // Normals
        0.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
        0.2f, 0.0f, 0.4f,    0.0f, 1.0f, 0.0f,
        -0.2f, 0.0f, 0.4f,   0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,    0.0f, 1.0f, 0.0f
    };

    indices = {
        0, 1, 2,
        1, 3, 2,
        0, 3, 1,
        0, 2, 3
    };
}

int main() {
    std::cout << "Improved Sphere Controller with Smooth Damping" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - WASD: Move the sphere" << std::endl;
    std::cout << "  - Mouse: Look around" << std::endl;
    std::cout << "  - Scroll: Zoom in/out" << std::endl;
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Sphere Controller with Smooth Damping", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Capture the mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

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

    // Generate arrow geometry for direction indicator
    std::vector<float> arrowVertices;
    std::vector<unsigned int> arrowIndices;
    generateArrow(arrowVertices, arrowIndices);

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

    // Set up arrow VAO, VBO, EBO
    unsigned int arrowVAO, arrowVBO, arrowEBO;
    glGenVertexArrays(1, &arrowVAO);
    glGenBuffers(1, &arrowVBO);
    glGenBuffers(1, &arrowEBO);

    glBindVertexArray(arrowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, arrowVBO);
    glBufferData(GL_ARRAY_BUFFER, arrowVertices.size() * sizeof(float), arrowVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, arrowEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, arrowIndices.size() * sizeof(unsigned int), arrowIndices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Light position
    glm::vec3 lightPos = glm::vec3(10.0f, 10.0f, 10.0f);

    // Initialize player vectors
    updatePlayerVectors();
    updateCamera();

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

        // Update player vectors after smooth rotation
        updatePlayerVectors();

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

        // Render direction arrow
        glm::mat4 arrowModel = glm::mat4(1.0f);
        arrowModel = glm::translate(arrowModel, playerPos + glm::vec3(0.0f, 1.5f, 0.0f));
        arrowModel = glm::rotate(arrowModel, playerRotation, glm::vec3(0.0f, 1.0f, 0.0f));
        arrowModel = glm::scale(arrowModel, glm::vec3(0.5f));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(arrowModel));
        glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.0f)));
        glBindVertexArray(arrowVAO);
        glDrawElements(GL_TRIANGLES, arrowIndices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteVertexArrays(1, &groundVAO);
    glDeleteVertexArrays(1, &arrowVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &groundVBO);
    glDeleteBuffers(1, &arrowVBO);
    glDeleteBuffers(1, &sphereEBO);
    glDeleteBuffers(1, &groundEBO);
    glDeleteBuffers(1, &arrowEBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();

    std::cout << "Application terminated successfully!" << std::endl;
    return 0;
}