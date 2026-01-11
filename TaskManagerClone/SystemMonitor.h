#pragma once

#include <Windows.h>
#include <Pdh.h>
#include <nvml.h>
#include <iostream>
#include <comdef.h>
#include <Wbemidl.h>
#include <vector>

struct Process {
	std::string name;
	DWORD pid = 0;
	ULONGLONG memoryUse = 0;
	std::string commandLine;
	std::string creationDate;
};

class SystemMonitor
{
public:
	//general 
	std::vector<std::string> GetWMIValues(std::string wmiClass, std::string what);
	std::string GetWMIInfo(std::string wmiClass, std::string what);

	// cpu
	float GetCPUUsagePercentage();

	// gpu
	float GetGPUUsagePercentage();
	unsigned int GetGPUTemp();
	std::string GetGPUModelName();

	// initialize
	void InitPDH();
	void InitNVML();
	void InitCOM();
	void CleanupCOM();

	// memory
	float GetUsedVRAM();
	float GetRAMUsagePercentage();
	float GetRAMUsageGB();
	float GetTotalVRAM();
	float GetFreeRAMGB();

	// processes
	std::vector<Process> GetWMIProcesses();
	std::string ParseWMIDate(const std::string& wmiDate);
	bool terminateProcessByPID(DWORD id);

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

	IWbemLocator* pLoc;
	IWbemServices* pSvc;
	HRESULT hr;
};

struct driveStruct {
	std::string driveLetter; // C: / D:
	std::string interfaceType; // NVMe, SATA, etc
	std::string size;
	std::string freeSpace;
};
