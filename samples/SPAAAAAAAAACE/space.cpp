#include <chrono>
#include <thread>

#include <renderer.hpp>
// #include <objects.h>
// #include <vulkan/vulkan.h>

volchara::Renderer renderer{};

void mvmnt(volchara::Object* obj, float passedSeconds, std::set<int> pressedKeys) {
    if (pressedKeys.contains(GLFW_KEY_UP)) obj->transform.rotation.up(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_DOWN)) obj->transform.rotation.down(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_LEFT)) obj->transform.rotation.left(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_RIGHT)) obj->transform.rotation.right(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_PERIOD)) obj->transform.rotation.cw(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_COMMA)) obj->transform.rotation.ccw(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_I)) obj->transform.rotation.up(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_K)) obj->transform.rotation.down(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_J)) obj->transform.rotation.left(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_L)) obj->transform.rotation.right(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_M)) obj->transform.rotation.cw(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_N)) obj->transform.rotation.ccw(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_1)) obj->transform.rotation.left(passedSeconds);
    if (pressedKeys.contains(GLFW_KEY_2)) obj->transform.rotation.right(passedSeconds);
}

int main() {
    std::this_thread::sleep_for(std::chrono::seconds(2));  //screw xcode
    volchara::Plane obj1 = renderer.objPlaneFromWorldCoordinates({{-0.3f, 0.5f, 0.01f}, {0.3f, 0.5f, 0.01f}, {0.3f, -0.5f, 0.01f}});
    volchara::Plane obj2 = renderer.objPlaneFromWorldCoordinates({{0.0f, 0.2f, 0.0f}, {0.5f, 0.2f, 0.0f}, {0.5f, -0.4f, 0.0f}});
    volchara::Plane obj3 = renderer.objPlaneFromWorldCoordinates({{0.3f, 0.8f, -0.3f}, {0.5f, 0.8f, -0.3f}, {0.5f, -0.1f, -0.3f}});
    obj2.loadTexture(renderer.getResourceDir() / "textures/nouwu.jpg");
    obj3.loadTexture(renderer.getResourceDir() / "textures/ovca.png");
    obj3.frameCallbacks.push_back(mvmnt);
    renderer.addObject(&obj1);
    renderer.addObject(&obj2);
    renderer.addObject(&obj3);
    renderer.run();
    return 0;
}