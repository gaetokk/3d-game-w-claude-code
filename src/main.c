#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

typedef struct {
    Vector3 position;
    Vector3 velocity;
    float speed;
    float rotationY;
    bool isMoving;
} Player;

typedef struct {
    int a;
    int b;
    int operation; // 0: +, 1: -, 2: *
    int correctAnswer;
} MathProblem;

typedef struct {
    int score;
    MathProblem currentProblem;
    int possibleAnswers[8];
} GameState;

typedef struct {
    int hatSocket;
    int rightHandSocket;
    int leftHandSocket;
    bool showHat;
    bool showSword;
    bool showShield;
} Equipment;

typedef struct {
    Vector3 position;
    bool exists;
    int answerNumber;
} Tree;

typedef struct {
    Camera3D camera;
    Vector3 offset;
    float distance;
    float rotationX;
    float rotationY;
    float sensitivity;
} GameCamera;

void UpdatePlayer(Player* player, GameCamera* gameCamera);
void UpdateGameCamera(GameCamera* gameCamera, Player* player);
int FindBoneSocket(Model model, const char* socketName);
Matrix GetSocketTransform(Model model, ModelAnimation animation, int frameIndex, int socketIndex, Matrix modelTransform);
void CheckTreeRemoval(Player* player, Tree* trees, int treeCount, GameState* gameState);
void GenerateNewMathProblem(GameState* gameState, Tree* trees, int treeCount);
void ShuffleArray(int* array, int size);

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Character Movement Game");
    SetTargetFPS(60);
    
    // Initialize random seed for rock placement
    srand((unsigned int)time(NULL));
    
    // Initialize a few test trees
    Tree trees[20];
    int treeCount = 0;
    
    // Initialize game state
    GameState gameState = { 0 };
    gameState.score = 0;
    
    // Add more trees in a denser grid pattern
    for (int x = -3; x <= 3; x++) {
        for (int z = -3; z <= 3; z++) {
            if (x == 0 && z == 0) continue; // Skip center where player starts
            if (treeCount >= 20) break;
            
            trees[treeCount].position = (Vector3){x * 6.0f, 0, z * 6.0f};
            trees[treeCount].exists = true;
            trees[treeCount].answerNumber = 0; // Will be set when problem is generated
            treeCount++;
        }
        if (treeCount >= 20) break;
    }
    
    Player player = {
        .position = (Vector3){ 0.0f, 0.0f, 0.0f },
        .velocity = (Vector3){ 0.0f, 0.0f, 0.0f },
        .speed = 10.0f,
        .rotationY = 0.0f,
        .isMoving = false
    };
    
    GameCamera gameCamera = {
        .camera = {
            .position = (Vector3){ 0.0f, 0.0f, 0.0f }, // Will be calculated from rotation
            .target = (Vector3){ 0.0f, 0.0f, 0.0f },
            .up = (Vector3){ 0.0f, 1.0f, 0.0f },
            .fovy = 45.0f,
            .projection = CAMERA_PERSPECTIVE
        },
        .offset = (Vector3){ 0.0f, 8.0f, 12.0f },
        .distance = 20.0f,
        .rotationX = -60.0f,
        .rotationY = 0.0f,
        .sensitivity = 0.3f
    };
    
    // Generate initial math problem after trees are created
    GenerateNewMathProblem(&gameState, trees, treeCount);
    
    // Set initial camera position based on rotation
    UpdateGameCamera(&gameCamera, &player);
    
    Model characterModel = { 0 };
    ModelAnimation* modelAnimations = NULL;
    int animationCount = 0;
    int currentAnimation = 0;
    int currentFrame = 0;
    bool modelLoaded = false;
    
    // Equipment models
    Model hatModel = { 0 };
    Model swordModel = { 0 };
    Model shieldModel = { 0 };
    Equipment equipment = { -1, -1, -1, true, true, true };
    
    // Lighting setup
    Shader lightingShader = { 0 };
    Vector3 lightPos = { 10.0f, 10.0f, 10.0f };
    Color lightColor = WHITE;
    
    // Load basic lighting shader
    lightingShader = LoadShader("assets/shaders/lighting.vs", "assets/shaders/lighting.fs");
    if (lightingShader.id == 0) {
        // Use default shader if custom shader not found
        lightingShader = LoadShaderFromMemory(
            "#version 330\n"
            "in vec3 vertexPosition;\n"
            "in vec2 vertexTexCoord;\n"
            "in vec3 vertexNormal;\n"
            "uniform mat4 mvp;\n"
            "uniform mat4 matModel;\n"
            "out vec2 fragTexCoord;\n"
            "out vec3 fragNormal;\n"
            "out vec3 fragPosition;\n"
            "void main() {\n"
            "    fragTexCoord = vertexTexCoord;\n"
            "    fragNormal = normalize(vec3(matModel*vec4(vertexNormal, 0.0)));\n"
            "    fragPosition = vec3(matModel*vec4(vertexPosition, 1.0));\n"
            "    gl_Position = mvp*vec4(vertexPosition, 1.0);\n"
            "}",
            "#version 330\n"
            "in vec2 fragTexCoord;\n"
            "in vec3 fragNormal;\n"
            "in vec3 fragPosition;\n"
            "uniform sampler2D texture0;\n"
            "uniform vec4 colDiffuse;\n"
            "uniform vec3 lightPos;\n"
            "uniform vec3 viewPos;\n"
            "out vec4 finalColor;\n"
            "void main() {\n"
            "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
            "    vec3 lightColor = vec3(1.0);\n"
            "    vec3 ambient = lightColor * 0.3;\n"
            "    vec3 norm = normalize(fragNormal);\n"
            "    vec3 lightDir = normalize(lightPos - fragPosition);\n"
            "    float diff = max(dot(norm, lightDir), 0.0);\n"
            "    vec3 diffuse = diff * lightColor;\n"
            "    finalColor = vec4((ambient + diffuse), 1.0) * texelColor * colDiffuse;\n"
            "}"
        );
    }
    
    // Set shader uniform locations
    int lightPosLoc = GetShaderLocation(lightingShader, "lightPos");
    int viewPosLoc = GetShaderLocation(lightingShader, "viewPos");
    
    // Try to load character model
    if (FileExists("assets/models/greenman.glb")) {
        characterModel = LoadModel("assets/models/greenman.glb");
        modelAnimations = LoadModelAnimations("assets/models/greenman.glb", &animationCount);
        modelLoaded = true;
        printf("Loaded character model with %d animations\n", animationCount);
        
        // Apply lighting shader to character model (use materials[1] like raylib example)
        if (characterModel.materialCount > 1) {
            characterModel.materials[1].shader = lightingShader;
        } else {
            characterModel.materials[0].shader = lightingShader;
        }
        
        // Find bone sockets
        equipment.hatSocket = FindBoneSocket(characterModel, "socket_hat");
        equipment.rightHandSocket = FindBoneSocket(characterModel, "socket_hand_R");
        equipment.leftHandSocket = FindBoneSocket(characterModel, "socket_hand_L");
        
        printf("Hat socket: %d, Right hand: %d, Left hand: %d\n", 
               equipment.hatSocket, equipment.rightHandSocket, equipment.leftHandSocket);
    } else {
        printf("Character model not found. Using basic cube instead.\n");
    }
    
    // Load equipment models (without custom shader for now)
    if (FileExists("assets/models/greenman_hat.glb")) {
        hatModel = LoadModel("assets/models/greenman_hat.glb");
        printf("Loaded hat model\n");
    }
    if (FileExists("assets/models/greenman_sword.glb")) {
        swordModel = LoadModel("assets/models/greenman_sword.glb");
        printf("Loaded sword model\n");
    }
    if (FileExists("assets/models/greenman_shield.glb")) {
        shieldModel = LoadModel("assets/models/greenman_shield.glb");
        printf("Loaded shield model\n");
    }
    
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        
        UpdatePlayer(&player, &gameCamera);
        
        // Handle mouse input for camera rotation (vertical only)
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 mouseDelta = GetMouseDelta();
            // Only allow vertical rotation (X axis)
            gameCamera.rotationX -= mouseDelta.y * gameCamera.sensitivity;
            
            // Clamp vertical rotation to look down from above
            if (gameCamera.rotationX > -20.0f) gameCamera.rotationX = -20.0f;
            if (gameCamera.rotationX < -80.0f) gameCamera.rotationX = -80.0f;
        }
        
        UpdateGameCamera(&gameCamera, &player);
        
        // Check for tree removal
        CheckTreeRemoval(&player, trees, treeCount, &gameState);
        
        // Animation handling
        if (IsKeyPressed(KEY_T) && animationCount > 1) {
            currentAnimation = (currentAnimation + 1) % animationCount;
            currentFrame = 0;
        }
        if (IsKeyPressed(KEY_G) && animationCount > 1) {
            currentAnimation = (currentAnimation - 1 + animationCount) % animationCount;
            currentFrame = 0;
        }
        
        if (modelLoaded && animationCount > 0) {
            // Choose animation based on movement state
            int targetAnimation = 0; // idle animation
            if (player.isMoving && animationCount > 1) {
                targetAnimation = 1; // walking animation (if available)
            }
            
            // Switch animation if different from current
            if (currentAnimation != targetAnimation) {
                currentAnimation = targetAnimation;
                currentFrame = 0;
            }
            
            currentFrame++;
            if (currentFrame >= modelAnimations[currentAnimation].frameCount) {
                currentFrame = 0;
            }
            UpdateModelAnimationBones(characterModel, modelAnimations[currentAnimation], currentFrame);
        }
        
        BeginDrawing();
        ClearBackground(SKYBLUE);
        
        BeginMode3D(gameCamera.camera);
        
        // Update shader uniforms
        if (modelLoaded) {
            SetShaderValue(lightingShader, lightPosLoc, &lightPos, SHADER_UNIFORM_VEC3);
            SetShaderValue(lightingShader, viewPosLoc, &gameCamera.camera.position, SHADER_UNIFORM_VEC3);
        }
        
        // Draw light indicator
        DrawSphere(lightPos, 0.5f, YELLOW);
        
        // Draw background environment (trees and objects)
        float worldSize = 8 * 8.0f * 2.0f; // Calculate world size based on tree layout
        
        // Draw ground to match tree area size
        DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ worldSize, worldSize }, BEIGE);
        
        // Draw trees that still exist
        for (int i = 0; i < treeCount; i++)
        {
            if (trees[i].exists)
            {
                Vector3 pos = trees[i].position;
                // Draw trees (green cube on brown trunk)
                DrawCube((Vector3){ pos.x, 1.5f, pos.z }, 2.0f, 2.0f, 2.0f, LIME);
                DrawCube((Vector3){ pos.x, 0.5f, pos.z }, 0.5f, 1.0f, 0.5f, BROWN);
            }
        }
        
        // Draw simple number cubes above trees
        for (int i = 0; i < treeCount; i++)
        {
            if (trees[i].exists && trees[i].answerNumber > 0)
            {
                Vector3 pos = trees[i].position;
                Vector3 numberPos = {pos.x, pos.y + 3.5f, pos.z};
                
                // Draw a white cube above the tree
                DrawCube(numberPos, 1.0f, 1.0f, 1.0f, WHITE);
                DrawCubeWires(numberPos, 1.0f, 1.0f, 1.0f, BLACK);
            }
        }
        
        EndMode3D();
        
        // Draw tree answer numbers (2D overlay) - single position above tree
        for (int i = 0; i < treeCount; i++)
        {
            if (trees[i].exists && trees[i].answerNumber > 0)
            {
                Vector3 pos = trees[i].position;
                Vector3 numberWorldPos = {pos.x, pos.y + 3.5f, pos.z};
                Vector2 screenPos = GetWorldToScreen(numberWorldPos, gameCamera.camera);
                
                // Only draw if position is visible on screen
                if (screenPos.x >= 0 && screenPos.x <= SCREEN_WIDTH && 
                    screenPos.y >= 0 && screenPos.y <= SCREEN_HEIGHT)
                {
                    char numberText[10];
                    sprintf(numberText, "%d", trees[i].answerNumber);
                    int textWidth = MeasureText(numberText, 32);
                    
                    // Draw background rectangle for better visibility
                    DrawRectangle((int)screenPos.x - textWidth/2 - 6, (int)screenPos.y - 20, textWidth + 12, 36, (Color){0, 0, 0, 180});
                    // Draw number with outline
                    DrawText(numberText, (int)screenPos.x - textWidth/2 + 2, (int)screenPos.y - 16 + 2, 32, BLACK); // Shadow
                    DrawText(numberText, (int)screenPos.x - textWidth/2, (int)screenPos.y - 16, 32, WHITE);
                }
            }
        }
        
        BeginMode3D(gameCamera.camera);
        
        // Draw some rocks for variety at fixed positions
        Vector3 rockPositions[] = {
            {12.0f, 0.3f, 15.0f}, {-18.0f, 0.3f, -12.0f}, {25.0f, 0.3f, -8.0f},
            {-22.0f, 0.3f, 20.0f}, {8.0f, 0.3f, -25.0f}, {-10.0f, 0.3f, 30.0f},
            {35.0f, 0.3f, 5.0f}, {-28.0f, 0.3f, -18.0f}, {15.0f, 0.3f, 32.0f},
            {-5.0f, 0.3f, -35.0f}, {28.0f, 0.3f, -22.0f}, {-32.0f, 0.3f, 8.0f}
        };
        
        for (int i = 0; i < 12; i++)
        {
            DrawCube(rockPositions[i], 0.8f, 0.6f, 0.8f, GRAY);
        }
        
        DrawGrid((int)worldSize, 1.0f);
        
        // Draw character
        if (modelLoaded) {
            // Save original transform and apply rotation
            Matrix originalTransform = characterModel.transform;
            characterModel.transform = MatrixMultiply(MatrixRotateY(player.rotationY * DEG2RAD), 
                                                    MatrixTranslate(player.position.x, player.position.y, player.position.z));
            
            DrawModel(characterModel, Vector3Zero(), 1.0f, WHITE);
            
            // Restore original transform
            characterModel.transform = originalTransform;
            
            // Draw equipment using proper bone socket transforms with correct character transform
            Matrix characterTransform = MatrixMultiply(MatrixRotateY(player.rotationY * DEG2RAD), 
                                                     MatrixTranslate(player.position.x, player.position.y, player.position.z));
            
            if (equipment.showHat && equipment.hatSocket >= 0 && hatModel.meshCount > 0) {
                Transform *transform = &modelAnimations[currentAnimation].framePoses[currentFrame][equipment.hatSocket];
                Quaternion inRotation = characterModel.bindPose[equipment.hatSocket].rotation;
                Quaternion outRotation = transform->rotation;
                
                // Calculate socket rotation (angle between bone in initial pose and same bone in current animation frame)
                Quaternion rotate = QuaternionMultiply(outRotation, QuaternionInvert(inRotation));
                Matrix matrixTransform = QuaternionToMatrix(rotate);
                // Translate socket to its position in the current animation
                matrixTransform = MatrixMultiply(matrixTransform, MatrixTranslate(transform->translation.x, transform->translation.y, transform->translation.z));
                // Transform the socket using the transform of the character (angle and translate)
                matrixTransform = MatrixMultiply(matrixTransform, characterTransform);
                
                // Draw mesh at socket position with socket angle rotation (use materials[1] like raylib example)
                DrawMesh(hatModel.meshes[0], hatModel.materials[1], matrixTransform);
            }
            
            if (equipment.showSword && equipment.rightHandSocket >= 0 && swordModel.meshCount > 0) {
                Transform *transform = &modelAnimations[currentAnimation].framePoses[currentFrame][equipment.rightHandSocket];
                Quaternion inRotation = characterModel.bindPose[equipment.rightHandSocket].rotation;
                Quaternion outRotation = transform->rotation;
                
                // Calculate socket rotation (angle between bone in initial pose and same bone in current animation frame)
                Quaternion rotate = QuaternionMultiply(outRotation, QuaternionInvert(inRotation));
                Matrix matrixTransform = QuaternionToMatrix(rotate);
                // Translate socket to its position in the current animation
                matrixTransform = MatrixMultiply(matrixTransform, MatrixTranslate(transform->translation.x, transform->translation.y, transform->translation.z));
                // Transform the socket using the transform of the character (angle and translate)
                matrixTransform = MatrixMultiply(matrixTransform, characterTransform);
                
                // Draw mesh at socket position with socket angle rotation (use materials[1] like raylib example)
                DrawMesh(swordModel.meshes[0], swordModel.materials[1], matrixTransform);
            }
            
            if (equipment.showShield && equipment.leftHandSocket >= 0 && shieldModel.meshCount > 0) {
                Transform *transform = &modelAnimations[currentAnimation].framePoses[currentFrame][equipment.leftHandSocket];
                Quaternion inRotation = characterModel.bindPose[equipment.leftHandSocket].rotation;
                Quaternion outRotation = transform->rotation;
                
                // Calculate socket rotation (angle between bone in initial pose and same bone in current animation frame)
                Quaternion rotate = QuaternionMultiply(outRotation, QuaternionInvert(inRotation));
                Matrix matrixTransform = QuaternionToMatrix(rotate);
                // Translate socket to its position in the current animation
                matrixTransform = MatrixMultiply(matrixTransform, MatrixTranslate(transform->translation.x, transform->translation.y, transform->translation.z));
                // Transform the socket using the transform of the character (angle and translate)
                matrixTransform = MatrixMultiply(matrixTransform, characterTransform);
                
                // Draw mesh at socket position with socket angle rotation (use materials[1] like raylib example)
                DrawMesh(shieldModel.meshes[0], shieldModel.materials[1], matrixTransform);
            }
        } else {
            // Draw simple cube if model not available
            DrawCube(player.position, 2.0f, 2.0f, 2.0f, RED);
            DrawCubeWires(player.position, 2.0f, 2.0f, 2.0f, MAROON);
        }
        
        EndMode3D();
        
        // Equipment toggle controls
        if (IsKeyPressed(KEY_ONE)) equipment.showHat = !equipment.showHat;
        if (IsKeyPressed(KEY_TWO)) equipment.showSword = !equipment.showSword;  
        if (IsKeyPressed(KEY_THREE)) equipment.showShield = !equipment.showShield;
        
        // UI - Score display (top right)
        DrawText(TextFormat("Score: %d", gameState.score), SCREEN_WIDTH - 150, 10, 30, BLACK);
        
        // UI - Math problem display (center top)
        char problemText[100];
        char operatorChar = '+';
        if (gameState.currentProblem.operation == 1) operatorChar = '-';
        else if (gameState.currentProblem.operation == 2) operatorChar = '*';
        
        sprintf(problemText, "%d %c %d = ?", gameState.currentProblem.a, operatorChar, gameState.currentProblem.b);
        int textWidth = MeasureText(problemText, 40);
        DrawText(problemText, (SCREEN_WIDTH - textWidth) / 2, 20, 40, DARKBLUE);
        
        // UI - Controls
        DrawText("WASD or Arrow Keys to move", 10, 10, 20, DARKGRAY);
        DrawText("SPACE: Remove tree in front", 10, 30, 20, DARKGRAY);
        DrawText("Right Mouse Button + Drag: Tilt camera up/down", 10, 50, 20, DARKGRAY);
        DrawText(TextFormat("Position: (%.1f, %.1f, %.1f)", player.position.x, player.position.y, player.position.z), 10, 70, 20, DARKGRAY);
        if (modelLoaded) {
            DrawText(TextFormat("Animation: %d/%d", currentAnimation + 1, animationCount), 10, 100, 20, DARKGRAY);
            DrawText("Press T/G to change animation", 10, 130, 20, DARKGRAY);
            DrawText("1: Toggle Hat  2: Toggle Sword  3: Toggle Shield", 10, 160, 20, DARKGRAY);
            DrawText(TextFormat("Hat: %s  Sword: %s  Shield: %s", 
                              equipment.showHat ? "ON" : "OFF",
                              equipment.showSword ? "ON" : "OFF", 
                              equipment.showShield ? "ON" : "OFF"), 10, 190, 20, DARKGRAY);
        }
        
        EndDrawing();
    }
    
    if (modelLoaded) {
        UnloadModelAnimations(modelAnimations, animationCount);
        UnloadModel(characterModel);
    }
    
    // Unload equipment models
    if (hatModel.meshCount > 0) UnloadModel(hatModel);
    if (swordModel.meshCount > 0) UnloadModel(swordModel);
    if (shieldModel.meshCount > 0) UnloadModel(shieldModel);
    
    // Unload shader
    if (lightingShader.id > 0) UnloadShader(lightingShader);
    
    CloseWindow();
    return 0;
}

