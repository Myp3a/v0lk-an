#include <chrono>
#include <random>
#include <thread>

#include <renderer.hpp>

volchara::Renderer renderer{};
std::mt19937 rnd(std::chrono::time_point<std::chrono::system_clock>().time_since_epoch().count());
std::uniform_real_distribution<> dis(0.0f, 1.0f);
std::uniform_real_distribution<> bdis(0.05f, 0.25f);


void mvmnt(volchara::Object* obj, float passedSeconds, std::set<int> pressedKeys) {
    if (pressedKeys.contains(GLFW_KEY_UP)) obj->transform.position.up(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_DOWN)) obj->transform.position.down(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_LEFT)) obj->transform.position.left(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_RIGHT)) obj->transform.position.right(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_PERIOD)) obj->transform.position.forward(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_COMMA)) obj->transform.position.backward(passedSeconds, true);
    if (pressedKeys.contains(GLFW_KEY_I)) obj->transform.position.up(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_K)) obj->transform.position.down(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_J)) obj->transform.position.left(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_L)) obj->transform.position.right(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_M)) obj->transform.position.forward(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_N)) obj->transform.position.backward(passedSeconds, false);
    if (pressedKeys.contains(GLFW_KEY_1)) obj->transform.rotation.ccw(passedSeconds);
    if (pressedKeys.contains(GLFW_KEY_2)) obj->transform.rotation.cw(passedSeconds);
}

int main() {
    std::this_thread::sleep_for(std::chrono::seconds(2));  //screw xcode
    // volchara::Plane obj1 = renderer.objPlaneFromWorldCoordinates({{-1.3f, 0.5f, 0.01f}, {0.3f, 0.5f, 0.01f}, {0.3f, -0.5f, 0.01f}});
    volchara::Box obj1 = renderer.objBoxFromWorldCoordinates({{0.45f, 0.3f, -0.65f}, {1, 2, 1}, {{-0.3f, 0.5f, 0.01f}, {0.3f, 0.5f, 0.01f}, {0.3f, -0.5f, 0.01f}}});
    // volchara::Plane obj2 = renderer.objPlaneFromWorldCoordinates({{0.0f, 0.2f, 0.0f}, {0.5f, 0.2f, 0.0f}, {0.5f, -0.4f, 0.0f}});
    // volchara::Plane obj3 = renderer.objPlaneFromWorldCoordinates({{0.3f, 0.8f, -0.3f}, {0.5f, 0.8f, -0.3f}, {0.5f, -0.1f, -0.3f}});
    // obj2.loadTexture(renderer.getResourceDir() / "textures/nouwu.jpg");
    // obj3.loadTexture(renderer.getResourceDir() / "textures/ovca.png");
    // obj3.frameCallbacks.push_back(mvmnt);
    // renderer.addObject(&obj1);
    // renderer.addObject(&obj2);
    // renderer.addObject(&obj3);
    // volchara::GLTFModel obj1 = renderer.objGLTFModelFromFile(renderer.getResourceDir() / "models/fisch/fisch.gltf");
    obj1.frameCallbacks.push_back(mvmnt);
    renderer.addObject(&obj1);
    renderer.setAmbientLight({{}, {1.0f, 1.0f, 1.0f}, 0.01f});
    float r_offset = -0.05f;
    float b_offset = -0.7f;
    std::vector<volchara::DirectionalLight> lights{};
    for (float r = 0; r < 1; r += 0.1) {
        for (float g = 0; g < 2; g += 0.1) {
            volchara::DirectionalLight svit = renderer.objDirectionalLightFromWorldCoordinates({{r_offset + r, b_offset + g, -0.13f}, {static_cast<float>(dis(rnd)), static_cast<float>(dis(rnd)), static_cast<float>(dis(rnd))}, static_cast<float>(bdis(rnd))});
            lights.push_back(std::move(svit));
        }
    }
    for (int i = 0; i < lights.size(); i++) {
        renderer.addLight(&lights[i]);
    }
    
    renderer.run();
    return 0;
}