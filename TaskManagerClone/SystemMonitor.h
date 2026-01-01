#pragma once

#include <Windows.h>
#include <Pdh.h>
#include <nvml.h>
#include <iostream>

class SystemMonitor
{
public:
	// cpu
	float GetCPUUsagePercentage();

	// gpu
	float GetGPUUsagePercentage();
	unsigned int GetGPUTemp();
	std::string GetGPUModelName();
	
	// initialize
	void InitPDH();
	void InitNVML();

	// memory
	float GetUsedVRAM();
	float GetRAMUsagePercentage();
	float GetRAMUsageGB();
	float GetTotalVRAM();
	float GetFreeRAMGB();
private:
	void InitMemoryEx();

	MEMORYSTATUSEX memory;
	PDH_HQUERY handle;
	PDH_HCOUNTER counter;
	PDH_FMT_COUNTERVALUE cpuResult;

	nvmlDevice_t nvmlDevice;
	unsigned int deviceCount;
	nvmlUtilization_t utilStruct;
	nvmlTemperature_t gpuStruct;
	nvmlMemory_t memInfo;
};

