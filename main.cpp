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

// World boundaries
const float GROUND_SIZE = 20.0f;
const float WORLD_BOUNDARY = GROUND_SIZE / 2.0f + 1.0f; // Slightly smaller than ground for visual margin

// Player properties
glm::vec3 playerPos = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 playerTargetPos = playerPos;
float playerSpeed = 8.0f;
float playerRadius = 1.0f;
float playerRotation = 0.0f;
float playerRotationTarget = 0.0f;
bool playerAlive = true;
float playerRespawnTimer = 0.0f;
const float PLAYER_RESPAWN_TIME = 3.0f;

// Egg properties
struct Egg {
    glm::vec3 position;
    bool active;
    float radius;
    glm::vec3 color;
    float spawnTime;
    float lifeTimer;
    float scale;
    float pulseFactor;
    bool spawning;
    bool despawning;
    bool isPoison; // New: distinguishes between collectible and poison eggs
};

std::vector<Egg> eggs;
float eggSpawnTimer = 0.0f;
const float EGG_SPAWN_INTERVAL = 4.0f;
const float EGG_LIFESPAN = 4.0f;
const float EGG_RADIUS = 0.5f;
const int MAX_EGGS = 10;

// Poison egg properties
float poisonEggSpawnTimer = 0.0f;
const float POISON_EGG_SPAWN_INTERVAL = 6.0f;
const float POISON_EGG_LIFESPAN = 3.0f;
const float POISON_EGG_RADIUS = 0.6f;
const int MAX_POISON_EGGS = 5;

// Animation properties
const float SPAWN_ANIMATION_DURATION = 1.0f;
const float DESPAWN_ANIMATION_DURATION = 1.0f;
const float PULSE_SPEED = 3.0f;
const float POISON_PULSE_SPEED = 5.0f; // Faster pulse for poison eggs

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

// Camera settings for ImGui
bool showSettings = true;

// Game state
int score = 0;
int lives = 3;
bool gameOver = false;

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
float scrollSensitivity = 0.5f;

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

// Boundary checking function
void enforceWorldBoundaries(glm::vec3& position) {
    float boundary = WORLD_BOUNDARY - playerRadius;
    // X boundary
    position.x = glm::clamp(position.x, -boundary, boundary);

    // Z boundary 
    position.z = glm::clamp(position.z, -boundary, boundary);

    // Y boundary (ground collision)
    position.y = glm::max(position.y, playerRadius);
}

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

// Generate random position for eggs
glm::vec3 generateRandomEggPosition() {
    float boundary = WORLD_BOUNDARY - EGG_RADIUS - 1.0f; // Keep eggs away from edges
    float x = ((float)rand() / RAND_MAX) * 2.0f * boundary - boundary;
    float z = ((float)rand() / RAND_MAX) * 2.0f * boundary - boundary;
    return glm::vec3(x, EGG_RADIUS, z);
}

// Generate random color for eggs
glm::vec3 generateRandomEggColor() {
    return glm::vec3(
        (float)rand() / RAND_MAX * 0.5f + 0.5f, // R: 0.5-1.0
        (float)rand() / RAND_MAX * 0.5f + 0.5f, // G: 0.5-1.0
        (float)rand() / RAND_MAX * 0.5f + 0.5f  // B: 0.5-1.0
    );
}

// Spawn a new egg
void spawnEgg() {
    if (eggs.size() < MAX_EGGS) {
        Egg newEgg;
        newEgg.position = generateRandomEggPosition();
        newEgg.active = true;
        newEgg.radius = EGG_RADIUS;
        newEgg.color = generateRandomEggColor();
        newEgg.spawnTime = glfwGetTime();
        newEgg.lifeTimer = EGG_LIFESPAN;
        newEgg.scale = 0.0f; // Start at scale 0 for spawn animation
        newEgg.pulseFactor = 0.0f;
        newEgg.spawning = true;
        newEgg.despawning = false;
        newEgg.isPoison = false;
        eggs.push_back(newEgg);
        std::cout << "Egg spawned at (" << newEgg.position.x << ", " << newEgg.position.z << ")" << std::endl;
    }
}