void GenerateNewMathProblem(GameState* gameState, Tree* trees, int treeCount) {
    // Count existing trees
    int existingTreeCount = 0;
    for (int i = 0; i < treeCount; i++) {
        if (trees[i].exists) {
            existingTreeCount++;
        }
    }
    
    if (existingTreeCount == 0) return; // No trees to assign answers to
    
    // Generate random math problem
    gameState->currentProblem.a = rand() % 20 + 1; // 1-20
    gameState->currentProblem.b = rand() % 20 + 1; // 1-20
    gameState->currentProblem.operation = rand() % 3; // 0: +, 1: -, 2: *
    
    // Calculate correct answer
    switch (gameState->currentProblem.operation) {
        case 0: // Addition
            gameState->currentProblem.correctAnswer = gameState->currentProblem.a + gameState->currentProblem.b;
            break;
        case 1: // Subtraction
            gameState->currentProblem.correctAnswer = gameState->currentProblem.a - gameState->currentProblem.b;
            break;
        case 2: // Multiplication
            gameState->currentProblem.correctAnswer = gameState->currentProblem.a * gameState->currentProblem.b;
            break;
    }
    
    // Determine how many answers we need
    int numAnswers = (existingTreeCount < 8) ? existingTreeCount : 8;
    
    // Create array of possible answers (including correct one)
    gameState->possibleAnswers[0] = gameState->currentProblem.correctAnswer;
    
    // Generate wrong answers
    for (int i = 1; i < numAnswers; i++) {
        int wrongAnswer;
        bool duplicate;
        int attempts = 0;
        do {
            duplicate = false;
            attempts++;
            // Generate wrong answer within reasonable range
            wrongAnswer = gameState->currentProblem.correctAnswer + (rand() % 21) - 10; // +/- 10
            if (wrongAnswer < 0) wrongAnswer = abs(wrongAnswer); // Keep positive
            
            // Check for duplicates
            for (int j = 0; j < i; j++) {
                if (gameState->possibleAnswers[j] == wrongAnswer) {
                    duplicate = true;
                    break;
                }
            }
        } while (duplicate && attempts < 50); // Prevent infinite loop
        
        gameState->possibleAnswers[i] = wrongAnswer;
    }
    
    // Shuffle the answers
    ShuffleArray(gameState->possibleAnswers, numAnswers);
    
    // Assign answers to existing trees
    int answerIndex = 0;
    for (int i = 0; i < treeCount && answerIndex < numAnswers; i++) {
        if (trees[i].exists) {
            trees[i].answerNumber = gameState->possibleAnswers[answerIndex];
            answerIndex++;
            printf("Tree %d assigned answer: %d\n", i, trees[i].answerNumber);
        }
    }
    
    printf("Math problem: %d %c %d = %d\n", 
           gameState->currentProblem.a, 
           (gameState->currentProblem.operation == 0) ? '+' : 
           (gameState->currentProblem.operation == 1) ? '-' : '*',
           gameState->currentProblem.b,
           gameState->currentProblem.correctAnswer);
}

