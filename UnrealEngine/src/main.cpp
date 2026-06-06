#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <fstream>
#include "json.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

using json = nlohmann::json;

// --- Global State ---
std::queue<json> TaskQueue;
std::mutex QueueMutex;
std::condition_variable QueueCV;

json ActiveProfile = json::object();
std::mutex ProfileMutex;

std::string GetExePath() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    return std::string(buffer).substr(0, pos);
}

json LoadProfileFromDisk(const std::string& profileName) {
    std::string profilePath = GetExePath() + "\\profiles\\" + profileName + ".json";
    std::ifstream profileFile(profilePath);
    if (profileFile.is_open()) {
        json profile;
        profileFile >> profile;
        profileFile.close();
        return profile;
    }
    return json::object();
}

void ResolveAndLoadProfile(const std::string& clientName) {
    std::lock_guard<std::mutex> Lock(ProfileMutex);

    // Priority 1: BRIDGE_PROFILE environment variable
    const char* envProfile = std::getenv("BRIDGE_PROFILE");
    if (envProfile && std::string(envProfile).length() > 0) {
        ActiveProfile = LoadProfileFromDisk(envProfile);
        if (!ActiveProfile.empty()) return;
    }

    // Priority 2: Auto-detect from MCP clientInfo.name
    std::string lowerClient = clientName;
    for (auto& c : lowerClient) c = (char)tolower(c);

    if (lowerClient.find("antigravity") != std::string::npos) {
        ActiveProfile = LoadProfileFromDisk("antigravity");
        if (!ActiveProfile.empty()) return;
    }

    // Priority 3: Fall back to default.json
    ActiveProfile = LoadProfileFromDisk("default");
}

json GetActiveProfile() {
    std::lock_guard<std::mutex> Lock(ProfileMutex);
    return ActiveProfile;
}

// --- HTTP ---
std::string SendHttpRequest(const std::wstring& Path, const std::string& Method, const std::string& Payload = "") {
    HINTERNET hSession = WinHttpOpen(L"UnrealEngineBridge/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", 18777, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, std::wstring(Method.begin(), Method.end()).c_str(), Path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    bool bSuccess = false;
    if (Method == "POST") {
        bSuccess = WinHttpSendRequest(hRequest, L"Content-Type: application/json\r\n", -1L, (LPVOID)Payload.c_str(), Payload.length(), Payload.length(), 0);
    } else {
        bSuccess = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    }

    if (bSuccess) bSuccess = WinHttpReceiveResponse(hRequest, NULL);

    std::string ResponseStr;
    if (bSuccess) {
        DWORD Size = 0;
        DWORD Downloaded = 0;
        do {
            Size = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &Size)) break;
            if (Size == 0) break;
            char* Buffer = new char[Size + 1];
            if (WinHttpReadData(hRequest, Buffer, Size, &Downloaded)) {
                Buffer[Downloaded] = 0;
                ResponseStr.append(Buffer, Downloaded);
            }
            delete[] Buffer;
        } while (Size > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return ResponseStr;
}

// --- Background Threads ---
void MonitorThread() {
    bool bWasAvailable = false;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::string ResultStr = SendHttpRequest(L"/api/tools", "GET");
        bool bIsAvailable = !ResultStr.empty();
        
        if (bIsAvailable && !bWasAvailable) {
            json Notification = {
                {"jsonrpc", "2.0"},
                {"method", "notifications/tools/list_changed"}
            };
            std::cout << Notification.dump() << "\n" << std::flush;
        }
        bWasAvailable = bIsAvailable;
    }
}

static const std::string IMAGE_TOOLS[] = {
    "capture_widget",
    "capture_viewport",
    "capture_niagara_system_isolated"
};

bool IsImageTool(const std::string& toolName) {
    for (const auto& t : IMAGE_TOOLS) {
        if (t == toolName) return true;
    }
    return false;
}

void WorkerThread() {
    while (true) {
        json Task;
        {
            std::unique_lock<std::mutex> Lock(QueueMutex);
            QueueCV.wait(Lock, [] { return !TaskQueue.empty(); });
            Task = TaskQueue.front();
            TaskQueue.pop();
        }

        std::string Id;
        if (Task["id"].is_string()) Id = Task["id"].get<std::string>();
        else if (Task["id"].is_number()) Id = std::to_string(Task["id"].get<int>());

        json Params = Task.value("params", json::object());
        
        json Payload = {
            {"name", Params.value("name", "")},
            {"arguments", Params.value("arguments", json::object())}
        };

        std::string ResultStr = SendHttpRequest(L"/api/execute_tool", "POST", Payload.dump());
        
        json Response;
        Response["jsonrpc"] = "2.0";
        if (Task.contains("id")) {
            Response["id"] = Task["id"];
        }
        
        if (ResultStr.empty()) {
            Response["error"] = {
                {"code", -32603},
                {"message", "Unreal Engine Editor is not running or Antigravity HTTP server is unreachable"}
            };
        } else {
            json UE_Result = json::parse(ResultStr, nullptr, false);
            if (UE_Result.is_discarded()) {
                Response["error"] = { {"code", -32603}, {"message", "Invalid JSON from UE"} };
            } else {
                bool bSuccess = UE_Result.value("bSuccess", false);
                std::string Msg = UE_Result.value("ResultMessage", "");
                
                json Content = json::array();
                std::string toolName = Payload.value("name", "");
                
                json profile = GetActiveProfile();
                bool bImageSupport = profile.value("image_support", false);
                
                if (bSuccess && IsImageTool(toolName) && bImageSupport) {
                    // Format as MCP image content block
                    std::string base64Data = Msg;
                    size_t prefixPos = base64Data.find("base64,");
                    if (prefixPos != std::string::npos) {
                        base64Data = base64Data.substr(prefixPos + 7);
                    }
                    Content.push_back({
                        {"type", "image"},
                        {"data", base64Data},
                        {"mimeType", "image/png"}
                    });
                } else {
                    // Format as plain text
                    Content.push_back({
                        {"type", "text"},
                        {"text", Msg}
                    });
                }
                
                Response["result"] = {
                    {"content", Content},
                    {"isError", !bSuccess}
                };
            }
        }

        std::cout << Response.dump() << "\n" << std::flush;
    }
}

