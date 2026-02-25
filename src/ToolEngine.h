#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct ToolResult {
    bool success;
    std::string output;
    int exitCode;
};

class ToolEngine final {
public:
    ToolEngine(const std::filesystem::path& toolsDir = R"(C:\temp\monkai_tools)",
               size_t maxOutput = 4000);

    bool WriteScript(const std::string& filename, const std::string& code);
    ToolResult ExecuteScript(const std::string& filename, int timeoutMs = 30000);
    std::vector<std::string> ListTools();

private:
    std::filesystem::path _toolsDir;
    size_t _maxOutput;

    std::string GetInterpreter(const std::string& filename);
    ToolResult RunProcess(const std::string& command, int timeoutMs);
};