void ShuffleArray(int* array, int size) {
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

void UpdatePlayer(Player* player, GameCamera* gameCamera) {
    Vector3 movement = { 0.0f, 0.0f, 0.0f };
    bool moving = false;
    
    // Input handling - screen relative movement
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
        movement.z -= 1.0f; // Forward relative to camera
        moving = true;
    }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
        movement.z += 1.0f; // Backward relative to camera
        moving = true;
    }
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
        movement.x -= 1.0f; // Left relative to camera
        moving = true;
    }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
        movement.x += 1.0f; // Right relative to camera
        moving = true;
    }
    
    player->isMoving = moving;
    
    if (moving) {
        // Normalize movement vector
        movement = Vector3Normalize(movement);
        
        // Since camera Y rotation is fixed at 0, movement is direct
        Vector3 worldMovement = {
            movement.x,
            0.0f,
            movement.z
        };
        
        // Calculate rotation based on movement direction
        player->rotationY = atan2f(worldMovement.x, worldMovement.z) * RAD2DEG;
        
        // Apply movement
        float deltaTime = GetFrameTime();
        player->velocity = Vector3Scale(worldMovement, player->speed);
        player->position = Vector3Add(player->position, Vector3Scale(player->velocity, deltaTime));
    } else {
        player->velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    }
}

