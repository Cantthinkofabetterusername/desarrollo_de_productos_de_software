#include <iostream>
#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    };

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow* window = glfwCreateWindow(640, 480, "Shrinkigon", nullptr, nullptr);

    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        return -1;
    };

    glfwMakeContextCurrent(window);

    while (!glfwWindowShouldClose(window)) {
        glfwSwapBuffers(window);
        glfwPollEvents();
    };

    glfwTerminate();

    return 0;
}