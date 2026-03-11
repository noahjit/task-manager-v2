#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

struct PathHash {
    size_t operator()(const std::filesystem::path& p) const {
        return std::filesystem::hash_value(p);
    }
};

class StoragePage
{
    public:
        void DrawDirectoryTree(const std::filesystem::path& drive, int depth = 0);
        void ShowBinnedItems();
        std::unordered_map<std::filesystem::path, uintmax_t, PathHash> binnedItems;
        bool hasFailures = false;
        bool openFailedPopup = false;
        bool openSuccessPopup = false;
        std::vector<std::filesystem::path> failedDeletes;
        std::string FormatSize(long long bytes);
    private:
        uintmax_t CalculateFolderSize(const std::filesystem::path&);
        void RequestFolderSize(const std::filesystem::path&);
        void BinItem(const std::filesystem::path&, uintmax_t);
        void OrganiseDirectory(std::filesystem::path path);
        std::unordered_map<std::filesystem::path, uintmax_t, PathHash> folderSizes;
        std::unordered_map<std::filesystem::path, bool, PathHash> folderCalculating;
        std::mutex folderSizeMutex;
        static const std::unordered_map<std::filesystem::path, std::string, PathHash> extension_to_folder;
        void MkDirAndMove(std::string folderName, std::filesystem::directory_entry entry);
};