void UpdateGameCamera(GameCamera* gameCamera, Player* player) {
    // Calculate camera position based on rotation angles
    float radX = gameCamera->rotationX * DEG2RAD;
    float radY = gameCamera->rotationY * DEG2RAD;
    
    // Calculate offset based on spherical coordinates (corrected for proper top-down view)
    Vector3 offset = {
        gameCamera->distance * cosf(radX) * sinf(radY),
        gameCamera->distance * -sinf(radX), // Negative to look down from above
        gameCamera->distance * cosf(radX) * cosf(radY)
    };
    
    // Position camera relative to player
    Vector3 targetPosition = Vector3Add(player->position, offset);
    
    // Smooth camera movement
    gameCamera->camera.position = Vector3Lerp(gameCamera->camera.position, targetPosition, 0.1f);
    gameCamera->camera.target = Vector3Lerp(gameCamera->camera.target, player->position, 0.1f);
}

int FindBoneSocket(Model model, const char* socketName) {
    for (int i = 0; i < model.boneCount; i++) {
        if (TextIsEqual(model.bones[i].name, socketName)) {
            return i;
        }
    }
    return -1; // Socket not found
}

Matrix GetSocketTransform(Model model, ModelAnimation animation, int frameIndex, int socketIndex, Matrix modelTransform) {
    if (socketIndex < 0 || socketIndex >= model.boneCount || frameIndex >= animation.frameCount) {
        return MatrixIdentity();
    }
    
    // Get current animation frame bone transform and bind pose
    Transform* frameTransform = &animation.framePoses[frameIndex][socketIndex];
    Transform* bindPose = &model.bindPose[socketIndex];
    
    // Calculate relative rotation from bind pose to current frame (same as raylib example)
    Quaternion inRotation = bindPose->rotation;
    Quaternion outRotation = frameTransform->rotation;
    Quaternion rotate = QuaternionMultiply(outRotation, QuaternionInvert(inRotation));
    
    // Create transform matrix (same order as raylib example)
    Matrix matrixTransform = QuaternionToMatrix(rotate);
    matrixTransform = MatrixMultiply(matrixTransform, MatrixTranslate(frameTransform->translation.x, 
                                                                     frameTransform->translation.y, 
                                                                     frameTransform->translation.z));
    
    // Apply character model transform (same as raylib example)
    return MatrixMultiply(matrixTransform, modelTransform);
}

