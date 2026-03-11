#include "StoragePage.h"
#include "imgui.h"

#include <sstream>

void StoragePage::DrawDirectoryTree(const std::filesystem::path& drive, int depth) {
    std::vector<std::filesystem::directory_entry> files;
    std::vector<std::filesystem::directory_entry> folders;

    if (depth > 7)
        return;

    std::error_code ec;
 
    for (auto entry : std::filesystem::directory_iterator(drive, ec)) {
        if (ec)
            continue;

        if (entry.is_directory()) 
            folders.push_back(entry);

        if (entry.is_regular_file()) 
            files.push_back(entry);
    }
   
    for (std::filesystem::directory_entry entry : folders) {
        uintmax_t size = 0;
        auto sizeIterator = folderSizes.find(entry.path());
        auto calcIterator = folderCalculating.find(entry.path());
        bool isBinned = binnedItems.find(entry.path()) != binnedItems.end();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        if (isBinned)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        
        bool expanded = ImGui::TreeNode(entry.path().filename().string().c_str());

        if (isBinned)
            ImGui::PopStyleColor();

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Move to bin")) {
                BinItem(entry.path(), sizeIterator->second);
            }
            if (ImGui::MenuItem("Organise directory")) {
                OrganiseDirectory(entry.path());
            }
            ImGui::EndPopup();
        }

        ImGui::TableSetColumnIndex(1);

        if (isBinned) 
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        
        if (sizeIterator != folderSizes.end()) {
            if (!isBinned) {

                if (sizeIterator->second > 10ull * 1024 * 1024 * 1024)
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", FormatSize(sizeIterator->second));
                else if (sizeIterator->second > 5ull * 1024 * 1024 * 1024) 
                    ImGui::TextColored(ImVec4(1, 0.647f, 0, 1), "%s", FormatSize(sizeIterator->second));
                else 
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", FormatSize(sizeIterator->second));
            }
            else 
                ImGui::Text("%s", FormatSize(sizeIterator->second));
            
        }
        else {
            if (calcIterator == folderCalculating.end())
                RequestFolderSize(entry.path());
            
            ImGui::Text("...");
        }

        if (isBinned)
            ImGui::PopStyleColor();

        if (expanded) {
            DrawDirectoryTree(entry.path(), depth + 1);
            ImGui::TreePop();
        }
    }

    for (std::filesystem::directory_entry entry : files) {
        bool isBinned = binnedItems.find(entry.path()) != binnedItems.end();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        std::string size = FormatSize(entry.file_size());

        if (isBinned) 
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        if (ImGui::TreeNodeEx(entry.path().filename().string().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {}

        if (isBinned) 
            ImGui::PopStyleColor();

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Move to bin")) {
                BinItem(entry.path(), std::stoi(size));
            }
            ImGui::EndPopup();
        }

        ImGui::TableSetColumnIndex(1);

        if (isBinned)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        if (!isBinned) {
            if (std::stoi(size) > 10ull * 1024 * 1024 * 1024) 
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", size.c_str());
            else if (std::stoi(size) > 5ull * 1024 * 1024 * 1024) 
                ImGui::TextColored(ImVec4(1, 0.647f, 0, 1), "%s", size.c_str());
            else 
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", size.c_str());
        }
        else 
            ImGui::Text("%s", size.c_str());

        if (isBinned) {
            ImGui::PopStyleColor();
        }
        
    }
}

