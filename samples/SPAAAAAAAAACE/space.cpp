#include <chrono>
#include <thread>

#include <renderer.hpp>
// #include <objects.h>
// #include <vulkan/vulkan.h>

volchara::Renderer renderer{};

void mvmnt(volchara::Object* obj, float passedSeconds, std::set<int> pressedKeys) {
    if (pressedKeys.contains(GLFW_KEY_UP)) obj->transform.rotation.up(passedSeconds);
    if (pressedKeys.contains(GLFW_KEY_DOWN)) obj->transform.rotation.down(passedSeconds);
    if (pressedKeys.contains(GLFW_KEY_LEFT)) obj->transform.rotation.left(passedSeconds);
    if (pressedKeys.contains(GLFW_KEY_RIGHT)) obj->transform.rotation.right(passedSeconds);
    if (pressedKeys.contains(GLFW_KEY_PERIOD)) obj->transform.rotation.cw(passedSeconds);
    if (pressedKeys.contains(GLFW_KEY_COMMA)) obj->transform.rotation.ccw(passedSeconds);
}

int main() {
    std::this_thread::sleep_for(std::chrono::seconds(2));  //screw xcode
    volchara::Plane obj1 = renderer.createPlane({{-0.3f, -0.5f, 0.0f}, {0.3f, -0.5f, 0.0f}, {0.3f, 0.5f, 0.0f}});
    volchara::Plane obj2 = renderer.createPlane({{0.0f, -0.2f, 0.0f}, {0.5f, -0.2f, 0.0f}, {0.5f, 0.4f, 0.0f}});
    volchara::Plane obj3 = renderer.createPlane({{0.3f, -0.8f, -0.3f}, {0.5f, -0.8f, -0.3f}, {0.5f, 0.1f, -0.3f}});
    obj2.loadTexture("resources/textures/nouwu.jpg");
    obj3.loadTexture("resources/textures/ovca.png");
    obj3.frameCallbacks.push_back(mvmnt);
    renderer.addObject(&obj1);
    renderer.addObject(&obj2);
    renderer.addObject(&obj3);
    renderer.run();
    return 0;
}