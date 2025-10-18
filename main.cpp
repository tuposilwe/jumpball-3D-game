#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ImGui includes
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// FreeType includes
#include <ft2build.h>
#include FT_FREETYPE_H

// Window dimensions
unsigned int SCR_WIDTH = 800;
unsigned int SCR_HEIGHT = 600;

// Game states
enum GameState {
    GAME_START,
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_OVER
};

GameState currentGameState = GAME_START;

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
bool showSettings = false;

// Game state
int score = 0;
int lives = 3;
bool gameRunning = true;

// Fruit Ninja style miss system
int missedEggs = 0;
const int MAX_MISSES = 3;
std::vector<glm::vec3> missIndicators; // x,z for position, y for timer
float missIndicatorDuration = 1.5f;

// Enhanced collection effect properties (Fruit Ninja style)
struct CollectionEffect {
    glm::vec3 position;
    glm::vec3 color;
    float timer;
    float duration;
    bool active;
    std::vector<glm::vec3> particlePositions;
    std::vector<glm::vec3> particleVelocities;
    std::vector<glm::vec3> particleSizes;
    std::vector<float> particleRotations;
    std::vector<float> particleRotationSpeeds;
};

std::vector<CollectionEffect> collectionEffects;
const float COLLECTION_EFFECT_DURATION = 1.2f;
const int COLLECTION_PARTICLES = 16; // More particles for better effect

// Enhanced death effect
struct DeathEffect {
    glm::vec3 position;
    float timer;
    float duration;
    bool active;
    std::vector<glm::vec3> particlePositions;
    std::vector<glm::vec3> particleVelocities;
    std::vector<glm::vec3> particleSizes;
    std::vector<glm::vec3> particleColors;
};

std::vector<DeathEffect> deathEffects;
const float DEATH_EFFECT_DURATION = 2.0f;
const int DEATH_PARTICLES = 20;

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

// Text rendering structures
struct Character {
    unsigned int TextureID; // ID handle of the glyph texture
    glm::ivec2   Size;      // Size of glyph
    glm::ivec2   Bearing;   // Offset from baseline to left/top of glyph
    unsigned int Advance;   // Horizontal offset to advance to next glyph
};

std::map<char, Character> Characters;
unsigned int textVAO, textVBO;
unsigned int textShaderProgram;

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

// Miss indicator shader (simple unlit shader)
const char* missVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* missFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

uniform float alpha;

void main()
{
    FragColor = vec4(1.0, 0.0, 0.0, alpha);
}
)";

// Effect shader
const char* effectVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* effectFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 effectColor;
uniform float alpha;

void main()
{
    FragColor = vec4(effectColor, alpha);
}
)";

// Text shader sources
const char* textVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
out vec2 TexCoords;

uniform mat4 projection;