// Spawn a poison egg
void spawnPoisonEgg() {
    int poisonEggCount = 0;
    for (const auto& egg : eggs) {
        if (egg.isPoison && egg.active) {
            poisonEggCount++;
        }
    }

    if (poisonEggCount < MAX_POISON_EGGS) {
        Egg poisonEgg;
        poisonEgg.position = generateRandomEggPosition();
        poisonEgg.active = true;
        poisonEgg.radius = POISON_EGG_RADIUS;
        poisonEgg.color = glm::vec3(0.6f, 0.2f, 0.8f);
        poisonEgg.spawnTime = glfwGetTime();
        poisonEgg.lifeTimer = POISON_EGG_LIFESPAN;
        poisonEgg.scale = 0.0f;
        poisonEgg.pulseFactor = 0.0f;
        poisonEgg.spawning = true;
        poisonEgg.despawning = false;
        poisonEgg.isPoison = true;
        eggs.push_back(poisonEgg);
        std::cout << "POISON EGG spawned at (" << poisonEgg.position.x << ", " << poisonEgg.position.z << ")" << std::endl;
    }
}

// Player death function
void killPlayer() {
    if (playerAlive) {
        playerAlive = false;
        lives--;
        playerRespawnTimer = PLAYER_RESPAWN_TIME;

        if (lives <= 0) {
            gameOver = true;
            std::cout << "GAME OVER! Final Score: " << score << std::endl;
        }
        else {
            std::cout << "Player died! Lives remaining: " << lives << std::endl;
        }
    }
}

// Respawn player
void respawnPlayer() {
    playerPos = glm::vec3(0.0f, 1.0f, 0.0f);
    playerTargetPos = playerPos;
    playerAlive = true;
    std::cout << "Player respawned!" << std::endl;
}

// Update egg animations and lifecycle
void updateEggs() {
    // Update spawn timers
    eggSpawnTimer += deltaTime;
    poisonEggSpawnTimer += deltaTime;

    // Spawn new egg if timer reaches interval
    if (eggSpawnTimer >= EGG_SPAWN_INTERVAL) {
        spawnEgg();
        eggSpawnTimer = 0.0f;
    }

    // Spawn poison egg if timer reaches interval
    if (poisonEggSpawnTimer >= POISON_EGG_SPAWN_INTERVAL) {
        spawnPoisonEgg();
        poisonEggSpawnTimer = 0.0f;
    }

    // Update all eggs
    for (auto& egg : eggs) {
        if (egg.active) {
            // Update life timer
            egg.lifeTimer -= deltaTime;

            // Update pulse animation (continuous pulsing)
            float pulseSpeed = egg.isPoison ? POISON_PULSE_SPEED : PULSE_SPEED;
            egg.pulseFactor = sin(glfwGetTime() * pulseSpeed) * 0.1f + 1.0f; // Pulse between 0.9 and 1.1

            // Handle spawn animation
            if (egg.spawning) {
                float spawnProgress = 1.0f - (egg.lifeTimer / (egg.isPoison ? POISON_EGG_LIFESPAN : EGG_LIFESPAN));
                float spawnDuration = egg.isPoison ? SPAWN_ANIMATION_DURATION * 0.7f : SPAWN_ANIMATION_DURATION;
                if (spawnProgress < spawnDuration / (egg.isPoison ? POISON_EGG_LIFESPAN : EGG_LIFESPAN)) {
                    // Scale up during spawn animation
                    egg.scale = spawnProgress * ((egg.isPoison ? POISON_EGG_LIFESPAN : EGG_LIFESPAN) / spawnDuration);
                }
                else {
                    // Spawn animation complete
                    egg.scale = 1.0f;
                    egg.spawning = false;
                }
            }

            // Handle despawn animation
            float despawnDuration = egg.isPoison ? DESPAWN_ANIMATION_DURATION * 0.7f : DESPAWN_ANIMATION_DURATION;
            if (egg.lifeTimer <= despawnDuration && !egg.despawning) {
                egg.despawning = true;
            }

            if (egg.despawning) {
                // Scale down during despawn animation
                float despawnProgress = egg.lifeTimer / despawnDuration;
                egg.scale = despawnProgress;
            }

            // Deactivate egg if lifespan is over
            if (egg.lifeTimer <= 0.0f) {
                egg.active = false;
                if (egg.isPoison) {
                    std::cout << "Poison egg despawned!" << std::endl;
                }
                else {
                    std::cout << "Egg despawned!" << std::endl;
                }
            }

            // Check for collisions with player (only if player is alive)
            if (playerAlive) {
                float distance = glm::distance(playerPos, egg.position);
                float collisionDistance = playerRadius + egg.radius * egg.scale;

                if (distance < collisionDistance) {
                    if (egg.isPoison) {
                        // Poison egg - kill player
                        killPlayer();
                        egg.active = false;
                        std::cout << "Player hit poison egg! Lives: " << lives << std::endl;
                    }
                    else {
                        // Regular egg - collect and score
                        egg.active = false;
                        score += 10;
                        std::cout << "Egg collected! Score: " << score << std::endl;
                    }
                }
            }
        }
    }

    // Remove inactive eggs
    eggs.erase(std::remove_if(eggs.begin(), eggs.end(),
        [](const Egg& egg) { return !egg.active; }), eggs.end());
}

