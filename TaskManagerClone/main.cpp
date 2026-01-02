#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include "SystemMonitor.h"
#include <iostream>
#include <string>

static bool isDragging = false;
static double dragOffsetX, dragOffsetY;

int main() {
    if (!glfwInit())
        return -1;

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
    monitor.InitCOM();

    std::string gpuName = monitor.GetGPUModelName();
    std::string cpuName = monitor.GetWMIInfo("Win32_Processor", "Name");

    std::string cpuCores = monitor.GetWMIInfo("Win32_Processor", "NumberOfCores");
    std::string cpuThreads = monitor.GetWMIInfo("Win32_Processor", "NumberOfLogicalProcessors");
    std::string cpuSpeed = monitor.GetWMIInfo("Win32_Processor", "MaxClockSpeed");
    std::string cpuUpTime = monitor.GetWMIInfo("Win32_Processor", "LastBootUpTime");
       
    std::string memorySpeed = monitor.GetWMIInfo("Win32_PhysicalMemory", "Speed");
    std::string memoryType = monitor.GetWMIInfo("Win32_PhysicalMemory", "SMBIOSMemoryType");
    int type = std::stoi(memoryType);
    switch (type)
    {
        case 26: type = 4; break; // DDR4
        case 34: type = 5; break; // DDR5
        case 24: type = 3; break; // DDR3
        default: type = 0; break;
    }

    float lastCpu = 0.0f;
    auto lastCpuUpdate = std::chrono::steady_clock::now();

    float lastGpu = 0.0f;
    auto lastGpuUpdate = std::chrono::steady_clock::now();

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
        auto elapsedCpu = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCpuUpdate).count();
        auto elapsedGpu = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastGpuUpdate).count();

        if (elapsedCpu >= 500) {
            lastCpu = monitor.GetCPUUsagePercentage();
            lastCpuUpdate = now;
        }
        if (elapsedGpu >= 500) {
            lastGpu = monitor.GetGPUUsagePercentage();
            lastGpuUpdate = now;
        }

        // ui
        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Performance")) {
                if (ImGui::BeginTabBar("PerformanceTabs")) {
                    if (ImGui::BeginTabItem("GPU")) {
                        ImGui::Text("%s", gpuName.c_str());
                        ImGui::Text("Utilization: %.1f%%", lastGpu);
                        ImGui::Text("VRAM Usage: %.2f GB / %.2f GB", monitor.GetUsedVRAM(), monitor.GetTotalVRAM());
                        ImGui::Text(u8"GPU Temp: %u°C", monitor.GetGPUTemp());
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("CPU")) {
                        ImGui::Text("%s", cpuName.c_str());
                        ImGui::Text("Cores: %d", std::stoi(cpuCores));
                        ImGui::Text("Threads: %d", std::stoi(cpuThreads));
                        ImGui::Text("Base Speed: %d MHz", std::stoi(cpuSpeed));
                        ImGui::Text("Utilization: %.1f%%", lastCpu);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Memory")) {
                        ImGui::Text("Memory Usage: %.1f%%", monitor.GetRAMUsagePercentage());
                        ImGui::Text("Memory Usage GB: %.2fGB / %.2fGB", monitor.GetRAMUsageGB(), monitor.GetFreeRAMGB());
                        ImGui::Text("Speed: %d MHz", std::stoi(memorySpeed));
                        ImGui::Text("Type: DDR%d", type);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Storage")) {// storage cleanup, could maybe use code from my other project
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabItem();
                    ImGui::EndTabBar();
                }
            }
            if (ImGui::BeginTabItem("Processes")) { // list of processes like task manager
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
    monitor.CleanupCOM();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    nvmlShutdown();

    return 0;
}