#include "SystemMonitor.h"
#include <Windows.h>
#include <iostream>
#include <Pdh.h>

#pragma comment(lib, "pdh.lib")
#pragma warning(disable : 4996)


void SystemMonitor::InitMemoryEx() {
	memory.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memory);
}

void SystemMonitor::InitPDH() {
	PdhOpenQuery(nullptr, 0, &handle);
	PdhAddCounter(handle, L"\\Processor(_Total)\\% Processor Time", 0, &counter);
	PdhCollectQueryData(handle);
}

void SystemMonitor::InitNVML() {
	nvmlInit();
	nvmlDeviceGetCount(&deviceCount);
	nvmlDeviceGetHandleByIndex(0, &nvmlDevice);
}

float SystemMonitor::GetCPUUsagePercentage() {
	PdhCollectQueryData(handle);
	PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, 0, &cpuResult);
	return (float)cpuResult.doubleValue;
}

float SystemMonitor::GetGPUUsagePercentage() {
	nvmlReturn_t result = nvmlDeviceGetUtilizationRates(nvmlDevice, &utilStruct);
	if (result != NVML_SUCCESS) return -1.0f;
	return utilStruct.gpu;
}

float SystemMonitor::GetUsedVRAM() {
	if (nvmlDeviceGetMemoryInfo(nvmlDevice, &memInfo) != NVML_SUCCESS)
		return -1.0f;

	return memInfo.used / (1024.0f * 1024.0f * 1024.0f);
}

float SystemMonitor::GetTotalVRAM() {
	nvmlReturn_t result = nvmlDeviceGetMemoryInfo(nvmlDevice, &memInfo);
	if (result != NVML_SUCCESS) return -1.0f;
	return static_cast<float>(memInfo.total) / (1024.0 * 1024.0 * 1024.0);
}

unsigned int SystemMonitor::GetGPUTemp() {
	unsigned int temp;
	nvmlReturn_t result = nvmlDeviceGetTemperature(nvmlDevice, NVML_TEMPERATURE_GPU, &temp);
	if (result != NVML_SUCCESS) return 0;
	return temp;
}

std::string SystemMonitor::GetGPUModelName() {
	char name[NVML_DEVICE_NAME_BUFFER_SIZE];
	nvmlReturn_t result = nvmlDeviceGetName(nvmlDevice, name, NVML_DEVICE_NAME_BUFFER_SIZE);
	return std::string(name);
}

float SystemMonitor::GetRAMUsagePercentage() {
	InitMemoryEx();
	float percentage = ((float)(memory.ullTotalPhys - memory.ullAvailPhys) / memory.ullTotalPhys) * 100;
	return percentage;
}

float SystemMonitor::GetRAMUsageGB() {
	InitMemoryEx();
	float gb = (memory.ullTotalPhys - memory.ullAvailPhys) / (1024.0f * 1024.0f * 1024.0f);
	return gb;
}

float SystemMonitor::GetFreeRAMGB() {
	InitMemoryEx();
	float gb = memory.ullTotalPhys / (1024.0f * 1024.0f * 1024.0f);
	return gb;
}
