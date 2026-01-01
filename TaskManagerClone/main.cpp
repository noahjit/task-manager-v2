#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include "SystemMonitor.h"

static bool isDragging = false;
static double dragOffsetX, dragOffsetY;

int main() {
    if (!glfwInit())
        return -1;

    nvmlInit();

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(400, 500, "Overlay", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    SystemMonitor monitor;
    monitor.InitPDH();
    monitor.InitNVML();

    float last_cpu = 0.0f;
    auto last_update = std::chrono::steady_clock::now();

    //main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(400, 500));

        ImGui::Begin("System Monitor", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            int winX, winY;
            glfwGetWindowPos(window, &winX, &winY);
            dragOffsetX = mouseX;
            dragOffsetY = mouseY;
            isDragging = true;
        }

        if (ImGui::IsMouseReleased(0)) {
            isDragging = false;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();

        if (elapsed >= 500) {
            last_cpu = monitor.GetCPUUsagePercentage();
            last_update = now;
        }

        // ui
        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Performance")) {
                ImGui::Text("Useages");
                ImGui::Separator();
                ImGui::Text("CPU Usage: %.1f%%", last_cpu);
                ImGui::Text("GPU Usage: %.1f%%", monitor.GetGPUUsagePercentage());
                ImGui::Text("VRAM Usage: %.1f%%", monitor.GetGPUUsageVRAM());
                ImGui::Text("Memory Usage: %.1f%%", monitor.GetRAMUsagePercentage());
                ImGui::Text("Memory Usage GB: %.2fGB / %.2fGB", monitor.GetRAMUsageGB(), monitor.GetFreeRAMGB());

                ImGui::Separator();
                ImGui::Text("Temperatures");
                ImGui::Separator();
                ImGui::Text("CPU Temp: %u C", 0);
                ImGui::Text("GPU Temp: %u C", monitor.GetGPUUsageTemp());
                ImGui::Separator();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Processes")) { // list of processes like task manager
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Storage")) { // storage cleanup, could maybe use code from my other project
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }


        ImGui::End();

        if (isDragging) {
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            int winX, winY;
            glfwGetWindowPos(window, &winX, &winY);

            int newX = winX + (int)(mouseX - dragOffsetX);
            int newY = winY + (int)(mouseY - dragOffsetY);

            glfwSetWindowPos(window, newX, newY);
        }

        // rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    nvmlShutdown();

    return 0;
}