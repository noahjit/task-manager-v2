#include "SystemMonitor.h"
#include <Windows.h>
#include <iostream>
#include <Pdh.h>

#pragma comment(lib, "pdh.lib")

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
	nvmlDeviceGetHandleByIndex(0, &nvmlDevice);
	nvmlDeviceGetCount(&deviceCount);
}

float SystemMonitor::GetCPUUsagePercentage() {
	PdhCollectQueryData(handle);
	PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, 0, &cpuResult);
	return (float)cpuResult.doubleValue;
}

float SystemMonitor::GetGPUUsagePercentage() {
	nvmlDeviceGetUtilizationRates(nvmlDevice, &utilStruct);
	return utilStruct.gpu;
}

float SystemMonitor::GetGPUUsageVRAM() {
	nvmlDeviceGetUtilizationRates(nvmlDevice, &utilStruct);
	return utilStruct.memory;
}

unsigned int SystemMonitor::GetGPUUsageTemp() {
	unsigned int temp = 0;
	nvmlDeviceGetTemperatureV(nvmlDevice, &gpuStruct);
	return temp;
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
