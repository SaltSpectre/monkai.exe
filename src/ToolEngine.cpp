#include "ToolEngine.h"

#include <fstream>
#include <iostream>
#include <array>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using namespace std;
namespace fs = filesystem;

ToolEngine::ToolEngine(const fs::path& toolsDir) : _toolsDir(toolsDir) {
    fs::create_directories(_toolsDir);
}

bool ToolEngine::WriteScript(const string& filename, const string& code) {
    auto filepath = _toolsDir / filename;
    ofstream file(filepath);
    if (!file.is_open()) return false;

    file << code;
    file.close();
    return file.good();
}

string ToolEngine::GetInterpreter(const string& filename) {
    auto ext = fs::path(filename).extension().string();
    if (ext == ".py")  return "python";
    if (ext == ".ps1") return "powershell -ExecutionPolicy Bypass -File";
    if (ext == ".sh")  return "bash";
    if (ext == ".bat" || ext == ".cmd") return "cmd /c";
    return "python";
}

ToolResult ToolEngine::ExecuteScript(const string& filename, int timeoutMs) {
    auto filepath = _toolsDir / filename;

    if (!fs::exists(filepath)) {
        return { false, "error: " + filename + " not found. write it first with tool_write_script.", -1 };
    }

    string interpreter = GetInterpreter(filename);
    string command = interpreter + " \"" + filepath.string() + "\"";

    return RunProcess(command, timeoutMs);
}

vector<string> ToolEngine::ListTools() {
    vector<string> tools;
    if (!fs::exists(_toolsDir)) return tools;

    for (auto& entry : fs::directory_iterator(_toolsDir)) {
        if (entry.is_regular_file()) {
            tools.push_back(entry.path().filename().string());
        }
    }
    return tools;
}

#ifdef _WIN32
ToolResult ToolEngine::RunProcess(const string& command, int timeoutMs) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdoutRead = nullptr, stdoutWrite = nullptr;
    HANDLE stderrRead = nullptr, stderrWrite = nullptr;

    if (!CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0) ||
        !CreatePipe(&stderrRead, &stderrWrite, &sa, 0)) {
        return { false, "failed to create pipe", -1 };
    }

    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stderrWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    string cmdLine = "cmd /c " + command;

    if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(stdoutRead); CloseHandle(stdoutWrite);
        CloseHandle(stderrRead); CloseHandle(stderrWrite);
        return { false, "failed to start process", -1 };
    }

    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeoutMs));

    string output;
    char buffer[4096];
    DWORD bytesRead;

    while (ReadFile(stdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    string errOutput;
    while (ReadFile(stderrRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        errOutput += buffer;
    }

    if (!errOutput.empty()) {
        output += "\n[stderr]: " + errOutput;
    }

    DWORD exitCode = 0;
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        output += "\n[timeout]: script did not complete within " + to_string(timeoutMs) + "ms";
        exitCode = 1;
    } else {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }

    const size_t maxOutput = 4000;
    if (output.size() > maxOutput) {
        output = output.substr(0, maxOutput) + "\n...[output truncated, too long]";
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(stdoutRead);
    CloseHandle(stderrRead);

    return { exitCode == 0, output, static_cast<int>(exitCode) };
}
#else
ToolResult ToolEngine::RunProcess(const string& command, int timeoutMs) {
    (void)timeoutMs;
    array<char, 4096> buffer;
    string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return { false, "popen failed", -1 };

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }
    int status = pclose(pipe);
    return { status == 0, output, status };
}
#endif
