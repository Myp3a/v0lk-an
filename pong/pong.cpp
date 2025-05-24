#include <fw.h>
#include <objects.h>

#include <functional>
#include <random>

fw::Renderer renderer{};
std::mt19937 rnd(std::time(nullptr));

std::vector<fw::Vertex> vertices = {
        {{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };

class PlayerBar : public fw::Square2D {
    public:
    float speed = 0.2f;
    PlayerBar(std::array<float, 2> topLeft, std::array<float, 2> botRight, std::array<float, 3> color) : Square2D(topLeft, botRight, color) {}

};

void moveRightBar(fw::Object* object, float passedSeconds, std::set<int> pressedKeys) {
    PlayerBar* bar = dynamic_cast<PlayerBar*>(object);
    if (pressedKeys.contains(GLFW_KEY_UP) && (bar->boundUp() > -1.0f)) {
        bar->moveUp(bar->speed * passedSeconds);
    }
    if (pressedKeys.contains(GLFW_KEY_DOWN) && (bar->boundDown() < 1.0f)) {
        bar->moveDown(bar->speed * passedSeconds);
    }
    if (pressedKeys.contains(GLFW_KEY_RIGHT) && (bar->boundRight() < 1.0f)) {
        bar->moveRight(bar->speed * passedSeconds);
    }
    if (pressedKeys.contains(GLFW_KEY_LEFT) && (bar->boundLeft() > 0.0f)) {
        bar->moveLeft(bar->speed * passedSeconds);
    }
}

class Ball : public fw::Square2D {
    public:
        fw::Square2D* leftBar;
        fw::Square2D* rightBar;
        float speed = 0.5f;
        glm::vec2 direction{0.71f, -0.71f};
    
        Ball(std::array<float, 2> topLeft, std::array<float, 2> botRight, std::array<float, 3> color, fw::Square2D* left, fw::Square2D* right) : fw::Square2D(topLeft, botRight, color), leftBar(left), rightBar(right) {}
    };

void moveBall(fw::Object* obj, float passedSeconds, std::set<int> pressedKeys) {
    Ball* ball = dynamic_cast<Ball*>(obj);
    if (ball->leftBar->boundRight() > ball->boundLeft() && ball->boundDown() > ball->leftBar->boundUp() && ball->boundUp() < ball->leftBar->boundDown()) {
        glm::vec2 leftCenter{(ball->leftBar->boundLeft() + ball->leftBar->boundRight()) / 2, (ball->leftBar->boundUp() + ball->leftBar->boundDown()) / 2};
        glm::vec2 ballCenter{(ball->boundLeft() + ball->boundRight()) / 2, (ball->boundUp() + ball->boundDown()) / 2};
        ball->direction = ballCenter - leftCenter;
        ball->direction.g *= -1;
        ball->direction = glm::normalize(ball->direction);
        ball->speed += 0.01f;
    }
    if (ball->rightBar->boundLeft() < ball->boundRight() && ball->boundDown() > ball->rightBar->boundUp() && ball->boundUp() < ball->rightBar->boundDown()) {
        glm::vec2 rightCenter{(ball->rightBar->boundLeft() + ball->rightBar->boundRight()) / 2, (ball->rightBar->boundUp() + ball->rightBar->boundDown()) / 2};
        glm::vec2 ballCenter{(ball->boundLeft() + ball->boundRight()) / 2, (ball->boundUp() + ball->boundDown()) / 2};
        ball->direction = ballCenter - rightCenter;
        ball->direction.g *= -1;
        ball->direction = glm::normalize(ball->direction);
        ball->speed += 0.01f;
    }
    if (ball->boundUp() < -1.0f || ball->boundDown() > 1.0f) {
        ball->direction.g *= -1;
        ball->speed += 0.01f;
    }
    ball->moveUp(ball->direction.g * ball->speed * passedSeconds);
    ball->moveRight(ball->direction.r * ball->speed * passedSeconds);
}

// void restart(fw::Object* obj, float passedSeconds, std::set<int> pressedKeys) {
//     Ball* ball = dynamic_cast<Ball*>(obj);
//     if (pressedKeys.contains(GLFW_KEY_R)) {
//         ball->speed = 0.5f;
//         ball->moveLeft(ball->vertices[0].pos.r);
//         ball->moveUp(ball->vertices[0].pos.g);
//         ball->direction.r = 0.71f;
//         ball->direction.g = -0.71f;
//     }
// }

class AIBar : public fw::Square2D {
    public:
        Ball* ball;
        float currentSpeed = 0.0f;
        float targetCoord = 0.0f;
        bool shouldRetarget = true;

        AIBar(std::array<float, 2> topLeft, std::array<float, 2> botRight, std::array<float, 3> color) : Square2D(topLeft, botRight, color) {}

        void setBall(Ball* ball_ptr) {
            ball = ball_ptr;
        }
};

void moveLeftBar(fw::Object* obj, float passedSeconds, std::set<int> pressedKeys) {
    AIBar* bar = dynamic_cast<AIBar*>(obj);
    if (bar->shouldRetarget) {
        bar->shouldRetarget = false;
        bar->targetCoord = (rnd() % 100) / 84 - 0.6f;
        bar->currentSpeed = (rnd() % 7) / 10 + 0.3f;
    }
    float distToTarget = bar->targetCoord - (bar->boundUp() + bar->boundDown()) / 2;
    if (abs(distToTarget) < 0.01f) {
        bar->shouldRetarget = true;
        return;
    }
    int sign = (distToTarget < 0) ? -1 : 1;
    bar->moveDown(bar->currentSpeed * passedSeconds * sign);
}

class GameManager : public fw::Object {
    public:
    AIBar* enemy;
    PlayerBar* player;
    std::vector<Ball*> balls;

    GameManager(AIBar* enemy, PlayerBar* player) : Object({}), enemy(enemy), player(player) {}

    Ball* spawnBall() {
        Ball* ball = new Ball({-0.05f, -0.05f}, {0.05f, 0.05f}, {1.0f, 1.0f, 1.0f}, enemy, player);
        balls.push_back(ball);
        ball->frameCallbacks.push_back(moveBall);
        renderer.addObject(ball);
        return ball;
    }
};

class Bonus : public fw::Square2D {
    public:
    GameManager* mgr;
    Bonus(std::array<float, 2> topLeft, std::array<float, 2> botRight, std::array<float, 3> color) : Square2D(topLeft, botRight, color) {}

    virtual void doBonus() {};
};

class SpeedBonus : public Bonus {
    public:
    float bonus = 0.1f;
    SpeedBonus(std::array<float, 2> topLeft, std::array<float, 2> botRight) : Bonus(topLeft, botRight, {0.0f, 1.0f, 0.0f}) {}

    void doBonus() override {
        mgr->player->speed += 0.1f;
    }
};

class BallBonus : public Bonus {
    public:
    BallBonus(std::array<float, 2> topLeft, std::array<float, 2> botRight) : Bonus(topLeft, botRight, {1.0f, 0.0f, 1.0f}) {}

    void doBonus() override {
        mgr->spawnBall();
    }
};

void collectBonus(fw::Object* obj, float passedSeconds, std::set<int> pressedKeys) {
    Bonus* b = dynamic_cast<Bonus*>(obj);
    GameManager* mgr = b->mgr;
    PlayerBar* bar = mgr->player;
    glm::vec2 rightCenter{(bar->boundLeft() + bar->boundRight()) / 2, (bar->boundUp() + bar->boundDown()) / 2};
    glm::vec2 bonusCenter{(b->boundLeft() + b->boundRight()) / 2, (b->boundUp() + b->boundDown()) / 2};
    float dist = glm::length(bonusCenter - rightCenter);
    if (dist < 0.1f) {
        b->doBonus();
        renderer.delObject(b);
        delete b;
    }
}

void spawnBonus(fw::Object* obj, float passedSeconds, std::set<int> pressedKeys) {
    if ((int)((rnd() % 300) / passedSeconds) == 0) {
        GameManager* mgr = dynamic_cast<GameManager*>(obj);
        float top = (float)(rnd() % 19) / 10 - 1.0f;
        float left = (float)(rnd() % 9) / 10;
        if (rnd() % 2 == 0) {
            SpeedBonus* speed = new SpeedBonus({left, top}, {left + 0.1f, top + 0.1f});
            speed->frameCallbacks.push_back(collectBonus);
            speed->mgr = mgr;
            renderer.addObject(speed);
        } else {
            BallBonus* ball = new BallBonus({left, top}, {left + 0.1f, top + 0.1f});
            ball->frameCallbacks.push_back(collectBonus);
            ball->mgr = mgr;
            renderer.addObject(ball);
        }
    }
}

int main() {
    AIBar leftBar({-1.0f, 1.0f}, {-0.9f, 0.2f}, {1.0f, 0.0f, 0.0f});
    PlayerBar rightBar({0.9f, 1.0f}, {1.0f, 0.2f}, {0.0f, 0.0f, 1.0f});
    GameManager manager(&leftBar, &rightBar);
    manager.frameCallbacks.push_back(spawnBonus);
    rightBar.frameCallbacks.push_back(moveRightBar);
    leftBar.setBall(manager.spawnBall());
    leftBar.frameCallbacks.push_back(moveLeftBar);
    renderer.addObject(&leftBar);
    renderer.addObject(&rightBar);
    renderer.addObject(&manager);
    renderer.run();
}
