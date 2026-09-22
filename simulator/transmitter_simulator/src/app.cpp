#include "app.hpp"
#include "ui.hpp"

#include <filesystem>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace transmitter_simulator {
int run_gui(Simulator& simulator) noexcept {
    if (glfwInit() == GLFW_FALSE) return 1;
    GLFWwindow* window = glfwCreateWindow(1280, 760, "WRS Transmitter Simulator", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    constexpr const char* k_ui_font = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    if (std::filesystem::exists(k_ui_font))
        ImGui::GetIO().Fonts->AddFontFromFileTTF(k_ui_font, 16.0F);
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(20.0F, 16.0F);
    style.FramePadding = ImVec2(10.0F, 7.0F);
    style.ItemSpacing = ImVec2(12.0F, 10.0F);
    style.WindowRounding = 0.0F;
    style.ChildRounding = 6.0F;
    style.FrameRounding = 4.0F;
    style.GrabRounding = 4.0F;
    style.TabRounding = 4.0F;
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.10F, 0.14F, 0.21F, 1.0F);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.38F, 0.45F, 0.55F, 1.0F);
    colors[ImGuiCol_WindowBg] = ImVec4(0.96F, 0.975F, 0.99F, 1.0F);
    colors[ImGuiCol_ChildBg] = ImVec4(1.0F, 1.0F, 1.0F, 1.0F);
    colors[ImGuiCol_Border] = ImVec4(0.80F, 0.84F, 0.89F, 1.0F);
    colors[ImGuiCol_FrameBg] = ImVec4(0.94F, 0.96F, 0.985F, 1.0F);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.87F, 0.93F, 1.0F, 1.0F);
    colors[ImGuiCol_Button] = ImVec4(0.13F, 0.40F, 0.76F, 1.0F);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.10F, 0.32F, 0.64F, 1.0F);
    colors[ImGuiCol_Header] = ImVec4(0.86F, 0.93F, 1.0F, 1.0F);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.77F, 0.88F, 1.0F, 1.0F);
    colors[ImGuiCol_Tab] = ImVec4(0.90F, 0.94F, 0.99F, 1.0F);
    colors[ImGuiCol_TabSelected] = ImVec4(0.76F, 0.87F, 1.0F, 1.0F);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw_ui(simulator);
        ImGui::Render();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.94F, 0.96F, 0.99F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
}  // namespace transmitter_simulator
