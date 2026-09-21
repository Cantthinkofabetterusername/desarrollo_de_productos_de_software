#include "imgui_opengl21_renderer.h"

#include <OpenGL/gl.h>
#include <cstdint>

static GLuint g_FontTexture = 0;

bool ImGui_ImplOpenGL21_Init() {
    ImGuiIO& io = ImGui::GetIO();

    unsigned char* pixels;
    int width;
    int height;

    io.Fonts->GetTexDataAsRGBA32(
        &pixels,
        &width,
        &height
    );

    glGenTextures(1, &g_FontTexture);
    glBindTexture(GL_TEXTURE_2D, g_FontTexture);

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels
    );

    io.Fonts->SetTexID(
        static_cast<ImTextureID>(g_FontTexture)
    );

    return true;
}

void ImGui_ImplOpenGL21_Shutdown() {
    if (g_FontTexture != 0) {
        glDeleteTextures(1, &g_FontTexture);
        g_FontTexture = 0;
    }

    ImGui::GetIO().Fonts->SetTexID(0);
}

void ImGui_ImplOpenGL21_NewFrame() {}

void ImGui_ImplOpenGL21_RenderDrawData(ImDrawData* drawData) {
    if (drawData == nullptr) {
        return;
    }

    int framebufferWidth =
        static_cast<int>(
            drawData->DisplaySize.x *
            drawData->FramebufferScale.x
        );

    int framebufferHeight =
        static_cast<int>(
            drawData->DisplaySize.y *
            drawData->FramebufferScale.y
        );

    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    glPushAttrib(
        GL_ENABLE_BIT |
        GL_COLOR_BUFFER_BIT |
        GL_SCISSOR_BIT |
        GL_TEXTURE_BIT |
        GL_TRANSFORM_BIT |
        GL_VIEWPORT_BIT
    );

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    glEnable(GL_TEXTURE_2D);

    glViewport(
        0,
        0,
        framebufferWidth,
        framebufferHeight
    );

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glOrtho(
        drawData->DisplayPos.x,
        drawData->DisplayPos.x + drawData->DisplaySize.x,
        drawData->DisplayPos.y + drawData->DisplaySize.y,
        drawData->DisplayPos.y,
        -1.0,
        1.0
    );

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    for (int commandListIndex = 0;
         commandListIndex < drawData->CmdListsCount;
         commandListIndex++) {

        const ImDrawList* commandList =
            drawData->CmdLists[commandListIndex];

        const ImDrawVert* vertices =
            commandList->VtxBuffer.Data;

        const ImDrawIdx* indices =
            commandList->IdxBuffer.Data;

        for (int commandIndex = 0;
             commandIndex < commandList->CmdBuffer.Size;
             commandIndex++) {

            const ImDrawCmd& command =
                commandList->CmdBuffer[commandIndex];

            if (command.UserCallback != nullptr) {
                command.UserCallback(
                    commandList,
                    &command
                );

                continue;
            }

            ImVec4 clipRect = command.ClipRect;

            float clipX =
                (clipRect.x - drawData->DisplayPos.x)
                * drawData->FramebufferScale.x;

            float clipY =
                (clipRect.y - drawData->DisplayPos.y)
                * drawData->FramebufferScale.y;

            float clipWidth =
                (clipRect.z - clipRect.x)
                * drawData->FramebufferScale.x;

            float clipHeight =
                (clipRect.w - clipRect.y)
                * drawData->FramebufferScale.y;

            if (clipWidth <= 0.0f ||
                clipHeight <= 0.0f) {
                continue;
            }

            glScissor(
                static_cast<GLint>(clipX),
                static_cast<GLint>(
                    framebufferHeight -
                    clipY -
                    clipHeight
                ),
                static_cast<GLsizei>(clipWidth),
                static_cast<GLsizei>(clipHeight)
            );

            GLuint textureID =
                static_cast<GLuint>(command.GetTexID());

            glBindTexture(
                GL_TEXTURE_2D,
                textureID
            );

            glBegin(GL_TRIANGLES);

            for (unsigned int index = 0;
                 index < command.ElemCount;
                 index++) {

                ImDrawIdx elementIndex =
                    indices[command.IdxOffset + index];

                const ImDrawVert& vertex =
                    vertices[
                        command.VtxOffset + elementIndex
                    ];

                unsigned int color =
                    vertex.col;

                unsigned char red =
                    static_cast<unsigned char>(
                        (color >> 0) & 0xFF
                    );

                unsigned char green =
                    static_cast<unsigned char>(
                        (color >> 8) & 0xFF
                    );

                unsigned char blue =
                    static_cast<unsigned char>(
                        (color >> 16) & 0xFF
                    );

                unsigned char alpha =
                    static_cast<unsigned char>(
                        (color >> 24) & 0xFF
                    );

                glColor4ub(
                    red,
                    green,
                    blue,
                    alpha
                );

                glTexCoord2f(
                    vertex.uv.x,
                    vertex.uv.y
                );

                glVertex2f(
                    vertex.pos.x,
                    vertex.pos.y
                );
            }

            glEnd();
        }
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopAttrib();
}