// Update player respawn
void updatePlayer() {
    if (!playerAlive && !gameOver) {
        playerRespawnTimer -= deltaTime;
        if (playerRespawnTimer <= 0.0f) {
            respawnPlayer();
        }
    }
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
    // Check if ImGui wants to capture the mouse
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

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
    // Check if ImGui wants to capture the mouse
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    // Scroll wheel adjusts camera distance
    cameraTargetDistance -= yoffset * scrollSensitivity;
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

    // Process movement with left stick (only if player is alive)
    if (playerAlive && (fabs(leftX) > 0.0f || fabs(leftY) > 0.0f)) {
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

        // Enforce boundaries on target position
        enforceWorldBoundaries(playerTargetPos);
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
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    static bool keyPressed = false;
    // Toggle ImGui settings window with F1
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {

        if (!keyPressed) {
            showSettings = !showSettings;
            keyPressed = true;
        }
    }
    else {
        keyPressed = false;
    }

    // Reset game with R key
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && gameOver) {
        // Reset game state
        score = 0;
        lives = 3;
        playerAlive = true;
        gameOver = false;
        eggs.clear();
        playerPos = glm::vec3(0.0f, 1.0f, 0.0f);
        playerTargetPos = playerPos;
        std::cout << "Game reset! Starting over..." << std::endl;
    }

    // Reset player movement (only if player is alive)
    glm::vec3 movement = glm::vec3(0.0f);

    if (playerAlive) {
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

            // Enforce boundaries on target position
            enforceWorldBoundaries(playerTargetPos);
        }
    }

    // Process joystick input
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
    const float size = GROUND_SIZE;
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