void CheckTreeRemoval(Player* player, Tree* trees, int treeCount, GameState* gameState) {
    if (!IsKeyPressed(KEY_SPACE)) return;
    
    // Calculate direction player is facing
    float playerAngle = player->rotationY * DEG2RAD;
    Vector3 forward = { sinf(playerAngle), 0.0f, cosf(playerAngle) };
    
    // Check trees within removal range
    float removalRange = 4.0f;
    
    for (int i = 0; i < treeCount; i++) {
        if (!trees[i].exists) continue;
        
        Vector3 toTree = Vector3Subtract(trees[i].position, player->position);
        float distance = Vector3Length(toTree);
        
        // Check if tree is within range
        if (distance <= removalRange) {
            // Check if tree is roughly in front of player
            Vector3 normalizedToTree = Vector3Normalize(toTree);
            float dot = Vector3DotProduct(forward, normalizedToTree);
            
            // If dot product > 0.5, tree is in front of player (within ~60 degrees)
            if (dot > 0.5f) {
                trees[i].exists = false;
                
                // Check if this tree has the correct answer
                if (trees[i].answerNumber == gameState->currentProblem.correctAnswer) {
                    gameState->score += 1;
                    printf("Correct! Score +1. Tree removed at position (%.1f, %.1f)\n", trees[i].position.x, trees[i].position.z);
                } else {
                    gameState->score -= 2;
                    printf("Wrong! Score -2. Tree removed at position (%.1f, %.1f)\n", trees[i].position.x, trees[i].position.z);
                }
                
                // Generate new math problem
                GenerateNewMathProblem(gameState, trees, treeCount);
                
                // Respawn tree at random position
                float worldSize = 64.0f; // Match the world size
                float minDistance = 8.0f; // Minimum distance from player
                Vector3 newPos;
                int attempts = 0;
                
                // Try to find a valid position (not too close to player)
                do {
                    newPos.x = (float)(rand() % (int)(worldSize * 2)) - worldSize;
                    newPos.y = 0.0f;
                    newPos.z = (float)(rand() % (int)(worldSize * 2)) - worldSize;
                    attempts++;
                } while (Vector3Distance(newPos, player->position) < minDistance && attempts < 10);
                
                trees[i].position = newPos;
                trees[i].exists = true;
                printf("Tree respawned at position (%.1f, %.1f)\n", newPos.x, newPos.z);
                break; // Remove only one tree per space press
            }
        }
    }
}
