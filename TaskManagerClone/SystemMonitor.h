#pragma once

#include <Windows.h>
#include <Pdh.h>
#include <nvml.h>

class SystemMonitor
{
public:
	float GetCPUUsagePercentage();
	float GetGPUUsagePercentage();
	float GetGPUUsageVRAM();
	unsigned int GetGPUUsageTemp();
	float GetRAMUsagePercentage();
	float GetRAMUsageGB();
	float GetFreeRAMGB();
	void InitPDH();
	void InitNVML();
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
};