void showCameraSettingsWindow() {
    if (!showSettings) return;

    ImGui::Begin("Game Settings", &showSettings, ImGuiWindowFlags_AlwaysAutoResize);

    // Game status
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "SCORE: %d", score);
    ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "LIVES: %d", lives);

    if (!playerAlive && !gameOver) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "RESPAWNING IN: %.1f", playerRespawnTimer);
    }

    if (gameOver) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "GAME OVER! Press R to restart");
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Camera Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Camera parameters
        ImGui::SliderFloat("Camera Distance", &cameraTargetDistance, 3.0f, 15.0f);
        ImGui::SliderFloat("Camera Height", &cameraTargetHeight, 1.0f, 8.0f);
        ImGui::SliderAngle("Camera Angle", &cameraTargetAngle, -180.0f, 180.0f);
    }

    if (ImGui::CollapsingHeader("Camera Behavior", ImGuiTreeNodeFlags_DefaultOpen)) {

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
    }

    if (ImGui::CollapsingHeader("Player Info")) {
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", playerPos.x, playerPos.y, playerPos.z);
        ImGui::Text("Rotation: %.2f rad", playerRotation);
        ImGui::Text("Camera Pos: (%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);

        // Boundary info
        ImGui::Separator();
        ImGui::Text("World Boundaries: ±%.1f", WORLD_BOUNDARY);
        bool atBoundaryX = (playerPos.x >= WORLD_BOUNDARY - playerRadius - 0.1f) || (playerPos.x <= -WORLD_BOUNDARY + playerRadius + 0.1f);
        bool atBoundaryZ = (playerPos.z >= WORLD_BOUNDARY - playerRadius - 0.1f) || (playerPos.z <= -WORLD_BOUNDARY + playerRadius + 0.1f);

        if (atBoundaryX || atBoundaryZ) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "At World Boundary");
        }
        else {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Within Boundaries");
        }
    }

    if (ImGui::CollapsingHeader("Egg System")) {
        int regularEggCount = 0;
        int poisonEggCount = 0;
        for (const auto& egg : eggs) {
            if (egg.isPoison) poisonEggCount++;
            else regularEggCount++;
        }

        ImGui::Text("Regular Eggs: %d/%d", regularEggCount, MAX_EGGS);
        ImGui::Text("Poison Eggs: %d/%d", poisonEggCount, MAX_POISON_EGGS);
        ImGui::Text("Next Regular Egg: %.1f seconds", EGG_SPAWN_INTERVAL - eggSpawnTimer);
        ImGui::Text("Next Poison Egg: %.1f seconds", POISON_EGG_SPAWN_INTERVAL - poisonEggSpawnTimer);

        if (ImGui::Button("Spawn Regular Egg")) {
            spawnEgg();
        }

        ImGui::SameLine();
        if (ImGui::Button("Spawn Poison Egg")) {
            spawnPoisonEgg();
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear All Eggs")) {
            eggs.clear();
        }
    }

    if (ImGui::CollapsingHeader("Joystick settings")) {
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
    }

    if (ImGui::CollapsingHeader("Help")) {
        ImGui::Text("WASD: Move player");
        ImGui::Text("Mouse: Look around");
        ImGui::Text("Scroll: Zoom in/out");
        ImGui::Text("F1: Toggle this window");
        ImGui::Text("ESC: Exit");
        ImGui::Text("R: Restart game (when game over)");
        ImGui::Text("World Boundaries: Player cannot leave the ground area");
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Green Eggs: +10 points");
        ImGui::TextColored(ImVec4(1, 0, 1, 1), "Purple/Green Poison Eggs: -1 life");
        ImGui::Text("Collect good eggs, avoid poison eggs!");
    }
    ImGui::End();
}