// --- Main ---
int main() {
    // Attempt early profile resolution from env var (before initialize handshake)
    ResolveAndLoadProfile("");

    std::thread Worker(WorkerThread);
    Worker.detach();

    std::thread Monitor(MonitorThread);
    Monitor.detach();

    std::string Line;
    while (std::getline(std::cin, Line)) {
        if (Line.empty()) continue;
        json Req = json::parse(Line, nullptr, false);
        if (Req.is_discarded()) continue;

        std::string Method = Req.value("method", "");
        
        if (Method == "initialize") {
            // Extract clientInfo.name for profile auto-detection
            json Params = Req.value("params", json::object());
            json ClientInfo = Params.value("clientInfo", json::object());
            std::string clientName = ClientInfo.value("name", "");

            ResolveAndLoadProfile(clientName);

            json Capabilities = { 
                {"tools", {
                    {"listChanged", true}
                }} 
            };
            json Result = {
                {"protocolVersion", "2024-11-05"},
                {"capabilities", Capabilities},
                {"serverInfo", {{"name", "UnrealEngine"}, {"version", "2.0.0"}}}
            };
            json Response = {
                {"jsonrpc", "2.0"},
                {"id", Req["id"]},
                {"result", Result}
            };
            std::cout << Response.dump() << "\n" << std::flush;
        }
        else if (Method == "tools/list") {
            std::string ResultStr = SendHttpRequest(L"/api/tools", "GET");
            json Response;
            Response["jsonrpc"] = "2.0";
            Response["id"] = Req["id"];
            
            if (ResultStr.empty()) {
                Response["result"] = { {"tools", json::array()} };
            } else {
                json UE_Tools = json::parse(ResultStr, nullptr, false);
                if (UE_Tools.is_discarded()) {
                    Response["error"] = { {"code", -32603}, {"message", "Invalid JSON from UE"} };
                } else {
                    json McpTools = json::array();
                    
                    json profile = GetActiveProfile();
                    json disabledTools = profile.value("disabled_tools", json::array());
                    json toolOverrides = profile.value("tool_overrides", json::object());

                    if (UE_Tools.is_object() && UE_Tools.contains("tools") && UE_Tools["tools"].is_array()) {
                        for (auto& domainObj : UE_Tools["tools"]) {
                            if (domainObj.is_object() && domainObj.contains("tools") && domainObj["tools"].is_array()) {
                                for (auto& t : domainObj["tools"]) {
                                    json formattedTool = t;
                                    if (formattedTool.contains("input_schema")) {
                                        formattedTool["inputSchema"] = formattedTool["input_schema"];
                                        formattedTool.erase("input_schema");
                                    }
                                    
                                    std::string toolName = formattedTool.value("name", "");
                                    
                                    // Check disabled_tools list
                                    bool disabled = false;
                                    for (const auto& dt : disabledTools) {
                                        if (dt.is_string() && dt.get<std::string>() == toolName) {
                                            disabled = true;
                                            break;
                                        }
                                    }
                                    if (disabled) continue;

                                    // Apply tool_overrides
                                    if (toolOverrides.contains(toolName)) {
                                        json overrideData = toolOverrides[toolName];
                                        if (overrideData.contains("description")) {
                                            formattedTool["description"] = overrideData["description"];
                                        }
                                        if (overrideData.contains("inputSchema")) {
                                            formattedTool["inputSchema"] = overrideData["inputSchema"];
                                        }
                                    }
                                    
                                    McpTools.push_back(formattedTool);
                                }
                            }
                        }
                    }
                    Response["result"] = { {"tools", McpTools} };
                }
            }
            std::cout << Response.dump() << "\n" << std::flush;
        }
        else if (Method == "tools/call") {
            std::lock_guard<std::mutex> Lock(QueueMutex);
            TaskQueue.push(Req);
            QueueCV.notify_one();
        }
        else if (Method == "notifications/initialized" || Method == "ping") {
            // No-op
        }
    }
    return 0;
}
