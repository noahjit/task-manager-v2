#include "SystemMonitor.h"
#include <Windows.h>
#include <Pdh.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <string>
#include <vector>
#include <TlHelp32.h>

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

std::vector<Process> SystemMonitor::GetWMIProcesses() {
	std::vector<Process> processes;

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT HR = pSvc->ExecQuery(bstr_t(L"WQL"), bstr_t("SELECT * FROM Win32_Process"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);

	if (FAILED(HR)) 
		return processes;

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;

	while (pEnumerator) {
		HRESULT HR = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (0 == uReturn) break;
		
		Process process;
		VARIANT vtProp;

		HR = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
		if (SUCCEEDED(HR) && vtProp.vt == VT_BSTR) {
			process.name = _com_util::ConvertBSTRToString(vtProp.bstrVal);
		}
		VariantClear(&vtProp);

		HR = pclsObj->Get(L"ProcessId", 0, &vtProp, 0, 0);
		if (SUCCEEDED(HR)) {
			process.pid = vtProp.uintVal;
		}
		VariantClear(&vtProp);

		HR = pclsObj->Get(L"WorkingSetSize", 0, &vtProp, 0, 0);
		if (SUCCEEDED(HR) && vtProp.vt == VT_BSTR) {
			process.memoryUse = _wtoi64(vtProp.bstrVal);
		}
		VariantClear(&vtProp);

		HR = pclsObj->Get(L"CommandLine", 0, &vtProp, 0, 0);
		if (SUCCEEDED(HR) && vtProp.vt == VT_BSTR) {
			process.commandLine = _com_util::ConvertBSTRToString(vtProp.bstrVal);
		}
		VariantClear(&vtProp);

		HR = pclsObj->Get(L"CreationDate", 0, &vtProp, 0, 0);
		if (SUCCEEDED(HR) && vtProp.vt == VT_BSTR) {
			process.creationDate = _com_util::ConvertBSTRToString(vtProp.bstrVal);
		}
		VariantClear(&vtProp);
		
		processes.push_back(process);
		pclsObj->Release();
	}

	pEnumerator->Release();
	return processes;
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

std::string SystemMonitor::ParseWMIDate(const std::string& wmiDate) {
	// WMI format: "20250108143022.500000-000"
	// Extract: YYYYMMDDHHMMSS
	if (wmiDate.length() >= 14) {
		std::string year = wmiDate.substr(0, 4);
		std::string month = wmiDate.substr(4, 2);
		std::string day = wmiDate.substr(6, 2);
		std::string hour = wmiDate.substr(8, 2);
		std::string min = wmiDate.substr(10, 2);
		std::string sec = wmiDate.substr(12, 2);

		return year + "-" + month + "-" + day + " " + hour + ":" + min + ":" + sec;
	}
	return wmiDate;
}

bool SystemMonitor::terminateProcessByPID(DWORD id) {
	HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, id);

	if (hProcess == NULL) {
		return false;
	}

	bool successs = TerminateProcess(hProcess, 1);
	CloseHandle(hProcess);
	return successs;
}