int main() {
    std::cout << "Sphere Controller with Joystick Support and ImGui" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - WASD: Move the sphere" << std::endl;
    std::cout << "  - Mouse: Look around" << std::endl;
    std::cout << "  - Scroll: Zoom in/out" << std::endl;
    std::cout << "  - F1: Toggle camera settings" << std::endl;
    std::cout << "  - Joystick: Left stick to move, Right stick to look, Triggers to zoom" << std::endl;
    std::cout << "  - ESC: Exit" << std::endl;
    std::cout << "  - R: Restart game when game over" << std::endl;
    std::cout << "World Boundaries: Player is confined to a " << GROUND_SIZE << "x" << GROUND_SIZE << " area" << std::endl;
    std::cout << "Egg System:" << std::endl;
    std::cout << "  - Regular eggs (various colors): +10 points, spawn every 15 seconds" << std::endl;
    std::cout << "  - Poison eggs (purple/green): -1 life, spawn every 10 seconds" << std::endl;
    std::cout << "  - You have 3 lives. Game over when all lives are lost." << std::endl;

    // Seed random number generator
    srand(static_cast<unsigned int>(time(nullptr)));

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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Egg Collector - Avoid the Poison!", nullptr, nullptr);
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

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Check for joystick connection
    checkJoystickConnection();

    // Create shader program
    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // Generate sphere geometry for player
    std::vector<float> sphereVertices;
    std::vector<unsigned int> sphereIndices;
    generateSphere(playerRadius, 36, 18, sphereVertices, sphereIndices);

    // Generate sphere geometry for eggs (smaller spheres)
    std::vector<float> eggVertices;
    std::vector<unsigned int> eggIndices;
    generateSphere(EGG_RADIUS, 24, 12, eggVertices, eggIndices);

    // Generate sphere geometry for poison eggs (slightly larger)
    std::vector<float> poisonEggVertices;
    std::vector<unsigned int> poisonEggIndices;
    generateSphere(POISON_EGG_RADIUS, 24, 12, poisonEggVertices, poisonEggIndices);

    // Generate ground geometry
    std::vector<float> groundVertices;
    std::vector<unsigned int> groundIndices;
    generateGround(groundVertices, groundIndices);

    // Set up player sphere VAO, VBO, EBO
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

    // Set up egg sphere VAO, VBO, EBO
    unsigned int eggVAO, eggVBO, eggEBO;
    glGenVertexArrays(1, &eggVAO);
    glGenBuffers(1, &eggVBO);
    glGenBuffers(1, &eggEBO);

    glBindVertexArray(eggVAO);
    glBindBuffer(GL_ARRAY_BUFFER, eggVBO);
    glBufferData(GL_ARRAY_BUFFER, eggVertices.size() * sizeof(float), eggVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eggEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, eggIndices.size() * sizeof(unsigned int), eggIndices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Set up poison egg sphere VAO, VBO, EBO
    unsigned int poisonEggVAO, poisonEggVBO, poisonEggEBO;
    glGenVertexArrays(1, &poisonEggVAO);
    glGenBuffers(1, &poisonEggVBO);
    glGenBuffers(1, &poisonEggEBO);

    glBindVertexArray(poisonEggVAO);
    glBindBuffer(GL_ARRAY_BUFFER, poisonEggVBO);
    glBufferData(GL_ARRAY_BUFFER, poisonEggVertices.size() * sizeof(float), poisonEggVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, poisonEggEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, poisonEggIndices.size() * sizeof(unsigned int), poisonEggIndices.data(), GL_STATIC_DRAW);
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

        // Update egg system
        updateEggs();

        // Update player respawn
        updatePlayer();

        // Apply smooth damping to player position (only if alive)
        if (playerAlive) {
            playerPos = smoothDamp(playerPos, playerTargetPos, playerPosVelocity, positionSmoothTime, deltaTime);
            playerRotation = smoothDamp(playerRotation, playerRotationTarget, playerRotationVelocity, rotationSmoothTime, deltaTime);
        }

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

        // Show camera settings window
        showCameraSettingsWindow();

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

        // Render player sphere (only if alive)
        if (playerAlive) {
            glm::mat4 sphereModel = glm::mat4(1.0f);
            sphereModel = glm::translate(sphereModel, playerPos);
            sphereModel = glm::rotate(sphereModel, playerRotation, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec3 playerColor = glm::vec3(0.8f, 0.2f, 0.2f);
       
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(sphereModel));
            glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(playerColor));
            glBindVertexArray(sphereVAO);
            glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
        }

        // Render eggs with animations
        for (const auto& egg : eggs) {
            if (egg.active) {
                glm::mat4 eggModel = glm::mat4(1.0f);
                eggModel = glm::translate(eggModel, egg.position);

                // Apply scale animation
                float finalScale = egg.scale * egg.pulseFactor;
                eggModel = glm::scale(eggModel, glm::vec3(finalScale));

                glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(eggModel));

                // Choose the appropriate VAO based on egg type
                if (egg.isPoison) {
                    glBindVertexArray(poisonEggVAO);
                    // Make poison eggs more vibrant
                    glm::vec3 finalColor = egg.color * 1.2f;
                    glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(finalColor));
                }
                else {
                    glBindVertexArray(eggVAO);
                    glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(egg.color));
                }

                glDrawElements(GL_TRIANGLES, eggIndices.size(), GL_UNSIGNED_INT, 0);
            }
        }

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
    glDeleteVertexArrays(1, &eggVAO);
    glDeleteVertexArrays(1, &poisonEggVAO);
    glDeleteVertexArrays(1, &groundVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &eggVBO);
    glDeleteBuffers(1, &poisonEggVBO);
    glDeleteBuffers(1, &groundVBO);
    glDeleteBuffers(1, &sphereEBO);
    glDeleteBuffers(1, &eggEBO);
    glDeleteBuffers(1, &poisonEggEBO);
    glDeleteBuffers(1, &groundEBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();

    std::cout << "Application terminated successfully! Final Score: " << score << std::endl;
    return 0;
}