void main()
{
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

const char* textFragmentShaderSource = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;
uniform vec3 textColor;

void main()
{    
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(textColor, 1.0) * sampled;
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
    // Remove any inactive eggs first to keep the list clean
    eggs.erase(std::remove_if(eggs.begin(), eggs.end(),
        [](const Egg& egg) { return !egg.active; }), eggs.end());

    // Only spawn if we have room for more eggs and game is active
    int activeEggCount = std::count_if(eggs.begin(), eggs.end(),
        [](const Egg& egg) { return egg.active && !egg.isPoison; });

    if (activeEggCount < MAX_EGGS && currentGameState == GAME_PLAYING) {
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
    // Remove any inactive eggs first to keep the list clean
    eggs.erase(std::remove_if(eggs.begin(), eggs.end(),
        [](const Egg& egg) { return !egg.active; }), eggs.end());

    int poisonEggCount = std::count_if(eggs.begin(), eggs.end(),
        [](const Egg& egg) { return egg.active && egg.isPoison; });

    if (poisonEggCount < MAX_POISON_EGGS && currentGameState == GAME_PLAYING) {
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

// Enhanced collection effect creation (Fruit Ninja style)
void createCollectionEffect(const glm::vec3& position, const glm::vec3& color) {
    CollectionEffect effect;
    effect.position = position;
    effect.color = color;
    effect.timer = COLLECTION_EFFECT_DURATION;
    effect.duration = COLLECTION_EFFECT_DURATION;
    effect.active = true;

    // Create particles for burst effect - Fruit Ninja style
    for (int i = 0; i < COLLECTION_PARTICLES; i++) {
        // Random direction in a more controlled burst pattern
        float angle = (float)i / COLLECTION_PARTICLES * 2.0f * 3.14159f;
        float spread = 0.3f + ((float)rand() / RAND_MAX) * 0.7f;
        float speed = 3.0f + ((float)rand() / RAND_MAX) * 4.0f;

        glm::vec3 velocity = glm::vec3(
            cos(angle) * speed * spread,
            1.5f + ((float)rand() / RAND_MAX) * 3.0f, // More upward velocity
            sin(angle) * speed * spread
        );

        // Random particle size (like fruit chunks in Fruit Ninja)
        glm::vec3 size = glm::vec3(
            0.1f + ((float)rand() / RAND_MAX) * 0.2f,
            0.1f + ((float)rand() / RAND_MAX) * 0.2f,
            0.1f + ((float)rand() / RAND_MAX) * 0.2f
        );

        effect.particlePositions.push_back(position);
        effect.particleVelocities.push_back(velocity);
        effect.particleSizes.push_back(size);
        effect.particleRotations.push_back((float)rand() / RAND_MAX * 6.28318f);
        effect.particleRotationSpeeds.push_back(((float)rand() / RAND_MAX - 0.5f) * 10.0f);
    }

    collectionEffects.push_back(effect);
    std::cout << "Collection effect created at (" << position.x << ", " << position.z << ")" << std::endl;
}

// Enhanced death effect creation
void createDeathEffect(const glm::vec3& position) {
    DeathEffect effect;
    effect.position = position;
    effect.timer = DEATH_EFFECT_DURATION;
    effect.duration = DEATH_EFFECT_DURATION;
    effect.active = true;

    // Create particles for death explosion
    for (int i = 0; i < DEATH_PARTICLES; i++) {
        // Random spherical direction for explosion
        float theta = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
        float phi = acos(2.0f * ((float)rand() / RAND_MAX) - 1.0f);
        float speed = 3.0f + ((float)rand() / RAND_MAX) * 4.0f;

        glm::vec3 velocity = glm::vec3(
            sin(phi) * cos(theta) * speed,
            sin(phi) * sin(theta) * speed,
            cos(phi) * speed
        );

        // Random particle size
        glm::vec3 size = glm::vec3(
            0.15f + ((float)rand() / RAND_MAX) * 0.25f,
            0.15f + ((float)rand() / RAND_MAX) * 0.25f,
            0.15f + ((float)rand() / RAND_MAX) * 0.25f
        );

        // Purple color with some variation
        glm::vec3 particleColor = glm::vec3(
            0.6f + ((float)rand() / RAND_MAX) * 0.3f,
            0.1f + ((float)rand() / RAND_MAX) * 0.2f,
            0.7f + ((float)rand() / RAND_MAX) * 0.2f
        );

        effect.particlePositions.push_back(position);
        effect.particleVelocities.push_back(velocity);
        effect.particleSizes.push_back(size);
        effect.particleColors.push_back(particleColor);
    }

    deathEffects.push_back(effect);
    std::cout << "Death effect created at (" << position.x << ", " << position.z << ")" << std::endl;
}

// Update collection effects
void updateCollectionEffects() {
    for (auto& effect : collectionEffects) {
        if (effect.active) {
            effect.timer -= deltaTime;

            // Update particle positions and rotations
            for (size_t i = 0; i < effect.particlePositions.size(); i++) {
                effect.particlePositions[i] += effect.particleVelocities[i] * deltaTime;
                // Apply gravity
                effect.particleVelocities[i].y -= 9.8f * deltaTime;
                // Update rotation
                effect.particleRotations[i] += effect.particleRotationSpeeds[i] * deltaTime;
            }

            // Deactivate when timer expires
            if (effect.timer <= 0.0f) {
                effect.active = false;
            }
        }
    }

    // Remove inactive effects
    collectionEffects.erase(std::remove_if(collectionEffects.begin(), collectionEffects.end(),
        [](const CollectionEffect& effect) { return !effect.active; }), collectionEffects.end());
}

// Update death effects
void updateDeathEffects() {
    for (auto& effect : deathEffects) {
        if (effect.active) {
            effect.timer -= deltaTime;

            // Update particle positions
            for (size_t i = 0; i < effect.particlePositions.size(); i++) {
                effect.particlePositions[i] += effect.particleVelocities[i] * deltaTime;
                // Apply gravity
                effect.particleVelocities[i].y -= 9.8f * deltaTime;
            }

            // Deactivate when timer expires
            if (effect.timer <= 0.0f) {
                effect.active = false;
            }
        }
    }

    // Remove inactive effects
    deathEffects.erase(std::remove_if(deathEffects.begin(), deathEffects.end(),
        [](const DeathEffect& effect) { return !effect.active; }), deathEffects.end());
}

// Player death function
void killPlayer() {
    if (playerAlive) {
        playerAlive = false;
        lives--;
        playerRespawnTimer = PLAYER_RESPAWN_TIME;

        // Check for game over due to no lives
        if (lives <= 0) {
            currentGameState = GAME_OVER;
            std::cout << "GAME OVER! Final Score: " << score << std::endl;
            std::cout << "Reason: No lives remaining!" << std::endl;
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

// Check for missed eggs (Fruit Ninja style)
void checkForMissedEggs() {
    for (auto it = eggs.begin(); it != eggs.end(); ) {
        if (it->active && !it->isPoison && it->lifeTimer <= 0.0f) {
            // Egg expired without being collected - this is a miss!
            missedEggs++;

            // Add miss indicator at egg position
            glm::vec3 missIndicator = glm::vec3(it->position.x, missIndicatorDuration, it->position.z);
            missIndicators.push_back(missIndicator);

            std::cout << "Missed egg! Misses: " << missedEggs << "/" << MAX_MISSES << std::endl;

            // Check for game over due to too many misses
            if (missedEggs >= MAX_MISSES) {
                currentGameState = GAME_OVER;
                std::cout << "GAME OVER! Too many missed eggs! Final Score: " << score << std::endl;
            }

            // Remove the expired egg
            it = eggs.erase(it);
        }
        else {
            ++it;
        }
    }
}

// Update miss indicators
void updateMissIndicators() {
    // Remove expired miss indicators
    missIndicators.erase(std::remove_if(missIndicators.begin(), missIndicators.end(),
        [](const glm::vec3& indicator) {
            return indicator.y <= 0.0f; // Use y component as timer
        }), missIndicators.end());

    // Update timers for active indicators
    for (auto& indicator : missIndicators) {
        indicator.y -= deltaTime; // Decrease timer (stored in y component)
    }
}

// Update egg animations and lifecycle
void updateEggs() {
    // Don't update eggs if game is not playing
    if (currentGameState != GAME_PLAYING) return;

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

            // Check for collisions with player (only if player is alive)
            if (playerAlive) {
                float distance = glm::distance(playerPos, egg.position);
                float collisionDistance = playerRadius + egg.radius * egg.scale;

                if (distance < collisionDistance) {
                    if (egg.isPoison) {
                        // Poison egg - kill player immediately
                        createDeathEffect(egg.position); // Add death effect
                        killPlayer();
                        egg.active = false;
                        std::cout << "Player hit poison egg! Lives: " << lives << std::endl;
                    }
                    else {
                        // Regular egg - collect and score
                        createCollectionEffect(egg.position, egg.color); // Add collection effect
                        egg.active = false;
                        score += 10;
                        std::cout << "Egg collected! Score: " << score << std::endl;
                    }
                }
            }

            // Deactivate poison eggs if lifespan is over (regular eggs are handled in checkForMissedEggs)
            if (egg.isPoison && egg.lifeTimer <= 0.0f) {
                egg.active = false;
                std::cout << "Poison egg despawned!" << std::endl;
            }
        }
    }

    // Remove inactive eggs (only poison eggs, regular eggs are removed in checkForMissedEggs)
    eggs.erase(std::remove_if(eggs.begin(), eggs.end(),
        [](const Egg& egg) { return !egg.active && egg.isPoison; }), eggs.end());
}

// Update player respawn
void updatePlayer() {
    if (currentGameState != GAME_PLAYING) return; // Don't update player if game not playing

    if (!playerAlive && currentGameState == GAME_PLAYING) {
        playerRespawnTimer -= deltaTime;
        if (playerRespawnTimer <= 0.0f) {
            respawnPlayer();
        }
    }
}


// Reset game function
void resetGame() {
    score = 0;
    lives = 3;
    missedEggs = 0;
    playerAlive = true;
    eggs.clear();
    missIndicators.clear();
    collectionEffects.clear();
    deathEffects.clear();
    playerPos = glm::vec3(0.0f, 1.0f, 0.0f);
    playerTargetPos = playerPos;
    playerRotation = 0.0f;
    playerRotationTarget = 0.0f;
    eggSpawnTimer = 0.0f;
    poisonEggSpawnTimer = 0.0f;
    playerRespawnTimer = 0.0f;

    std::cout << "Game reset! Ready for new game." << std::endl;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    SCR_WIDTH = width;
    SCR_HEIGHT = height;
    glViewport(0, 0, width, height);

    // Update text projection matrix when window is resized
    glUseProgram(textShaderProgram);
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH), 0.0f, static_cast<float>(SCR_HEIGHT));
    glUniformMatrix4fv(glGetUniformLocation(textShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
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

    // Don't process mouse input in start, paused, or game over screens
    if (currentGameState == GAME_START || currentGameState == GAME_PAUSED || currentGameState == GAME_OVER) return;

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

    // Don't process scroll in start, paused, or game over screens
    if (currentGameState == GAME_START || currentGameState == GAME_PAUSED || currentGameState == GAME_OVER) return;

    // Scroll wheel adjusts camera distance
    cameraTargetDistance -= yoffset * scrollSensitivity;
    if (cameraTargetDistance < 3.0f) cameraTargetDistance = 3.0f;
    if (cameraTargetDistance > 15.0f) cameraTargetDistance = 15.0f;
    updateCamera();
}

// Joystick input processing
void processJoystickInput() {
    if (!joystickPresent || currentGameState != GAME_PLAYING) return;

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



// In the processInput function, replace the ESC handling:
void processInput(GLFWwindow* window) {
    // Handle ESC key with different behaviors based on game state
    static bool escKeyPressed = false;
    static bool pKeyPressed = false;
    static bool rKeyPressed = false;
    static bool enterKeyPressed = false;
    static bool spaceKeyPressed = false;

    // Replace the current ESC handling section with this:
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (!escKeyPressed) {
            escKeyPressed = true;

            if (currentGameState == GAME_PLAYING) {
                // ESC during gameplay: pause the game
                currentGameState = GAME_PAUSED;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                std::cout << "Game paused! Press ESC again for Main Menu" << std::endl;
            }
            else if (currentGameState == GAME_PAUSED || currentGameState == GAME_OVER) {
                // ESC from pause/game over: reset game and go to start screen
                resetGame();
                currentGameState = GAME_START;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                std::cout << "Returning to Main Menu. Game has been reset." << std::endl;
            }
            else if (currentGameState == GAME_START) {
                // ESC at start screen: quit game
                glfwSetWindowShouldClose(window, true);
                std::cout << "Quitting game..." << std::endl;
            }
        }
    }
    else {
        escKeyPressed = false;
    }



    // Toggle ImGui settings window with F1
    static bool f1KeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
        if (!f1KeyPressed) {
            showSettings = !showSettings;
            f1KeyPressed = true;
        }
    }
    else {
        f1KeyPressed = false;
    }

    // Handle game state transitions
    if (currentGameState == GAME_START) {
        // Start game with ENTER or SPACE (with key debouncing)
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
            if (!enterKeyPressed) {
                currentGameState = GAME_PLAYING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                enterKeyPressed = true;
                std::cout << "Game started!" << std::endl;
            }
        }
        else {
            enterKeyPressed = false;
        }

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (!spaceKeyPressed) {
                currentGameState = GAME_PLAYING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                spaceKeyPressed = true;
                std::cout << "Game started!" << std::endl;
            }
        }
        else {
            spaceKeyPressed = false;
        }
    }
    else if (currentGameState == GAME_PLAYING) {
        // Pause game with P key (with key debouncing)
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
            if (!pKeyPressed) {
                if (currentGameState == GAME_PLAYING) {
                    currentGameState = GAME_PAUSED;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
                else if (currentGameState == GAME_PAUSED) {
                    currentGameState = GAME_PLAYING;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    firstMouse = true; // Reset firstMouse when resuming
                }
                pKeyPressed = true;
            }
        }
        else {
            pKeyPressed = false;
        }

        // Reset game with R key (with key debouncing) - FIXED
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            if (!rKeyPressed) {
                resetGame();
                // Ensure game goes back to playing state after reset
                if (currentGameState != GAME_START) {
                    currentGameState = GAME_PLAYING;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    firstMouse = true; // Reset mouse look
                }
                rKeyPressed = true;
                std::cout << "Game restarted! Score: 0, Lives: 3, Misses: 0" << std::endl;
            }
        }
        else {
            rKeyPressed = false;
        }

        // Player movement relative to camera direction using pre-calculated vectors
        glm::vec3 movement = glm::vec3(0.0f);

        if (playerAlive) {
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
    else if (currentGameState == GAME_PAUSED) {
        // Resume game with P key (with key debouncing)
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
            if (!pKeyPressed) {
                currentGameState = GAME_PLAYING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                pKeyPressed = true;
                std::cout << "Game resumed!" << std::endl;
            }
        }
        else {
            pKeyPressed = false;
        }
        // Reset game with R key (with key debouncing) - FIXED
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            if (!rKeyPressed) {
                resetGame();
                // Ensure game goes back to playing state after reset
                if (currentGameState != GAME_START) {
                    currentGameState = GAME_PLAYING;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    firstMouse = true; // Reset mouse look
                }
                rKeyPressed = true;
                std::cout << "Game restarted! Score: 0, Lives: 3, Misses: 0" << std::endl;
            }
        }
        else {
            rKeyPressed = false;
        }
    }
    else if (currentGameState == GAME_OVER) {
        // Reset game with R key (with key debouncing) - FIXED
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            if (!rKeyPressed) {
                resetGame();
                // Ensure game goes back to playing state after reset
                if (currentGameState != GAME_START) {
                    currentGameState = GAME_PLAYING;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    firstMouse = true; // Reset mouse look
                }
                rKeyPressed = true;
                std::cout << "Game restarted! Score: 0, Lives: 3, Misses: 0" << std::endl;
            }
        }
        else {
            rKeyPressed = false;
        }
    }
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

// Function to generate a cross shape for miss indicators
void generateCross(std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    vertices.clear();
    indices.clear();

    float size = 1.0f;

    // Two perpendicular lines forming a cross
    // Horizontal line
    vertices.insert(vertices.end(), {
        -size, 0.1f, 0.0f,  // position
        0.0f, 1.0f, 0.0f,   // normal (not used for miss shader)
        size, 0.1f, 0.0f,
        0.0f, 1.0f, 0.0f
        });

    // Vertical line
    vertices.insert(vertices.end(), {
        0.0f, 0.1f, -size,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.1f, size,
        0.0f, 1.0f, 0.0f
        });

    indices.insert(indices.end(), {
        0, 1,  // horizontal line
        2, 3   // vertical line
        });
}

// Initialize text rendering with FreeType
void initTextRendering() {
    // Compile and setup the shader
    textShaderProgram = createShaderProgram(textVertexShaderSource, textFragmentShaderSource);

    // Configure VAO/VBO for texture quads
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, "PressStart2P-Regular.ttf", 0, &face)) {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 30);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Load first 128 characters of ASCII set
    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            continue;
        }

        // Generate texture
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Configure shader
    glUseProgram(textShaderProgram);
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH), 0.0f, static_cast<float>(SCR_HEIGHT));
    glUniformMatrix4fv(glGetUniformLocation(textShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
}

// Render text function
void RenderText(std::string text, float x, float y, float scale, glm::vec3 color) {
    // Activate corresponding render state
    glUseProgram(textShaderProgram);
    glUniform3f(glGetUniformLocation(textShaderProgram, "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    // Iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        // Update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        // Render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        // Update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64)
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Render Start Screen
void renderStartScreen() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Title
    std::string titleText = "EGG COLLECTOR";
    float titleWidth = 0;
    for (char c : titleText) {
        Character ch = Characters[c];
        titleWidth += (ch.Advance >> 6) * 1.0f;
    }
    float titleX = (SCR_WIDTH - titleWidth) / 2.0f;
    RenderText(titleText, titleX, SCR_HEIGHT * 0.7f, 1.0f, glm::vec3(1.0f, 1.0f, 0.0f));

    // Subtitle
    std::string subtitleText = "Fruit Ninja Style!";
    float subtitleWidth = 0;
    for (char c : subtitleText) {
        Character ch = Characters[c];
        subtitleWidth += (ch.Advance >> 6) * 0.5f;
    }
    float subtitleX = (SCR_WIDTH - subtitleWidth) / 2.0f;
    RenderText(subtitleText, subtitleX, SCR_HEIGHT * 0.6f, 0.5f, glm::vec3(1.0f, 0.5f, 0.0f));

    // Instructions
    std::string instruction1 = "Collect colorful eggs, avoid purple poison eggs!";
    float inst1Width = 0;
    for (char c : instruction1) {
        Character ch = Characters[c];
        inst1Width += (ch.Advance >> 6) * 0.3f;
    }
    float inst1X = (SCR_WIDTH - inst1Width) / 2.0f;
    RenderText(instruction1, inst1X, SCR_HEIGHT * 0.45f, 0.3f, glm::vec3(0.8f, 0.8f, 0.8f));

    std::string instruction2 = "You can only miss " + std::to_string(MAX_MISSES) + " eggs total!";
    float inst2Width = 0;
    for (char c : instruction2) {
        Character ch = Characters[c];
        inst2Width += (ch.Advance >> 6) * 0.3f;
    }
    float inst2X = (SCR_WIDTH - inst2Width) / 2.0f;
    RenderText(instruction2, inst2X, SCR_HEIGHT * 0.4f, 0.3f, glm::vec3(0.8f, 0.8f, 0.8f));

    // Controls
    std::string controlsTitle = "CONTROLS:";
    float ctrlTitleWidth = 0;
    for (char c : controlsTitle) {
        Character ch = Characters[c];
        ctrlTitleWidth += (ch.Advance >> 6) * 0.4f;
    }
    float ctrlTitleX = (SCR_WIDTH - ctrlTitleWidth) / 2.0f;
    RenderText(controlsTitle, ctrlTitleX, SCR_HEIGHT * 0.3f, 0.4f, glm::vec3(0.3f, 0.8f, 1.0f));

    std::string controls1 = "WASD: Move   |   Mouse: Look   |   Scroll: Zoom";
    float ctrl1Width = 0;
    for (char c : controls1) {
        Character ch = Characters[c];
        ctrl1Width += (ch.Advance >> 6) * 0.25f;
    }
    float ctrl1X = (SCR_WIDTH - ctrl1Width) / 2.0f;
    RenderText(controls1, ctrl1X, SCR_HEIGHT * 0.25f, 0.25f, glm::vec3(0.7f, 0.7f, 0.7f));

    std::string controls2 = "P: Pause   |   R: Restart   |   F1: Settings   |   ESC: Quit";
    float ctrl2Width = 0;
    for (char c : controls2) {
        Character ch = Characters[c];
        ctrl2Width += (ch.Advance >> 6) * 0.25f;
    }
    float ctrl2X = (SCR_WIDTH - ctrl2Width) / 2.0f;
    RenderText(controls2, ctrl2X, SCR_HEIGHT * 0.22f, 0.25f, glm::vec3(0.7f, 0.7f, 0.7f));

    // Start prompt
    std::string startText = "Press ENTER or SPACE to Start";
    float startWidth = 0;
    for (char c : startText) {
        Character ch = Characters[c];
        startWidth += (ch.Advance >> 6) * 0.4f;
    }
    float startX = (SCR_WIDTH - startWidth) / 2.0f;

    // Blinking effect
    float blink = sin(glfwGetTime() * 3.0f) * 0.5f + 0.5f;
    RenderText(startText, startX, SCR_HEIGHT * 0.1f, 0.4f, glm::vec3(0.0f, 1.0f, 0.0f) * blink);

    glDisable(GL_BLEND);
}

// Render Pause Screen
void renderPauseScreen() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Semi-transparent background
    glDisable(GL_DEPTH_TEST);
    glUseProgram(textShaderProgram);
    glBindVertexArray(textVAO);

    // Draw a semi-transparent quad over the entire screen
    float vertices[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        SCR_WIDTH, 0.0f, 1.0f, 0.0f,
        SCR_WIDTH, SCR_HEIGHT, 1.0f, 1.0f,
        0.0f, SCR_HEIGHT, 0.0f, 1.0f
    };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    unsigned int tempVAO, tempVBO, tempEBO;
    glGenVertexArrays(1, &tempVAO);
    glGenBuffers(1, &tempVBO);
    glGenBuffers(1, &tempEBO);

    glBindVertexArray(tempVAO);
    glBindBuffer(GL_ARRAY_BUFFER, tempVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tempEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Use a simple shader for the overlay
    unsigned int overlayShader = createShaderProgram(
        "#version 330 core\n"
        "layout (location = 0) in vec4 aPos;\n"
        "void main() { gl_Position = vec4(aPos.xy * 2.0 - 1.0, 0.0, 1.0); }",
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main() { FragColor = vec4(0.0, 0.0, 0.0, 0.7); }"
    );

    glUseProgram(overlayShader);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glDeleteProgram(overlayShader);
    glDeleteVertexArrays(1, &tempVAO);
    glDeleteBuffers(1, &tempVBO);
    glDeleteBuffers(1, &tempEBO);

    glEnable(GL_DEPTH_TEST);

    // Pause text
    std::string pauseText = "GAME PAUSED";
    float pauseWidth = 0;
    for (char c : pauseText) {
        Character ch = Characters[c];
        pauseWidth += (ch.Advance >> 6) * 0.8f;
    }
    float pauseX = (SCR_WIDTH - pauseWidth) / 2.0f;
    RenderText(pauseText, pauseX, SCR_HEIGHT * 0.6f, 0.8f, glm::vec3(1.0f, 1.0f, 0.0f));

    // Continue prompt
    std::string continueText = "Press P to Continue";
    float continueWidth = 0;
    for (char c : continueText) {
        Character ch = Characters[c];
        continueWidth += (ch.Advance >> 6) * 0.4f;
    }
    float continueX = (SCR_WIDTH - continueWidth) / 2.0f;
    RenderText(continueText, continueX, SCR_HEIGHT * 0.4f, 0.4f, glm::vec3(1.0f, 1.0f, 1.0f));

    // Restart prompt
    std::string restartText = "Press R to Restart";
    float restartWidth = 0;
    for (char c : restartText) {
        Character ch = Characters[c];
        restartWidth += (ch.Advance >> 6) * 0.4f;
    }
    float restartX = (SCR_WIDTH - restartWidth) / 2.0f;
    RenderText(restartText, restartX, SCR_HEIGHT * 0.35f, 0.4f, glm::vec3(1.0f, 1.0f, 1.0f));

    glDisable(GL_BLEND);
}

// Render Game Over Screen
void renderGameOverScreen() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Semi-transparent background (same as pause screen)
    glDisable(GL_DEPTH_TEST);
    glUseProgram(textShaderProgram);
    glBindVertexArray(textVAO);

    float vertices[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        SCR_WIDTH, 0.0f, 1.0f, 0.0f,
        SCR_WIDTH, SCR_HEIGHT, 1.0f, 1.0f,
        0.0f, SCR_HEIGHT, 0.0f, 1.0f
    };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    unsigned int tempVAO, tempVBO, tempEBO;
    glGenVertexArrays(1, &tempVAO);
    glGenBuffers(1, &tempVBO);
    glGenBuffers(1, &tempEBO);

    glBindVertexArray(tempVAO);
    glBindBuffer(GL_ARRAY_BUFFER, tempVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tempEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    unsigned int overlayShader = createShaderProgram(
        "#version 330 core\n"
        "layout (location = 0) in vec4 aPos;\n"
        "void main() { gl_Position = vec4(aPos.xy * 2.0 - 1.0, 0.0, 1.0); }",
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main() { FragColor = vec4(0.2, 0.0, 0.0, 0.8); }"
    );

    glUseProgram(overlayShader);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glDeleteProgram(overlayShader);
    glDeleteVertexArrays(1, &tempVAO);
    glDeleteBuffers(1, &tempVBO);
    glDeleteBuffers(1, &tempEBO);

    glEnable(GL_DEPTH_TEST);

    // Game Over text
    std::string gameOverText = "GAME OVER";
    float gameOverWidth = 0;
    for (char c : gameOverText) {
        Character ch = Characters[c];
        gameOverWidth += (ch.Advance >> 6) * 1.0f;
    }
    float gameOverX = (SCR_WIDTH - gameOverWidth) / 2.0f;
    RenderText(gameOverText, gameOverX, SCR_HEIGHT * 0.7f, 1.0f, glm::vec3(1.0f, 0.0f, 0.0f));

    // Final score
    std::string scoreText = "Final Score: " + std::to_string(score);
    float scoreWidth = 0;
    for (char c : scoreText) {
        Character ch = Characters[c];
        scoreWidth += (ch.Advance >> 6) * 0.5f;
    }
    float scoreX = (SCR_WIDTH - scoreWidth) / 2.0f;
    RenderText(scoreText, scoreX, SCR_HEIGHT * 0.55f, 0.5f, glm::vec3(1.0f, 1.0f, 0.0f));

    // Game over reason
    std::string reasonText;
    if (missedEggs >= MAX_MISSES) {
        reasonText = "Too many missed eggs!";
    }
    else {
        reasonText = "No lives remaining!";
    }
    float reasonWidth = 0;
    for (char c : reasonText) {
        Character ch = Characters[c];
        reasonWidth += (ch.Advance >> 6) * 0.4f;
    }
    float reasonX = (SCR_WIDTH - reasonWidth) / 2.0f;
    RenderText(reasonText, reasonX, SCR_HEIGHT * 0.45f, 0.4f, glm::vec3(1.0f, 0.5f, 0.5f));

    // Restart prompt
    std::string restartText = "Press R to Play Again";
    float restartWidth = 0;
    for (char c : restartText) {
        Character ch = Characters[c];
        restartWidth += (ch.Advance >> 6) * 0.4f;
    }
    float restartX = (SCR_WIDTH - restartWidth) / 2.0f;
    RenderText(restartText, restartX, SCR_HEIGHT * 0.3f, 0.4f, glm::vec3(0.0f, 1.0f, 0.0f));

    // Return to menu prompt
    std::string menuText = "Press ESC for Main Menu";
    float menuWidth = 0;
    for (char c : menuText) {
        Character ch = Characters[c];
        menuWidth += (ch.Advance >> 6) * 0.3f;
    }
    float menuX = (SCR_WIDTH - menuWidth) / 2.0f;
    RenderText(menuText, menuX, SCR_HEIGHT * 0.25f, 0.3f, glm::vec3(0.7f, 0.7f, 0.7f));

    glDisable(GL_BLEND);
}

// Render HUD function
void renderHUD() {
    // Only show HUD during gameplay
    if (currentGameState != GAME_PLAYING) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render score in top-left
    std::string scoreText = "SCORE: " + std::to_string(score);
    RenderText(scoreText, 25.0f, SCR_HEIGHT - 50.0f, 0.5f, glm::vec3(1.0f, 1.0f, 0.0f));

    // Render lives in top-left below score
    std::string livesText = "LIVES: " + std::to_string(lives);
    glm::vec3 livesColor = (lives <= 1) ? glm::vec3(1.0f, 0.3f, 0.3f) : glm::vec3(0.3f, 1.0f, 0.3f);
    RenderText(livesText, 25.0f, SCR_HEIGHT - 90.0f, 0.5f, livesColor);

    // Render misses in top-left below lives
    std::string missesText = "MISSES: " + std::to_string(missedEggs) + "/" + std::to_string(MAX_MISSES);
    glm::vec3 missesColor = (missedEggs >= MAX_MISSES - 1) ? glm::vec3(1.0f, 0.3f, 0.3f) : glm::vec3(1.0f, 1.0f, 1.0f);
    RenderText(missesText, 25.0f, SCR_HEIGHT - 130.0f, 0.5f, missesColor);

    // Render respawn timer if player is dead but game isn't over
    if (!playerAlive && currentGameState == GAME_PLAYING) {
        std::string respawnText = "RESPAWNING IN: " + std::to_string(static_cast<int>(playerRespawnTimer) + 1);
        float textWidth = 0;
        for (char c : respawnText) {
            Character ch = Characters[c];
            textWidth += (ch.Advance >> 6) * 0.5f;
        }
        float x = (SCR_WIDTH - textWidth) / 2.0f;
        RenderText(respawnText, x, 100.0f, 0.5f, glm::vec3(1.0f, 0.5f, 0.0f));
    }

    // Render controls hint at bottom
    std::string controlsText = "WASD: Move  |  Mouse: Look  |  Scroll: Zoom  |  P: Pause  |  F1: Settings  |  ESC: Quit";
    RenderText(controlsText, 25.0f, 30.0f, 0.3f, glm::vec3(0.7f, 0.7f, 0.7f));

    glDisable(GL_BLEND);
}

void showCameraSettingsWindow() {
    if (!showSettings) return;

    ImGui::Begin("Game Settings", &showSettings, ImGuiWindowFlags_AlwaysAutoResize);

    // Game status
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "SCORE: %d", score);
    ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "LIVES: %d", lives);
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "MISSES: %d/%d", missedEggs, MAX_MISSES);

    // Game state
    std::string stateText;
    switch (currentGameState) {
    case GAME_START: stateText = "START SCREEN"; break;
    case GAME_PLAYING: stateText = "PLAYING"; break;
    case GAME_PAUSED: stateText = "PAUSED"; break;
    case GAME_OVER: stateText = "GAME OVER"; break;
    }
    ImGui::TextColored(ImVec4(0, 1, 1, 1), "STATE: %s", stateText.c_str());

    if (!playerAlive && currentGameState == GAME_PLAYING) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "RESPAWNING IN: %.1f", playerRespawnTimer);
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
            if (egg.active) {
                if (egg.isPoison) poisonEggCount++;
                else regularEggCount++;
            }
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

    if (ImGui::CollapsingHeader("Effect System")) {
        ImGui::Text("Collection Effects: %zu", collectionEffects.size());
        ImGui::Text("Death Effects: %zu", deathEffects.size());

        if (ImGui::Button("Test Collection Effect")) {
            createCollectionEffect(playerPos, glm::vec3(1.0f, 0.5f, 0.0f));
        }

        ImGui::SameLine();
        if (ImGui::Button("Test Death Effect")) {
            createDeathEffect(playerPos);
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

    if (ImGui::CollapsingHeader("Game State Controls")) {
        if (ImGui::Button("Start Game")) {
            currentGameState = GAME_PLAYING;
            glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause Game")) {
            currentGameState = GAME_PAUSED;
            glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        ImGui::SameLine();
        if (ImGui::Button("Game Over")) {
            currentGameState = GAME_OVER;
            glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        ImGui::SameLine();
        if (ImGui::Button("Main Menu")) {
            currentGameState = GAME_START;
            glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    if (ImGui::CollapsingHeader("Help")) {
        ImGui::Text("WASD: Move player");
        ImGui::Text("Mouse: Look around");
        ImGui::Text("Scroll: Zoom in/out");
        ImGui::Text("P: Pause/Resume game");
        ImGui::Text("R: Restart game");
        ImGui::Text("F1: Toggle this window");
        ImGui::Text("ESC: Quit / Return to menu");
        ImGui::Text("ENTER/SPACE: Start game from menu");
        ImGui::Text("World Boundaries: Player cannot leave the ground area");
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Colorful Eggs: +10 points");
        ImGui::TextColored(ImVec4(1, 0, 1, 1), "Purple Poison Eggs: -1 life");
        ImGui::Text("FRUIT NINJA STYLE:");
        ImGui::Text("  - Collect ALL regular eggs (max 3 misses)");
        ImGui::Text("  - Avoid poison eggs (instant death)");
        ImGui::Text("  - Red X appears where you miss an egg");
        ImGui::Text("  - Colorful burst effects when collecting eggs");
        ImGui::Text("  - Purple explosion effects when hitting poison eggs");
    }
    ImGui::End();
}

int main() {
    std::cout << "Egg Collector - Fruit Ninja Style!" << std::endl;
    std::cout << "FRUIT NINJA RULES:" << std::endl;
    std::cout << "  - Collect ALL regular eggs (you can only miss " << MAX_MISSES << ")" << std::endl;
    std::cout << "  - Poison eggs kill you immediately" << std::endl;
    std::cout << "  - Red X appears where you miss an egg" << std::endl;
    std::cout << "  - Colorful burst effects when collecting eggs" << std::endl;
    std::cout << "  - Purple explosion effects when hitting poison eggs" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - WASD: Move the sphere" << std::endl;
    std::cout << "  - Mouse: Look around" << std::endl;
    std::cout << "  - Scroll: Zoom in/out" << std::endl;
    std::cout << "  - P: Pause/Resume game" << std::endl;
    std::cout << "  - R: Restart game" << std::endl;
    std::cout << "  - F1: Toggle camera settings" << std::endl;
    std::cout << "  - Joystick: Left stick to move, Right stick to look, Triggers to zoom" << std::endl;
    std::cout << "  - ESC: Exit / Return to menu" << std::endl;
    std::cout << "  - ENTER/SPACE: Start game from menu" << std::endl;
    std::cout << "World Boundaries: Player is confined to a " << GROUND_SIZE << "x" << GROUND_SIZE << " area" << std::endl;
    std::cout << "Egg System:" << std::endl;
    std::cout << "  - Regular eggs (various colors): +10 points, must collect them all!" << std::endl;
    std::cout << "  - Poison eggs (purple): -1 life, avoid at all costs!" << std::endl;

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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Egg Collector - Fruit Ninja Style!", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Start with cursor enabled for the start screen
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    // Create shader programs
    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    unsigned int missShaderProgram = createShaderProgram(missVertexShaderSource, missFragmentShaderSource);
    unsigned int effectShaderProgram = createShaderProgram(effectVertexShaderSource, effectFragmentShaderSource);

    // Initialize text rendering
    initTextRendering();

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

    // Generate cross geometry for miss indicators
    std::vector<float> crossVertices;
    std::vector<unsigned int> crossIndices;
    generateCross(crossVertices, crossIndices);

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

    // Set up cross VAO, VBO, EBO for miss indicators
    unsigned int crossVAO, crossVBO, crossEBO;
    glGenVertexArrays(1, &crossVAO);
    glGenBuffers(1, &crossVBO);
    glGenBuffers(1, &crossEBO);

    glBindVertexArray(crossVAO);
    glBindBuffer(GL_ARRAY_BUFFER, crossVBO);
    glBufferData(GL_ARRAY_BUFFER, crossVertices.size() * sizeof(float), crossVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, crossEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, crossIndices.size() * sizeof(unsigned int), crossIndices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

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

    // Enable line rendering for miss indicators
    glLineWidth(3.0f);

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

        // Update game logic only when playing
        if (currentGameState == GAME_PLAYING) {
            // Update egg system
            updateEggs();

            // Check for missed eggs (Fruit Ninja style) - MUST be called AFTER updateEggs
            checkForMissedEggs();

            // Update miss indicators
            updateMissIndicators();

            // Update effect systems
            updateCollectionEffects();
            updateDeathEffects();

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
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Show camera settings window
        showCameraSettingsWindow();

        // Rendering
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render 3D scene for all states except start screen
        if (currentGameState != GAME_START) {
            // Use main shader program for 3D objects
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

            // Render player sphere (only if alive and in playing state)
            if (playerAlive && currentGameState == GAME_PLAYING) {
                glm::mat4 sphereModel = glm::mat4(1.0f);
                sphereModel = glm::translate(sphereModel, playerPos);
                sphereModel = glm::rotate(sphereModel, playerRotation, glm::vec3(0.0f, 1.0f, 0.0f));
                glm::vec3 playerColor = glm::vec3(0.8f, 0.2f, 0.2f);

                glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(sphereModel));
                glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, glm::value_ptr(playerColor));
                glBindVertexArray(sphereVAO);
                glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
            }

            // Render eggs with animations (only in playing state)
            if (currentGameState == GAME_PLAYING) {
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
            }

            // Render miss indicators (Fruit Ninja style) - only in playing state
            if (!missIndicators.empty() && currentGameState == GAME_PLAYING) {
                glUseProgram(missShaderProgram);

                // Set view and projection for miss shader
                glUniformMatrix4fv(glGetUniformLocation(missShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
                glUniformMatrix4fv(glGetUniformLocation(missShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

                glBindVertexArray(crossVAO);

                for (const auto& indicator : missIndicators) {
                    glm::mat4 crossModel = glm::mat4(1.0f);
                    crossModel = glm::translate(crossModel, glm::vec3(indicator.x, 0.2f, indicator.z)); // Position above ground
                    crossModel = glm::scale(crossModel, glm::vec3(1.5f, 1.5f, 1.5f)); // Scale up the cross

                    // Calculate alpha based on timer (fade out effect)
                    float alpha = indicator.y / missIndicatorDuration;

                    glUniformMatrix4fv(glGetUniformLocation(missShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(crossModel));
                    glUniform1f(glGetUniformLocation(missShaderProgram, "alpha"), alpha);

                    // Draw as lines
                    glDrawElements(GL_LINES, crossIndices.size(), GL_UNSIGNED_INT, 0);
                }
            }

            // Render collection effects (Fruit Ninja style)
            if (!collectionEffects.empty()) {
                glUseProgram(effectShaderProgram);

                // Set view and projection for effect shader
                glUniformMatrix4fv(glGetUniformLocation(effectShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
                glUniformMatrix4fv(glGetUniformLocation(effectShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

                glBindVertexArray(sphereVAO); // Use sphere geometry for particles

                for (const auto& effect : collectionEffects) {
                    if (effect.active) {
                        float progress = 1.0f - (effect.timer / effect.duration);
                        float alpha = (1.0f - progress) * 0.8f; // Fade out

                        glUniform3fv(glGetUniformLocation(effectShaderProgram, "effectColor"), 1, glm::value_ptr(effect.color));
                        glUniform1f(glGetUniformLocation(effectShaderProgram, "alpha"), alpha);

                        // Render each particle
                        for (size_t i = 0; i < effect.particlePositions.size(); i++) {
                            glm::mat4 particleModel = glm::mat4(1.0f);
                            particleModel = glm::translate(particleModel, effect.particlePositions[i]);
                            particleModel = glm::rotate(particleModel, effect.particleRotations[i], glm::vec3(0.0f, 1.0f, 0.0f));
                            particleModel = glm::scale(particleModel, effect.particleSizes[i] * (1.0f - progress * 0.5f)); // Shrink over time

                            glUniformMatrix4fv(glGetUniformLocation(effectShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(particleModel));
                            glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
                        }
                    }
                }
            }

            // Render death effects
            if (!deathEffects.empty()) {
                glUseProgram(effectShaderProgram);

                // Set view and projection for effect shader
                glUniformMatrix4fv(glGetUniformLocation(effectShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
                glUniformMatrix4fv(glGetUniformLocation(effectShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

                glBindVertexArray(sphereVAO);

                for (const auto& effect : deathEffects) {
                    if (effect.active) {
                        float progress = 1.0f - (effect.timer / effect.duration);
                        float alpha = (1.0f - progress) * 0.6f; // Fade out

                        // Render each particle with its own color
                        for (size_t i = 0; i < effect.particlePositions.size(); i++) {
                            glm::mat4 particleModel = glm::mat4(1.0f);
                            particleModel = glm::translate(particleModel, effect.particlePositions[i]);
                            particleModel = glm::scale(particleModel, effect.particleSizes[i] * (1.0f - progress * 0.7f)); // Shrink over time

                            glUniform3fv(glGetUniformLocation(effectShaderProgram, "effectColor"), 1, glm::value_ptr(effect.particleColors[i]));
                            glUniform1f(glGetUniformLocation(effectShaderProgram, "alpha"), alpha);
                            glUniformMatrix4fv(glGetUniformLocation(effectShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(particleModel));
                            glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
                        }
                    }
                }
            }
        }

        // Render appropriate UI based on game state
        switch (currentGameState) {
        case GAME_START:
            renderStartScreen();
            break;
        case GAME_PLAYING:
            renderHUD();
            break;
        case GAME_PAUSED:
            renderHUD(); // Show HUD behind pause screen
            renderPauseScreen();
            break;
        case GAME_OVER:
            renderHUD(); // Show HUD behind game over screen
            renderGameOverScreen();
            break;
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

    // Clean up text rendering resources
    glDeleteProgram(textShaderProgram);
    glDeleteVertexArrays(1, &textVAO);
    glDeleteBuffers(1, &textVBO);
    for (auto& character : Characters) {
        glDeleteTextures(1, &character.second.TextureID);
    }

    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteVertexArrays(1, &eggVAO);
    glDeleteVertexArrays(1, &poisonEggVAO);
    glDeleteVertexArrays(1, &groundVAO);
    glDeleteVertexArrays(1, &crossVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &eggVBO);
    glDeleteBuffers(1, &poisonEggVBO);
    glDeleteBuffers(1, &groundVBO);
    glDeleteBuffers(1, &crossVBO);
    glDeleteBuffers(1, &sphereEBO);
    glDeleteBuffers(1, &eggEBO);
    glDeleteBuffers(1, &poisonEggEBO);
    glDeleteBuffers(1, &groundEBO);
    glDeleteBuffers(1, &crossEBO);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(missShaderProgram);
    glDeleteProgram(effectShaderProgram);

    glfwTerminate();

    std::cout << "Application terminated successfully! Final Score: " << score << std::endl;
    return 0;
}