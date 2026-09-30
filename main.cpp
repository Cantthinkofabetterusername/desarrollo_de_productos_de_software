#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "imgui_opengl21_renderer.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct Character {
    GLuint textureID;
    int width;
    int height;
    int bearingX;
    int bearingY;
    unsigned int advance;
};

std::map<char, Character> Characters;

struct Vec3 {
    float x, y, z;
};

struct Vertex {
    Vec3 pos;
    Vec3 norm;
    float u, v;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    GLuint textureID = 0;
    bool hasTexture = false;
    Vec3 diffuseColor = { 0.8f, 0.8f, 0.8f };
};

struct Edge {
    unsigned int u, v;
    bool operator<(const Edge& o) const {
        if (u != o.u) return u < o.u;
        return v < o.v;
    }
};

std::vector<MeshData> loadedMeshes;
std::vector<Vec3> loadedVertices;
std::vector<Vec3> loadedNormals;
std::vector<unsigned int> loadedIndices;
Vec3 modelCenter = { 0.0f, 0.0f, 0.0f };
float modelScale = 1.0f;
unsigned int totalVertexCount = 0;
unsigned int totalEdgeCount = 0;
float modelMemoryKB = 0.0f;
bool modelLoaded = false;

bool isDirectory(const std::string& path) {
    struct stat s;
    if (stat(path.c_str(), &s) == 0) {
        return (s.st_mode & S_IFDIR) != 0;
    }
    return false;
}

std::string findModelInPath(const std::string& path) {
    if (!isDirectory(path)) return path;

    std::vector<std::string> extensions = { ".gltf", ".glb", ".obj", ".fbx", ".dae", ".blend", ".stl", ".ply" };
    DIR* dir = opendir(path.c_str());
    if (!dir) return path;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string fullPath = path + "/" + name;
        if (!isDirectory(fullPath)) {
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            for (const auto& ext : extensions) {
                if (lowerName.size() >= ext.size() &&
                    lowerName.compare(lowerName.size() - ext.size(), ext.size(), ext) == 0) {
                    closedir(dir);
                    return fullPath;
                }
            }
        }
    }
    closedir(dir);
    return path;
}

std::string openMacFileDialog() {
    std::string result = "";
    const char* script = "osascript -e 'POSIX path of (choose folder with prompt \"Select the folder that contains the 3D model.\")' 2>/dev/null";

    FILE* pipe = popen(script, "r");
    if (pipe) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
    }
    return result;
}

GLuint loadTexture(const std::string& baseDir, const std::string& texPath, const aiScene* scene) {
    if (texPath.empty()) return 0;

    int width = 0, height = 0, channels = 0;
    unsigned char* data = nullptr;

    if (texPath[0] == '*') {
        int texIdx = std::atoi(texPath.c_str() + 1);
        if (texIdx >= 0 && texIdx < static_cast<int>(scene->mNumTextures)) {
            aiTexture* tex = scene->mTextures[texIdx];
            if (tex->mHeight == 0) {
                data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(tex->pcData), tex->mWidth, &width, &height, &channels, 4);
            } else {
                data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(tex->pcData), tex->mWidth * tex->mHeight * sizeof(aiTexel), &width, &height, &channels, 4);
            }
        }
    } else {
        std::string cleanPath = texPath;
        std::replace(cleanPath.begin(), cleanPath.end(), '\\', '/');

        std::string filename = cleanPath;
        size_t lastSlash = cleanPath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            filename = cleanPath.substr(lastSlash + 1);
        }

        std::vector<std::string> candidates = {
            baseDir + "/" + cleanPath,
            baseDir + "/textures/" + filename,
            baseDir + "/" + filename,
            cleanPath
        };

        for (const auto& candidate : candidates) {
            data = stbi_load(candidate.c_str(), &width, &height, &channels, 4);
            if (data) break;
        }
    }

    if (!data) return 0;

    GLuint texID = 0;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(data);

    return texID;
}

void clearLoadedModel() {
    for (auto& mesh : loadedMeshes) {
        if (mesh.hasTexture && mesh.textureID != 0) {
            glDeleteTextures(1, &mesh.textureID);
        }
    }
    loadedMeshes.clear();
    loadedVertices.clear();
    loadedNormals.clear();
    loadedIndices.clear();
    totalVertexCount = 0;
    totalEdgeCount = 0;
    modelLoaded = false;
}

