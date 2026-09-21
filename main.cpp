#include <iostream>
#include <map>
#include <string>
#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "imgui_opengl21_renderer.h"

struct Character {
    GLuint textureID;
    int width;
    int height;
    int bearingX;
    int bearingY;
    unsigned int advance;
};

std::map<char, Character> Characters;

void renderText(const std::string& text, float x, float y, float scale, float r, float g, float b, float a, int windowWidth, int windowHeight) {
    float screenX = (x + 1.0f) * 0.5f * windowWidth;
    float screenY = (y + 1.0f) * 0.5f * windowHeight;

    float totalWidth = 0.0f;
    for (char c : text) {
        if (Characters.find(c) != Characters.end()) {
            totalWidth += (Characters[c].advance >> 6) * scale;
        }
    }

    float startX = screenX - (totalWidth / 2.0f);
    float startY = screenY;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, a);

    for (char c : text) {
        if (Characters.find(c) == Characters.end()) continue;

        Character ch = Characters[c];

        float xpos = startX + ch.bearingX * scale;
        float ypos = startY - (ch.height - ch.bearingY) * scale;

        float w = ch.width * scale;
        float h = ch.height * scale;

        glBindTexture(GL_TEXTURE_2D, ch.textureID);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(xpos, ypos);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(xpos + w, ypos);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(xpos + w, ypos + h);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(xpos, ypos + h);
        glEnd();

        startX += (ch.advance >> 6) * scale;
    }

    glDisable(GL_TEXTURE_2D);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

float bezier_curve(float t, float point1, float point2, float point3, float point4) {
    return pow((1 - t), 3) * point1 + 3 * pow((1 - t), 2) * t * point2 + 3 * (1 - t) * pow(t, 2) * point3 + pow(t, 3) * point4;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow* window = glfwCreateWindow(
        1600,
        900,
        "Shrinkigon",
        nullptr,
        nullptr
    );

    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("fonts/Geo-Regular.ttf", 24.0f);
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL21_Init();

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Failed to initialize FreeType!" << std::endl;
        glfwTerminate();
        return -1;
    }

    FT_Face face;
    if (FT_New_Face(ft, "fonts/CaacupeOne-Regular.ttf", 0, &face)) {
        std::cerr << "Failed to load font!" << std::endl;
        FT_Done_FreeType(ft);
        glfwTerminate();
        return -1;
    }

    FT_Set_Pixel_Sizes(face, 0, 128);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            continue;
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_ALPHA,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_ALPHA,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            static_cast<int>(face->glyph->bitmap.width),
            static_cast<int>(face->glyph->bitmap.rows),
            face->glyph->bitmap_left,
            face->glyph->bitmap_top,
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    float t = 0.00f;
    float alpha = 1.00f;

    bool button1_pressed = false;
    bool button2_pressed = false;
    float loading = 3.0f;

    const char* methods[] = {
        "Select Optimization",
        "Vertex Removal",
        "Edge Collapse",
        "Quadric Error Metrics",
        "Vertex Clustering"
    };

    int selectedMethod = 0;

    ImGuiStyle& style = ImGui::GetStyle();
    style.ItemSpacing = ImVec2(8, 8);
    style.FramePadding = ImVec2(10, 6);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);

    while (!glfwWindowShouldClose(window)) {

        ImGui_ImplOpenGL21_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glClear(GL_COLOR_BUFFER_BIT);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, 0, height, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        if (t <= 1.00f) {
            float ypos = bezier_curve(t, -2.1f, -1.6f, 1.5f, 0.3f);
            renderText("Welcome", 0.0f, ypos, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, width, height);
            t += 0.01f;
        } else if (t > 1.00f && t <= 2.00f) {
            float ypos = bezier_curve(t - 1, -2.1f, -1.6f, 1.1f, -0.1f);
            renderText("Welcome", 0.0f, 0.3f, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, width, height);
            renderText("to", 0.0f, ypos, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, width, height);
            t += 0.01f;
        } else if (t > 2.00f && t <= 3.00f) {
            float ypos = bezier_curve(t - 2, -2.1f, -1.6f, 0.7f, -0.5f);
            renderText("Welcome", 0.0f, 0.3f, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, width, height);
            renderText("to", 0.0f, -0.1f, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, width, height);
            renderText("Shrinkigon!", 0.0f, ypos, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, width, height);
            t += 0.01f;
        } else if (t > 3.00f && alpha >= 0.00f) {
            renderText("Welcome", 0.0f, 0.3f, 2.0f, 0.0f, 0.0f, 0.0f, alpha, width, height);
            renderText("to", 0.0f, -0.1f, 2.0f, 0.0f, 0.0f, 0.0f, alpha, width, height);
            renderText("Shrinkigon!", 0.0f, -0.5f, 2.0f, 0.0f, 0.0f, 0.0f, alpha, width, height);
            alpha -= 0.005f;
        } else {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Shrinkigon", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::BeginChild("Sidebar", ImVec2(250, 0));
            ImGui::Text("Current Asset");
            ImGui::Separator();
            if (ImGui::Button("Upload Asset")) {
                button1_pressed = !button1_pressed;
            }
            if (button1_pressed) {
                ImGui::Combo("##Method", &selectedMethod, methods, IM_ARRAYSIZE(methods));
                if (ImGui::Button("Optimize Model")) {
                    button1_pressed = false;
                    button2_pressed = true;
                    loading = 0.0f;
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            ImGui::BeginChild("Main Area", ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::BeginChild("Preview", ImVec2(0, 700));
            if (loading < 3.0f) {
                ImGui::Text(" Loading...");
                loading += 0.01f;
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.8f, 0.1f, 1.0f));
            ImGui::BeginChild("Information", ImVec2(0, 0));
            ImGui::Text(" Asset Information");
            if (button1_pressed) {
                ImGui::Text(" Vertices: 678");
                ImGui::Text(" Edges: 514");
                ImGui::Text(" Memory Usage: 5.7KB");
            } else if (button2_pressed && loading >= 3.0f) {
                ImGui::Text(" Vertices: 429");
                ImGui::Text(" Edges: 374");
                ImGui::Text(" Memory Usage: 3.2KB");
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::EndChild();
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL21_RenderDrawData(
            ImGui::GetDrawData()
        );

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (auto const& pair : Characters) {
        glDeleteTextures(1, &pair.second.textureID);
    }

    ImGui_ImplOpenGL21_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    return 0;
}