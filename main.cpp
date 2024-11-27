#include <iostream>
#include <raylib.h>
#include <deque>
#include <raymath.h>
#include <fstream>
#include <string>
#include <algorithm>
using namespace std;

static bool allowMove = false;
Color green = {173, 204, 96, 255};
Color darkGreen = {43, 51, 24, 255};

int cellSize = 30;
int cellCount = 25;
int offset = 75;
double lastUpdateTime = 0;

bool ElementInDeque(Vector2 element, deque<Vector2> deque) {
    for (unsigned int i = 0; i < deque.size(); i++) {
        if (Vector2Equals(deque[i], element)) {
            return true;
        }
    }
    return false;
}

bool EventTriggered(double interval) {
    double currentTime = GetTime();
    if (currentTime - lastUpdateTime >= interval) {
        lastUpdateTime = currentTime;
        return true;
    }
    return false;
}

class Particle {
public:
    Vector2 position;
    float lifespan;

    Particle(Vector2 pos) : position(pos), lifespan(1.0f) {}

    void Update() {
        lifespan -= 0.05f;
    }

    void Draw() {
        Color fade = ColorAlpha(WHITE, lifespan);
        DrawCircleV(Vector2{offset + position.x * cellSize, offset + position.y * cellSize}, cellSize / 4, fade);
    }
};

class Snake {
public:
    deque<Vector2> body = {Vector2{6, 9}, Vector2{5, 9}, Vector2{4, 9}};
    Vector2 direction = {1, 0};
    bool addSegment = false;
    Color skinColor = {43, 51, 24, 255};

    void Draw() {
        for (unsigned int i = 0; i < body.size(); i++) {
            float x = body[i].x;
            float y = body[i].y;
            Rectangle segment = Rectangle{offset + x * cellSize, offset + y * cellSize, (float)cellSize, (float)cellSize};
            DrawRectangleRounded(segment, 0.5, 6, skinColor);
        }
    }

    void Update() {
        body.push_front(Vector2Add(body[0], direction));
        if (addSegment) {
            addSegment = false;
        } else {
            body.pop_back();
        }
    }

    void Reset() {
        body = {Vector2{6, 9}, Vector2{5, 9}, Vector2{4, 9}};
        direction = {1, 0};
    }
};

class Food {
public:
    Vector2 position;
    Texture2D texture;

    Food(deque<Vector2> snakeBody) {
        Image image = LoadImage("Graphics/food.png");
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
        position = GenerateRandomPos(snakeBody);
    }

    ~Food() {
        UnloadTexture(texture);
    }

    void Draw() {
        DrawTexture(texture, offset + position.x * cellSize, offset + position.y * cellSize, WHITE);
    }

    Vector2 GenerateRandomCell() {
        float x = GetRandomValue(0, cellCount - 1);
        float y = GetRandomValue(0, cellCount - 1);
        return Vector2{x, y};
    }

    Vector2 GenerateRandomPos(deque<Vector2> snakeBody) {
        Vector2 position = GenerateRandomCell();
        while (ElementInDeque(position, snakeBody)) {
            position = GenerateRandomCell();
        }
        return position;
    }
};

class Obstacle {
public:
    deque<Vector2> blocks;

    void GenerateObstacles(deque<Vector2> snakeBody) {
        blocks.clear();
        for (int i = 0; i < 5; i++) {
            Vector2 position = { (float)GetRandomValue(0, cellCount - 1), (float)GetRandomValue(0, cellCount - 1) };
            if (!ElementInDeque(position, snakeBody)) {
                blocks.push_back(position);
            }
        }
    }

    void Draw() {
        for (auto block : blocks) {
            DrawRectangle(offset + block.x * cellSize, offset + block.y * cellSize, cellSize, cellSize, RED);
        }
    }
};

class Game {
public:
    Snake snake = Snake();
    Food food = Food(snake.body);
    Obstacle obstacle = Obstacle();
    bool running = true;
    bool paused = false;
    int score = 0;
    int finalscore=0;   
    int highestScore = 0;
    bool newHighScore = false;
    Sound eatSound, wallSound, powerUpSound;
    deque<Particle> particles;

    Game() {
        InitAudioDevice();
        eatSound = LoadSound("Sounds/eat.mp3");
        wallSound = LoadSound("Sounds/wall.mp3");
        powerUpSound = LoadSound("Sounds/powerup.mp3");
        LoadHighScore();
        obstacle.GenerateObstacles(snake.body);
    }

    ~Game() {
        UnloadSound(eatSound);
        UnloadSound(wallSound);
        UnloadSound(powerUpSound);
        CloseAudioDevice();
    }

    void TogglePause() {
        paused = !paused; // This will toggle the paused state
    }

