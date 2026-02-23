#include "Memento.h"

#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

using namespace std;
namespace fs = filesystem;

Memento::Memento(const fs::path& filepath) : _filepath(filepath) {}

string Memento::Read() {
    if (!fs::exists(_filepath)) {
        return "";
    }

    ifstream file(_filepath);
    if (!file.is_open()) return "";

    ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void Memento::Write(const string& note) {
    auto now = chrono::system_clock::now();
    auto timeVal = chrono::system_clock::to_time_t(now);
    tm tmBuf{};
    localtime_s(&tmBuf, &timeVal);

    ofstream file(_filepath);
    file << "[" << put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << "]\n";
    file << note << "\n";

    auto inventory = GetInventory();
    if (!inventory.empty()) {
        file << "\n--- INVENTORY ---\n";
        for (auto& item : inventory) {
            file << "- " << item.filename << ": " << item.description << "\n";
        }
    }
}

void Memento::AppendInventory(const string& toolName, const string& description) {
    string current = Read();

    auto inv = GetInventory();
    for (auto& item : inv) {
        if (item.filename == toolName) {
            return;
        }
    }

    ofstream file(_filepath, ios::app);
    if (current.find("--- INVENTORY ---") == string::npos) {
        file << "\n--- INVENTORY ---\n";
    }
    file << "- " << toolName << ": " << description << "\n";
}

vector<InventoryItem> Memento::GetInventory() {
    vector<InventoryItem> items;
    string content = Read();

    auto pos = content.find("--- INVENTORY ---");
    if (pos == string::npos) return items;

    istringstream stream(content.substr(pos));
    string line;
    getline(stream, line);

    while (getline(stream, line)) {
        if (line.size() < 3 || line[0] != '-' || line[1] != ' ') continue;

        auto colon = line.find(": ", 2);
        if (colon == string::npos) continue;

        InventoryItem item;
        item.filename = line.substr(2, colon - 2);
        item.description = line.substr(colon + 2);
        items.push_back(move(item));
    }

    return items;
}