bool loadModelFile(const std::string& inputPath) {
    std::string actualPath = findModelInPath(inputPath);
    if (actualPath.empty()) return false;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(actualPath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs |
        aiProcess_PreTransformVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        return false;
    }

    clearLoadedModel();

    std::string baseDir = "";
    size_t lastSlash = actualPath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseDir = actualPath.substr(0, lastSlash);
    } else {
        baseDir = ".";
    }

    std::vector<GLuint> materialTextures(scene->mNumMaterials, 0);
    std::vector<Vec3> materialColors(scene->mNumMaterials, { 0.8f, 0.8f, 0.8f });

    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* mat = scene->mMaterials[i];

        aiColor3D color(0.8f, 0.8f, 0.8f);
        if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            materialColors[i] = { color.r, color.g, color.b };
        }

        aiString texPath;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS ||
            mat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
            GLuint texID = loadTexture(baseDir, texPath.C_Str(), scene);
            materialTextures[i] = texID;
        }
    }

    std::set<Edge> uniqueEdges;
    Vec3 minPos = { 1e9f, 1e9f, 1e9f };
    Vec3 maxPos = { -1e9f, -1e9f, -1e9f };

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];
        MeshData meshData;

        if (mesh->mMaterialIndex < scene->mNumMaterials) {
            meshData.diffuseColor = materialColors[mesh->mMaterialIndex];
            meshData.textureID = materialTextures[mesh->mMaterialIndex];
            meshData.hasTexture = (meshData.textureID != 0);
        }

        unsigned int vertexOffset = static_cast<unsigned int>(loadedVertices.size());
        totalVertexCount += mesh->mNumVertices;

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vec3 pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            Vec3 norm = { 0.0f, 1.0f, 0.0f };
            if (mesh->HasNormals()) {
                norm = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            }

            float u = 0.0f, v = 0.0f;
            if (mesh->HasTextureCoords(0)) {
                u = mesh->mTextureCoords[0][i].x;
                v = mesh->mTextureCoords[0][i].y;
            }

            Vertex virt = { pos, norm, u, v };
            meshData.vertices.push_back(virt);

            if (pos.x < minPos.x) minPos.x = pos.x;
            if (pos.y < minPos.y) minPos.y = pos.y;
            if (pos.z < minPos.z) minPos.z = pos.z;
            if (pos.x > maxPos.x) maxPos.x = pos.x;
            if (pos.y > maxPos.y) maxPos.y = pos.y;
            if (pos.z > maxPos.z) maxPos.z = pos.z;

            loadedVertices.push_back(pos);
            loadedNormals.push_back(norm);
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            aiFace face = mesh->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                meshData.indices.push_back(face.mIndices[j]);
                loadedIndices.push_back(vertexOffset + face.mIndices[j]);
            }
            if (face.mNumIndices >= 3) {
                for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                    unsigned int idx1 = vertexOffset + face.mIndices[j];
                    unsigned int idx2 = vertexOffset + face.mIndices[(j + 1) % face.mNumIndices];
                    Edge e = { std::min(idx1, idx2), std::max(idx1, idx2) };
                    uniqueEdges.insert(e);
                }
            }
        }
        loadedMeshes.push_back(meshData);
    }

    totalEdgeCount = static_cast<unsigned int>(uniqueEdges.size());
    modelCenter.x = (minPos.x + maxPos.x) * 0.5f;
    modelCenter.y = (minPos.y + maxPos.y) * 0.5f;
    modelCenter.z = (minPos.z + maxPos.z) * 0.5f;

    float dx = maxPos.x - minPos.x;
    float dy = maxPos.y - minPos.y;
    float dz = maxPos.z - minPos.z;
    float maxDim = std::max(dx, std::max(dy, dz));
    if (maxDim > 0.00001f) {
        modelScale = 3.0f / maxDim;
    } else {
        modelScale = 1.0f;
    }

    size_t memBytes = loadedVertices.size() * sizeof(Vec3) * 2 + loadedIndices.size() * sizeof(unsigned int);
    modelMemoryKB = static_cast<float>(memBytes) / 1024.0f;

    modelLoaded = true;
    return true;
}

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

    static char filePath[256] = "";
    bool isLoading = false;
    int loadingFrames = 0;
    float camRotX = 20.0f;
    float camRotY = 45.0f;
    float camZoom = 5.0f;
    float camPanX = 0.0f;
    float camPanY = 0.0f;
    ImVec2 previewMin(0, 0);
    ImVec2 previewSize(0, 0);

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

        if (isLoading) {
            loadingFrames++;
            if (loadingFrames > 2) {
                if (strlen(filePath) > 0) {
                    loadModelFile(filePath);
                }
                isLoading = false;
            }
        }

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
            ImGui::InputText("##Path", filePath, IM_ARRAYSIZE(filePath));
            if (ImGui::Button("Upload Asset")) {
                std::string selectedPath = openMacFileDialog();
                if (!selectedPath.empty()) {
                    strncpy(filePath, selectedPath.c_str(), sizeof(filePath) - 1);
                    filePath[sizeof(filePath) - 1] = '\0';
                    button1_pressed = true;
                    isLoading = true;
                    loadingFrames = 0;
                }
            }
            if (button1_pressed) {
                ImGui::Combo("##Method", &selectedMethod, methods, IM_ARRAYSIZE(methods));
                if (ImGui::Button("Optimize Model")) {
                    button1_pressed = false;
                    button2_pressed = true;
                    isLoading = true;
                    loadingFrames = 0;
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            ImGui::BeginChild("Main Area", ImVec2(0, 0));

            ImVec4 previewBg = modelLoaded ? ImVec4(0.0f, 0.0f, 0.0f, 1.0f) : ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
            ImVec4 previewText = modelLoaded ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, previewBg);
            ImGui::PushStyleColor(ImGuiCol_Text, previewText);
            ImGui::BeginChild("Preview", ImVec2(0, 700));
            previewMin = ImGui::GetWindowPos();
            previewSize = ImGui::GetWindowSize();
            if (ImGui::IsWindowHovered()) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    ImVec2 delta = ImGui::GetIO().MouseDelta;
                    camRotY += delta.x * 0.5f;
                    camRotX += delta.y * 0.5f;
                }
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                    ImVec2 delta = ImGui::GetIO().MouseDelta;
                    camPanX += delta.x * 0.005f * camZoom;
                    camPanY -= delta.y * 0.005f * camZoom;
                }
                if (ImGui::GetIO().MouseWheel != 0.0f) {
                    camZoom -= ImGui::GetIO().MouseWheel * 0.3f;
                    if (camZoom < 0.1f) camZoom = 0.1f;
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.8f, 0.1f, 1.0f));
            ImGui::BeginChild("Information", ImVec2(0, 0));
            ImGui::Text(" Asset Information");
            if (isLoading) {
                ImGui::Text(" Loading...");
            } else if (modelLoaded) {
                ImGui::Text(" Vertices: %u", totalVertexCount);
                ImGui::Text(" Edges: %u", totalEdgeCount);
                if (modelMemoryKB >= 1024.0f) {
                    ImGui::Text(" Memory Usage: %.2fMB", modelMemoryKB / 1024.0f);
                } else {
                    ImGui::Text(" Memory Usage: %.1fKB", modelMemoryKB);
                }
            } else if (button1_pressed) {
                ImGui::Text(" Invalid file! Try uploading a folder with a .glTF or .glb file instead.");
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

        if (t > 3.00f && alpha <= 0.00f && modelLoaded && previewSize.x > 0 && previewSize.y > 0) {
            int winW, winH;
            glfwGetWindowSize(window, &winW, &winH);
            float scaleX = static_cast<float>(width) / winW;
            float scaleY = static_cast<float>(height) / winH;

            GLint vpX = static_cast<GLint>(previewMin.x * scaleX);
            GLint vpY = static_cast<GLint>(height - (previewMin.y + previewSize.y) * scaleY);
            GLsizei vpW = static_cast<GLsizei>(previewSize.x * scaleX);
            GLsizei vpH = static_cast<GLsizei>(previewSize.y * scaleY);

            glEnable(GL_SCISSOR_TEST);
            glScissor(vpX, vpY, vpW, vpH);
            glViewport(vpX, vpY, vpW, vpH);

            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            float aspect = previewSize.x / (previewSize.y > 0.0f ? previewSize.y : 1.0f);
            float fov = 45.0f;
            float halfFov = tanf((fov * 0.5f) * 3.14159265f / 180.0f);
            float nearZ = 0.1f;
            float farZ = 100.0f;
            float top = nearZ * halfFov;
            float right = top * aspect;
            glFrustum(-right, right, -top, top, nearZ, farZ);

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            glTranslatef(camPanX, camPanY, -camZoom);
            glRotatef(camRotX, 1.0f, 0.0f, 0.0f);
            glRotatef(camRotY, 0.0f, 1.0f, 0.0f);
            glScalef(modelScale, modelScale, modelScale);
            glTranslatef(-modelCenter.x, -modelCenter.y, -modelCenter.z);

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);

            GLfloat lightPos[] = { 1.0f, 1.0f, 2.0f, 0.0f };
            GLfloat lightDiffuse[] = { 0.9f, 0.9f, 0.9f, 1.0f };
            GLfloat lightAmbient[] = { 0.4f, 0.4f, 0.4f, 1.0f };
            glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
            glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);

            for (const auto& mesh : loadedMeshes) {
                if (mesh.hasTexture && mesh.textureID != 0) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, mesh.textureID);
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                } else {
                    glDisable(GL_TEXTURE_2D);
                    glEnable(GL_COLOR_MATERIAL);
                    glColor3f(mesh.diffuseColor.x, mesh.diffuseColor.y, mesh.diffuseColor.z);
                }

                glBegin(GL_TRIANGLES);
                for (unsigned int idx : mesh.indices) {
                    const auto& virt = mesh.vertices[idx];
                    if (mesh.hasTexture) {
                        glTexCoord2f(virt.u, virt.v);
                    }
                    glNormal3f(virt.norm.x, virt.norm.y, virt.norm.z);
                    glVertex3f(virt.pos.x, virt.pos.y, virt.pos.z);
                }
                glEnd();

                if (mesh.hasTexture) {
                    glDisable(GL_TEXTURE_2D);
                } else {
                    glDisable(GL_COLOR_MATERIAL);
                }
            }

            glDisable(GL_LIGHT0);
            glDisable(GL_LIGHTING);
            glDisable(GL_DEPTH_TEST);

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();

            glDisable(GL_SCISSOR_TEST);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    clearLoadedModel();

    for (auto const& pair : Characters) {
        glDeleteTextures(1, &pair.second.textureID);
    }

    ImGui_ImplOpenGL21_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    return 0;
}