#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "SystemMonitor.h"
#include "implot.h"
#include "StoragePage.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>


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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Overlay", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // styling
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8, 6);
    style.FramePadding = ImVec2(6, 4);
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_TableRowBg] = ImVec4(1, 1, 1, 1);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.96f, 0.96f, 0.96f, 1);
    io.Fonts->AddFontFromFileTTF("fonts/font.ttf", 16.0f);

    SystemMonitor monitor;
    StoragePage storage;
    monitor.InitPDH();
    monitor.InitNVML();
    monitor.InitCOM();

    std::string gpuName = monitor.GetGPUModelName();
    std::string cpuName = monitor.GetWMIInfo("Win32_Processor", "Name");

    std::string cpuCores = monitor.GetWMIInfo("Win32_Processor", "NumberOfCores");
    std::string cpuThreads = monitor.GetWMIInfo("Win32_Processor", "NumberOfLogicalProcessors");
    std::string cpuSpeed = monitor.GetWMIInfo("Win32_Processor", "MaxClockSpeed");
       
    std::string memorySpeed = monitor.GetWMIInfo("Win32_PhysicalMemory", "Speed");
    std::string memoryType = monitor.GetWMIInfo("Win32_PhysicalMemory", "SMBIOSMemoryType");

    std::vector<std::string> driveLetters = monitor.GetWMIValues("Win32_LogicalDisk", "DeviceID");
    std::vector<std::string> freeSpaces = monitor.GetWMIValues("Win32_LogicalDisk", "FreeSpace");
    std::vector<std::string> sizes = monitor.GetWMIValues("Win32_LogicalDisk", "Size");
    std::vector<std::string> interfaceTypes = monitor.GetWMIValues("Win32_DiskDrive", "InterfaceType");
    std::vector<std::string> physicalSizes = monitor.GetWMIValues("Win32_DiskDrive", "Size");

    std::vector<Process> processes = monitor.GetWMIProcesses();

    std::vector<driveStruct> drives;

    for (int i = 0; i < driveLetters.size(); i++) {
        driveStruct d;
        d.driveLetter = driveLetters[i];

        if (i < freeSpaces.size())
            d.freeSpace = freeSpaces[i];

        if (i < sizes.size())
            d.size = sizes[i];

        if (i < interfaceTypes.size())
            d.interfaceType = interfaceTypes[i];

        drives.push_back(d);
    }

    int type = std::stoi(memoryType);
    switch (type)
    {
        case 26: type = 4; break; // DDR4
        case 34: type = 5; break; // DDR5
        case 24: type = 3; break; // DDR3
        default: type = 0; break;
    }

    float lastCpu = 0.0f;
    std::vector<float> CpuUtilReadings;
    auto lastCpuUpdate = std::chrono::steady_clock::now();

    float lastGpu = 0.0f;
    std::vector<float> GpuUtilReadings;
    auto lastGpuUpdate = std::chrono::steady_clock::now();

    float lastMemory = 0.0f;
    std::vector<float> MemoryUtilReadings;
    auto lastMemoryUpdate = std::chrono::steady_clock::now();

    static int currentDriveSelection = 0;

    //main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 500), ImVec2(FLT_MAX, FLT_MAX));

        ImGui::Begin("System Monitor", nullptr);

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
        auto elapsedMemory = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMemoryUpdate).count();

        if (elapsedCpu >= 500) {
            lastCpu = monitor.GetCPUUsagePercentage();
            CpuUtilReadings.push_back(lastCpu);

            if (CpuUtilReadings.size() > 50)
                CpuUtilReadings.erase(CpuUtilReadings.begin());

            lastCpuUpdate = now;
        }
        if (elapsedGpu >= 500) {
            lastGpu = monitor.GetGPUUsagePercentage();
            GpuUtilReadings.push_back(lastGpu);

            if (GpuUtilReadings.size() > 50)
                GpuUtilReadings.erase(GpuUtilReadings.begin());

            lastGpuUpdate = now;
        }
        if (elapsedMemory >= 500) {
            lastMemory = monitor.GetRAMUsagePercentage();
            MemoryUtilReadings.push_back(lastMemory);

            if (MemoryUtilReadings.size() > 50)
                MemoryUtilReadings.erase(MemoryUtilReadings.begin());

            lastMemoryUpdate = now;
        }

        static bool showSuccessPopup = false;
        static bool showFailedPopup = false;

        // ui
        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Performance")) {
                if (ImGui::BeginTabBar("PerformanceTabs")) {
                    if (ImGui::BeginTabItem("GPU")) {
                        ImGui::Text("%s", gpuName.c_str());
                        ImGui::Text("Utilization: %.1f%%", lastGpu);

                        if (ImPlot::BeginPlot("GPU Usage", ImVec2(-1, 0), ImPlotFlags_NoInputs | ImPlotFlags_NoMouseText)) {
                            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100);

                            ImPlot::SetupAxes("", "Utilization %");
                            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);

                            int count = GpuUtilReadings.size();
                            double xMax = count > 0 ? count - 1 : 0;
                            double xMin = count > 60 ? count - 60 : 0;
                            ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImGuiCond_Always);

                            ImPlot::PlotShaded("", GpuUtilReadings.data(), GpuUtilReadings.size());
                            ImPlot::EndPlot();
                        }
                        
                        ImGui::Text("VRAM Usage: %.2f GB / %.2f GB", monitor.GetUsedVRAM(), monitor.GetTotalVRAM());
                        ImGui::Text(u8"GPU Temp: %u°C", monitor.GetGPUTemp());
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("CPU")) {
                        ImGui::Text("%s", cpuName.c_str());

                        ImGui::Text("Utilization: %.1f%%", lastCpu);

                        if (ImPlot::BeginPlot("CPU Usage", ImVec2(-1, 0), ImPlotFlags_NoInputs | ImPlotFlags_NoMouseText)) {
                            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100);

                            ImPlot::SetupAxes("", "Utilization %");
                            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);

                            int count = CpuUtilReadings.size();
                            double xMax = count > 0 ? count - 1 : 0;
                            double xMin = count > 60 ? count - 60 : 0;
                            ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImGuiCond_Always);

                            ImPlot::PlotShaded("", CpuUtilReadings.data(), CpuUtilReadings.size());
                            ImPlot::EndPlot();
                        }

                        ImGui::Text("Cores: %d", std::stoi(cpuCores));
                        ImGui::Text("Threads: %d", std::stoi(cpuThreads));
                        ImGui::Text("Base Speed: %d MHz", std::stoi(cpuSpeed));
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Memory")) {
                        ImGui::Text("Memory Usage: %.1f%%", lastMemory);

                        if (ImPlot::BeginPlot("Memory Usage", ImVec2(-1, 0), ImPlotFlags_NoInputs | ImPlotFlags_NoMouseText)) {
                            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100);

                            ImPlot::SetupAxes("", "Utilization %");
                            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);

                            int count = MemoryUtilReadings.size();
                            double xMax = count > 0 ? count - 1 : 0;
                            double xMin = count > 60 ? count - 60 : 0;
                            ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImGuiCond_Always);

                            ImPlot::PlotShaded("", MemoryUtilReadings.data(), MemoryUtilReadings.size());
                            ImPlot::EndPlot();
                        }

                        ImGui::Text("Memory Usage GB: %.2fGB / %.2fGB", monitor.GetRAMUsageGB(), monitor.GetFreeRAMGB());
                        ImGui::Text("Speed: %d MHz", std::stoi(memorySpeed));
                        ImGui::Text("Type: DDR%d", type);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Storage")) {// storage cleanup, could maybe use code from my other project
                        if (ImGui::BeginCombo("Drives", drives[currentDriveSelection].driveLetter.c_str())) {

                            for (int i = 0; i < drives.size(); i++) {
                                bool selected = (currentDriveSelection == i);

                                if (ImGui::Selectable(drives[i].driveLetter.c_str(), selected))
                                    currentDriveSelection = i;

                                if (selected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        driveStruct drive = drives[currentDriveSelection];

                        long long freeSpace = std::stoll(drive.freeSpace) / (1024 * 1024 * 1024);
                        long long driveSize = std::stoll(drive.size) / (1024 * 1024 * 1024);
                        long long gbUsed = driveSize - freeSpace;

                        ImGui::Text("Space: %lldGB / %lldGB", gbUsed, driveSize);

                        std::filesystem::path driveSelected = drive.driveLetter + '\\';

                        if (ImGui::BeginTable("Storage", 2, ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable)) { // figure out how to sort size column
                            ImGui::TableSetupColumn("File/Folder", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending);
                            ImGui::TableHeadersRow();

                            storage.DrawDirectoryTree(driveSelected);

                            ImGui::EndTable();
                        }
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Bin")) {
                        if (ImGui::BeginTable("BinTable", 2)) {
                            ImGui::TableSetupColumn("File/Folder", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
                            ImGui::TableHeadersRow();

                            storage.ShowBinnedItems();
                            ImGui::EndTable();
                        }
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabItem();
                    ImGui::EndTabBar();
                }
            }
            if (ImGui::BeginTabItem("Processes")) {
                std::sort(processes.begin(), processes.end(),[](const Process& a, const Process& b) {return a.memoryUse > b.memoryUse;});
                if (ImGui::BeginTable("ProcessTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Command Line", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Creation Date", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableHeadersRow();

                    for (auto it = processes.begin(); it != processes.end(); ) {
                        Process& p = *it;
                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::Text("%u", p.pid);

                        std::string uniqueID = p.name + "##" + std::to_string(p.pid);
                        ImGui::TableNextColumn();
                        ImGui::Selectable(uniqueID.c_str());

                        bool shouldRemove = false;
                        if (ImGui::BeginPopupContextItem()) {
                            if (ImGui::MenuItem("End Task")) {
                                if (monitor.terminateProcessByPID({ p.pid })) {
                                    showSuccessPopup = true;
                                    shouldRemove = true;
                                }   
                                else
                                    showFailedPopup = true;
                            }
                            ImGui::EndPopup();
                        }

                        if (shouldRemove)
                            it = processes.erase(it);
                        else
                            it++;

                        ImGui::TableNextColumn();

                        if (p.memoryUse > 1ull * 1024 * 1024 * 1024)
                            ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", storage.FormatSize(p.memoryUse));
                        else if (p.memoryUse > 350ull * 1024 * 1024)
                            ImGui::TextColored(ImVec4(1, 0.647f, 0, 1), "%s", storage.FormatSize(p.memoryUse));
                        else
                            ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", storage.FormatSize(p.memoryUse));

                        std::string cmdLineID = p.commandLine.empty() ? ("##cmdline" + std::to_string(p.pid)) : p.commandLine;
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(cmdLineID.c_str());

                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(monitor.ParseWMIDate(p.creationDate).c_str());
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();       
        }

        // deleting files
        if (storage.openFailedPopup) {
            ImGui::OpenPopup("Failed");
            storage.openFailedPopup = false;
        }
        if (storage.openSuccessPopup) {
            ImGui::OpenPopup("Success");
            storage.openSuccessPopup = false;
        }

        if (ImGui::BeginPopupModal("Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Successully deleted with some failures.");
            for (auto item : storage.failedDeletes) {
                ImGui::Text("Failed to delete item: %s", item.string().c_str());
            }

            if (ImGui::Button("Confirm")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            storage.binnedItems.clear();
        }
        if (ImGui::BeginPopupModal("Success")) {
            ImGui::Text("Successully deleted all binned items.");

            if (ImGui::Button("Confirm")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            storage.binnedItems.clear();
        }

        // terminate process
        if (showSuccessPopup) {
            ImGui::OpenPopup("TerminateSuccess");
            showSuccessPopup = false;
        }
        if (showFailedPopup) {
            ImGui::OpenPopup("TerminateFailed");
            showFailedPopup = false;
        }

        if (ImGui::BeginPopupModal("TerminateSuccess", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Successfully terminated process.");
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("TerminateFailed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Failed to terminate process. Did you run the program as administrator?");
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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
    ImPlot::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    nvmlShutdown();

    return 0;
}