    void Draw() {
        if (!running) {
            // Draw the Game Over screen
            DrawRectangle(150, 150, 600, 600, Color{0, 0, 0, 180});
            DrawText("GAME OVER", 300, 250, 50, WHITE);
            if (newHighScore) {
                DrawText("New High Score!", 320, 330, 30, YELLOW);
            }
            // Display finalScore instead of score
            DrawText(TextFormat("Your Score: %i", finalscore), 350, 450, 30, WHITE);
            DrawText(TextFormat("Highest Score: %i", highestScore), 320, 550, 30, WHITE);
            DrawText("Press Enter or Space to Retry!", 200, 500, 30, WHITE);
        } else if (paused) {
            // Draw the paused overlay if the game is paused
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.6f));
            DrawText("PAUSED", GetScreenWidth() / 2 - MeasureText("PAUSED", 50) / 2, GetScreenHeight() / 2 - 25, 50, WHITE);
        } else {
            // Draw normal game elements if running
            food.Draw();
            snake.Draw();
            obstacle.Draw();
            for (auto &particle : particles) particle.Draw();
        }
    }

    void Update() {
    if (!paused) {
        if (running) {
            snake.Update();
            CheckCollisionWithFood();
            CheckCollisionWithEdges();
            CheckCollisionWithTail();
            for (auto &particle : particles) particle.Update();
            particles.erase(remove_if(particles.begin(), particles.end(), [](Particle &p) { return p.lifespan <= 0; }), particles.end());
        }
    } 
    }

    void CheckCollisionWithFood() {
        if (Vector2Equals(snake.body[0], food.position)) {
            food.position = food.GenerateRandomPos(snake.body);
            snake.addSegment = true;
            score++;
            PlaySound(eatSound);
            particles.push_back(Particle(snake.body[0]));
            if (score % 5 == 0) obstacle.GenerateObstacles(snake.body);
        }
    }

    void CheckCollisionWithEdges() {
        if (snake.body[0].x == cellCount || snake.body[0].x == -1 || snake.body[0].y == cellCount || snake.body[0].y == -1) {
            GameOver();
        }
    }

    void CheckCollisionWithTail() {
        deque<Vector2> headlessBody = snake.body;
        headlessBody.pop_front();
        if (ElementInDeque(snake.body[0], headlessBody) || ElementInDeque(snake.body[0], obstacle.blocks)) {
            GameOver();
        }
    }

    void GameOver() {
        finalscore = score; // Store the player's score before resetting
        if (score > highestScore) {
            highestScore = score;
            newHighScore = true;
            SaveHighScore();
        } else {
            newHighScore = false;
        }
        snake.Reset();
        food.position = food.GenerateRandomPos(snake.body);
        obstacle.GenerateObstacles(snake.body);
        running = false;
        paused = true;
        score = 0;  // Reset the current score
        PlaySound(wallSound);
    }

    void ResetGame() {
        running = true;
        paused = false;
        snake.Reset();
        food.position = food.GenerateRandomPos(snake.body);
        obstacle.GenerateObstacles(snake.body);
        score = 0;
    }

    void SaveHighScore() {
        ofstream outFile("highscore.txt");
        if (outFile) outFile << highestScore;
    }

    void LoadHighScore() {
        ifstream inFile("highscore.txt");
        if (inFile) inFile >> highestScore;
    }
};

int main() {
    cout << "Starting the game..." << endl;
    InitWindow(2 * offset + cellSize * cellCount, 2 * offset + cellSize * cellCount, "Enhanced Snake Game");
    SetTargetFPS(150);

    Game game = Game();

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(green);

        if (EventTriggered(0.2)) {
            allowMove = true;
            game.Update();
        }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            game.ResetGame();  // Reset the game when either key is pressed
            }
        

        if (IsKeyPressed(KEY_P)) {
            game.TogglePause(); // Toggle the pause state
        }

        if (IsKeyPressed(KEY_UP) && game.snake.direction.y != 1 && allowMove) {
            game.snake.direction = {0, -1};
            game.running = true;
            allowMove = false;
        }
        if (IsKeyPressed(KEY_DOWN) && game.snake.direction.y != -1 && allowMove) {
            game.snake.direction = {0, 1};
            game.running = true;
            allowMove = false;
        }
        if (IsKeyPressed(KEY_LEFT) && game.snake.direction.x != 1 && allowMove) {
            game.snake.direction = {-1, 0};
            game.running = true;
            allowMove = false;
        }
        if (IsKeyPressed(KEY_RIGHT) && game.snake.direction.x != -1 && allowMove) {
            game.snake.direction = {1, 0};
            game.running = true;
            allowMove = false;
        }

        DrawRectangleLinesEx(Rectangle{(float)offset - 5, (float)offset - 5, (float)cellSize * cellCount + 10, (float)cellSize * cellCount + 10}, 5, darkGreen);
        DrawText("Retro Snake", offset - 5, 20, 40, darkGreen);
        DrawText(TextFormat("Score: %i", game.score), offset - 5, offset + cellSize * cellCount + 10, 30, darkGreen);
        DrawText(TextFormat("Highest Score: %i", game.highestScore), offset + cellSize * cellCount - 200, offset + cellSize * cellCount + 10, 30, darkGreen);

        game.Draw();
        EndDrawing();
    }                             

    CloseWindow();
    return 0;
}