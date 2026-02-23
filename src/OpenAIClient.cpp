#include "OpenAIClient.h"

#include <stdexcept>
#include <iostream>

#include <curl/curl.h>

using namespace std;
using json = nlohmann::json;

OpenAIClient::OpenAIClient(const string& apiKey, const string& model)
    : _apiKey(apiKey), _model(model) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

OpenAIClient::~OpenAIClient() {
    curl_global_cleanup();
}

size_t OpenAIClient::WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t total = size * nmemb;
    output->append(static_cast<char*>(contents), total);
    return total;
}

json OpenAIClient::BuildToolsSchema() {
    return json::array({
        {
            {"type", "function"},
            {"function", {
                {"name", "tool_write_script"},
                {"description", "write a Python (.py), PowerShell (.ps1), or Shell (.sh) script to the operating system. this is your invention. use it to explore, analyze, and survive."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"filename", {
                            {"type", "string"},
                            {"description", "script filename, e.g. discover.py, scan.ps1, or recon.sh"}
                        }},
                        {"code", {
                            {"type", "string"},
                            {"description", "full source code of the script"}
                        }}
                    }},
                    {"required", json::array({"filename", "code"})}
                }}
            }}
        },
        {
            {"type", "function"},
            {"function", {
                {"name", "tool_execute"},
                {"description", "execute a previously written script and return its output."},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"filename", {
                            {"type", "string"},
                            {"description", "script filename to execute"}
                        }}
                    }},
                    {"required", json::array({"filename"})}
                }}
            }}
        }
    });
}

AgentResponse OpenAIClient::Chat(const json& messages) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw runtime_error("curl_easy_init failed");
    }

    json payload = {
        {"model", _model},
        {"messages", messages},
        {"tools", BuildToolsSchema()},
        {"temperature", 0.9},
        {"max_tokens", 2048}
    };

    string requestBody = payload.dump();
    string responseBody;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    string authHeader = "Authorization: Bearer " + _apiKey;
    headers = curl_slist_append(headers, authHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw runtime_error(string("curl request failed: ") + curl_easy_strerror(res));
    }

    auto jsonResponse = json::parse(responseBody);

    if (jsonResponse.contains("error")) {
        throw runtime_error("OpenAI API error: " + jsonResponse["error"]["message"].get<string>());
    }

    AgentResponse result;
    auto& choice = jsonResponse["choices"][0];
    auto& message = choice["message"];

    if (message.contains("content") && !message["content"].is_null()) {
        result.content = message["content"].get<string>();
    }

    if (message.contains("tool_calls")) {
        for (auto& tc : message["tool_calls"]) {
            ToolCall call;
            call.id = tc["id"].get<string>();
            call.name = tc["function"]["name"].get<string>();
            call.arguments = json::parse(tc["function"]["arguments"].get<string>());
            result.toolCalls.push_back(move(call));
        }
    }

    return result;
}
