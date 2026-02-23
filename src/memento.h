#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct InventoryItem {
    std::string filename;
    std::string description;
};

class Memento final {
public:
    Memento(const std::filesystem::path& filepath = "memento.txt");

    std::string Read();
    void Write(const std::string& note);
    void AppendInventory(const std::string& toolName, const std::string& description);
    std::vector<InventoryItem> GetInventory();

private:
    std::filesystem::path _filepath;
};
