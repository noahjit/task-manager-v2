#include "SystemMonitor.h"
#include <Windows.h>
#include <Pdh.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <string>
#include <vector>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wbemuuid.lib")
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

void SystemMonitor::InitCOM() {
	hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) return;
	hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

	if (FAILED(hr)) {
		CoUninitialize();
		return;
	}

	hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
	hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);

	if (FAILED(hr)) {
		std::cout << "Could not connect" << std::endl;
		return;
	}
}

void SystemMonitor::CleanupCOM() {
	if (pSvc) {
		pSvc->Release();
		pSvc = nullptr;
	}

	if (pLoc) {
		pLoc->Release();
		pLoc = nullptr;
	}

	CoUninitialize();
}

float SystemMonitor::GetCPUUsagePercentage() {
	PdhCollectQueryData(handle);
	PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, 0, &cpuResult);
	return (float)cpuResult.doubleValue;
}

std::string SystemMonitor::GetWMIInfo(std::string wmiClass, std::string what) {
	std::string result;

	auto values = GetWMIValues(wmiClass, what);

	if (!values.empty())
		return values.front();

	return {};
}

std::vector<std::string> SystemMonitor::GetWMIValues(std::string wmiClass, std::string what) {
	std::vector<std::string> result;
	IEnumWbemClassObject* pEnumerator = nullptr;
	std::wstring query = L"SELECT * FROM " + std::wstring(wmiClass.begin(), wmiClass.end());
	hr = pSvc->ExecQuery(bstr_t(L"WQL"), bstr_t(query.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);

	if (FAILED(hr)) {
		std::cout << "Query failed" << std::endl;
		return {};
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;

	while (pEnumerator) {
		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (0 == uReturn) break;

		VARIANT vtProp;
		std::wstring wide(what.begin(), what.end());
		pclsObj->Get(wide.c_str(), 0, &vtProp, 0, 0);

		if (vtProp.vt == VT_BSTR)
			result.push_back((char*)_bstr_t(vtProp.bstrVal));
		else if (vtProp.vt == VT_UI4 || vtProp.vt == VT_I4)
			result.push_back(std::to_string(vtProp.uintVal));
		else if (vtProp.vt == VT_UI2 || vtProp.vt == VT_I2)
			result.push_back(std::to_string(vtProp.uiVal));

		VariantClear(&vtProp);
		pclsObj->Release();
	}

	pEnumerator->Release();
	return result;
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