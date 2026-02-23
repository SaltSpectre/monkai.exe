#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

struct ToolCall {
    std::string id;
    std::string name;
    nlohmann::json arguments;
};

struct AgentResponse {
    std::string content;
    std::vector<ToolCall> toolCalls;
};

class OpenAIClient final {
public:
    OpenAIClient(const std::string& apiKey, const std::string& model = "gpt-4o");
    ~OpenAIClient();

    AgentResponse Chat(const nlohmann::json& messages);

    static nlohmann::json BuildToolsSchema();

private:
    std::string _apiKey;
    std::string _model;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
};
