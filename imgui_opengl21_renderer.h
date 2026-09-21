#pragma once

#include "imgui.h"

bool ImGui_ImplOpenGL21_Init();
void ImGui_ImplOpenGL21_Shutdown();
void ImGui_ImplOpenGL21_NewFrame();
void ImGui_ImplOpenGL21_RenderDrawData(ImDrawData* drawData);

#ifndef SHRINKIGON_IMGUI_OPENGL21_RENDERER_H
#define SHRINKIGON_IMGUI_OPENGL21_RENDERER_H

#endif //SHRINKIGON_IMGUI_OPENGL21_RENDERER_H