std::string StoragePage::FormatSize(long long bytes) {
    double size = bytes;
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int index = 0;
    while (size >= 1024 && index < 4) {
        size /= 1024;
        index++;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[index];
    return oss.str();
}

uintmax_t StoragePage::CalculateFolderSize(const std::filesystem::path& path) {
    std::error_code error;
    uintmax_t total = 0;

    for (auto entry = std::filesystem::recursive_directory_iterator(path, std::filesystem::directory_options::skip_permission_denied, error);
        entry != std::filesystem::recursive_directory_iterator();
        entry.increment(error))
        {
        if (error) {
            error.clear();
            continue;
        }

        if (entry->is_regular_file(error)) {
            total += entry->file_size(error);
        }     
    }
    return total;
}

void StoragePage::RequestFolderSize(const std::filesystem::path& path) {
    {
        std::lock_guard<std::mutex> lock(folderSizeMutex);

        if (folderSizes.find(path) != folderSizes.end() ||(folderCalculating.find(path) != folderCalculating.end() && folderCalculating.find(path)->second))
            return;

        folderCalculating[path] = true;
    }

    std::thread([this, path]() {
        uintmax_t size = CalculateFolderSize(path);

        std::lock_guard<std::mutex> lock(folderSizeMutex);
        folderSizes[path] = size;
        folderCalculating[path] = false;
        }).detach();
}

void StoragePage::BinItem(const std::filesystem::path& path, uintmax_t size) {
    binnedItems[path] = size;
}

void StoragePage::ShowBinnedItems() {
    uintmax_t total = 0;
    if (binnedItems.empty()) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("No Items in bin.");
        return;
    }

    for (auto it = binnedItems.begin(); it != binnedItems.end();) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Selectable(it->first.string().c_str(), false);
        ImGui::TableSetColumnIndex(1);

        total += it->second;

        bool shouldDelete = false;
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Restore")) {
                shouldDelete = true;
            }
            ImGui::EndPopup();
        }

        if (it->second > 10ull * 1024 * 1024 * 1024) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", FormatSize(it->second));
        }
        else if (it->second > 5ull * 1024 * 1024 * 1024) {
            ImGui::TextColored(ImVec4(1, 0.647f, 0, 1), "%s", FormatSize(it->second));
        }
        else {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", FormatSize(it->second));
        }

        if (shouldDelete)
            it = binnedItems.erase(it);
        else
            it++;
    }

    ImGui::TableSetColumnIndex(1);
    ImGui::Text("Total: %s", FormatSize(total));

    if (ImGui::Button("Delete permanently")) {
        failedDeletes.clear();
        std::error_code ec;
        hasFailures = false;

        for (auto entry : binnedItems) {
            std::filesystem::remove_all(entry.first, ec);

            if (ec) {
                failedDeletes.push_back(entry.first);
                hasFailures = true;
            }
        }

        openFailedPopup = hasFailures;
        openSuccessPopup = !hasFailures;
    }
}

void StoragePage::OrganiseDirectory(std::filesystem::path path) {
    if (!std::filesystem::exists(path)) {
        return;
    }

    try {
        for (auto const& entry : std::filesystem::recursive_directory_iterator{ path }) {
            auto it = extension_to_folder.find(entry.path().extension());

            if (!std::filesystem::is_regular_file(entry.path()))
                continue;

            if (it != extension_to_folder.end())
                MkDirAndMove(it->second, entry);
            else
                MkDirAndMove("Others", entry);
        }
    }
    catch (const std::filesystem::filesystem_error e) {
        return;
    }
}

void StoragePage::MkDirAndMove(std::string folderName, std::filesystem::directory_entry entry) {
    const std::filesystem::path newPath = entry.path().parent_path() / folderName;
    const std::filesystem::path finalPath = newPath / entry.path().filename();

    if (!std::filesystem::exists(newPath))
        std::filesystem::create_directory(newPath);

    try {
        std::filesystem::rename(entry.path(), finalPath);
    }
    catch (const std::filesystem::filesystem_error e) {
        return;
    }
}

const std::unordered_map<std::filesystem::path, std::string, PathHash> StoragePage::extension_to_folder =
{
    // documents
    {".doc", "Documents"},
    {".docx", "Documents"},
    {".pdf", "Documents"},
    {".txt", "Documents"},
    {".odt", "Documents"},
    {".ppt", "Presentations"},
    {".pptx", "Presentations"},

    // image
    {".jpg", "Images"},
    {".jpeg", "Images"},
    {".png", "Images"},
    {".gif", "Images"},
    {".bmp", "Images"},
    {".tif", "Images"},
    {".tiff", "Images"},
    {".bmp", "Images"},

    // audio
    {".mp3", "Audio"},
    {".wav", "Audio"},
    {".aac", "Audio"},
    {".m4a", "Audio"},
    {".ogg", "Audio"},

    // video
    {".mp4", "Video"},
    {".avi", "Video"},
    {".mov", "Video"},
    {".wmv", "Video"},
    {".flv", "Video"},

    // archive
    {".zip", "Archives"},
    {".rar", "Archives"},
    {".7z", "Archives"},

    // executables
    {".exe", "Executables"},
    {".bat", "Scripts"},
    {".sh", "Scripts"},
    {".js", "Scripts"},
    {".dll", "Executables"},

    // data
    {".csv", "Data"},
    {".xml", "Data"},
    {".json", "Data"},
    {".ini", "Data"},
    {".sql", "Data"}
};
