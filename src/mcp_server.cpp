/*
 * This file is part of pcb2gcode.
 *
 * Model Context Protocol (MCP) server implementation.
 */

#include "mcp_server.hpp"
#include "nlohmann/json.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#else
#include <windows.h>
#include <direct.h>
#endif

using json = nlohmann::json;

struct ProcessResult {
    int exit_code = -1;
    std::string stdout_str;
    std::string stderr_str;
};

static ProcessResult run_process(const std::string& exec_path, const std::vector<std::string>& args, const std::string& cwd) {
    ProcessResult result;
#ifndef _WIN32
    int out_pipe[2];
    int err_pipe[2];
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        result.stderr_str = "Failed to create pipes";
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        result.stderr_str = "Failed to fork process";
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return result;
    }

    if (pid == 0) {
        close(out_pipe[0]);
        close(err_pipe[0]);

        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        close(out_pipe[1]);
        close(err_pipe[1]);

        if (!cwd.empty()) {
            if (chdir(cwd.c_str()) != 0) {
                perror("chdir failed");
                _exit(127);
            }
        }

        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(exec_path.c_str()));
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execv(exec_path.c_str(), c_args.data());
        execvp(exec_path.c_str(), c_args.data());

        perror("exec failed");
        _exit(127);
    } else {
        close(out_pipe[1]);
        close(err_pipe[1]);

        char buffer[4096];
        ssize_t bytes_read;

        while (true) {
            bool read_any = false;
            bytes_read = read(out_pipe[0], buffer, sizeof(buffer));
            if (bytes_read > 0) {
                result.stdout_str.append(buffer, bytes_read);
                read_any = true;
            }
            bytes_read = read(err_pipe[0], buffer, sizeof(buffer));
            if (bytes_read > 0) {
                result.stderr_str.append(buffer, bytes_read);
                read_any = true;
            }
            if (!read_any) {
                int status;
                pid_t r = waitpid(pid, &status, WNOHANG);
                if (r > 0) {
                    while ((bytes_read = read(out_pipe[0], buffer, sizeof(buffer))) > 0) {
                        result.stdout_str.append(buffer, bytes_read);
                    }
                    while ((bytes_read = read(err_pipe[0], buffer, sizeof(buffer))) > 0) {
                        result.stderr_str.append(buffer, bytes_read);
                    }
                    if (WIFEXITED(status)) {
                        result.exit_code = WEXITSTATUS(status);
                    } else if (WIFSIGNALED(status)) {
                        result.exit_code = 128 + WTERMSIG(status);
                    }
                    break;
                }
                usleep(5000);
            }
        }

        close(out_pipe[0]);
        close(err_pipe[0]);
    }
#else
    std::string cmd = "\"" + exec_path + "\"";
    for (const auto& a : args) {
        cmd += " \"" + a + "\"";
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hOutRead, hOutWrite;
    HANDLE hErrRead, hErrWrite;
    CreatePipe(&hOutRead, &hOutWrite, &sa, 0);
    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
    CreatePipe(&hErrRead, &hErrWrite, &sa, 0);
    SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hOutWrite;
    si.hStdError = hErrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    LPCSTR working_dir = cwd.empty() ? NULL : cwd.c_str();

    if (CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, TRUE, 0, NULL, working_dir, &si, &pi)) {
        CloseHandle(hOutWrite);
        CloseHandle(hErrWrite);

        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hOutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
            result.stdout_str.append(buffer, bytesRead);
        }
        while (ReadFile(hErrRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
            result.stderr_str.append(buffer, bytesRead);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exit_code = static_cast<int>(exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hOutRead);
        CloseHandle(hErrRead);
    } else {
        result.stderr_str = "Failed to launch process via CreateProcessA";
    }
#endif
    return result;
}

static void send_response(const json& resp) {
    std::string out = resp.dump();
    std::cout << out << "\n" << std::flush;
}

void run_mcp_server(const char* exec_path) {
    std::string self_bin = exec_path ? exec_path : "pcb2gcode";
    std::cerr << "[pcb2gcode MCP] Starting MCP server over stdio..." << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        try {
            json req = json::parse(line);

            if (!req.contains("jsonrpc") || req["jsonrpc"] != "2.0") {
                continue;
            }

            std::string method = req.value("method", "");
            json id = req.contains("id") ? req["id"] : json(nullptr);

            if (method == "notifications/initialized") {
                continue;
            }

            if (method == "initialize") {
                json resp;
                resp["jsonrpc"] = "2.0";
                resp["id"] = id;
                resp["result"] = {
                    {"protocolVersion", "2024-11-05"},
                    {"capabilities", {
                        {"tools", json::object()}
                    }},
                    {"serverInfo", {
                        {"name", "pcb2gcode"},
                        {"version", "3.0.4"}
                    }}
                };
                send_response(resp);
                continue;
            }

            if (method == "ping") {
                json resp;
                resp["jsonrpc"] = "2.0";
                resp["id"] = id;
                resp["result"] = json::object();
                send_response(resp);
                continue;
            }

            if (method == "tools/list") {
                json tools = json::array();

                json version_tool = {
                    {"name", "pcb2gcode_version"},
                    {"description", "Get version information of pcb2gcode."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", json::object()}
                    }}
                };

                json help_tool = {
                    {"name", "pcb2gcode_help"},
                    {"description", "Get available options and help documentation for pcb2gcode."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", json::object()}
                    }}
                };

                json run_tool = {
                    {"name", "pcb2gcode_run"},
                    {"description", "Run pcb2gcode with specified command-line arguments."},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"args", {
                                {"type", "array"},
                                {"items", {{"type", "string"}}},
                                {"description", "List of command line arguments (e.g. [\"--front\", \"board.gbr\", \"--zwork\", \"-0.1\"])."}
                            }},
                            {"cwd", {
                                {"type", "string"},
                                {"description", "Optional working directory for relative file paths."}
                            }}
                        }},
                        {"required", {"args"}}
                    }}
                };

                tools.push_back(version_tool);
                tools.push_back(help_tool);
                tools.push_back(run_tool);

                json resp;
                resp["jsonrpc"] = "2.0";
                resp["id"] = id;
                resp["result"] = {
                    {"tools", tools}
                };
                send_response(resp);
                continue;
            }

            if (method == "tools/call") {
                json params = req.value("params", json::object());
                std::string tool_name = params.value("name", "");
                json arguments = params.value("arguments", json::object());

                ProcessResult pr;
                if (tool_name == "pcb2gcode_version") {
                    pr = run_process(self_bin, {"--version"}, "");
                } else if (tool_name == "pcb2gcode_help") {
                    pr = run_process(self_bin, {"--help"}, "");
                } else if (tool_name == "pcb2gcode_run") {
                    std::vector<std::string> args;
                    if (arguments.contains("args") && arguments["args"].is_array()) {
                        for (const auto& item : arguments["args"]) {
                            args.push_back(item.get<std::string>());
                        }
                    }
                    std::string cwd = arguments.value("cwd", "");
                    pr = run_process(self_bin, args, cwd);
                } else {
                    json resp;
                    resp["jsonrpc"] = "2.0";
                    resp["id"] = id;
                    resp["error"] = {
                        {"code", -32601},
                        {"message", "Unknown tool: " + tool_name}
                    };
                    send_response(resp);
                    continue;
                }

                std::string output_text = "Exit code: " + std::to_string(pr.exit_code) + "\n\n";
                if (!pr.stdout_str.empty()) output_text += "STDOUT:\n" + pr.stdout_str + "\n";
                if (!pr.stderr_str.empty()) output_text += "STDERR:\n" + pr.stderr_str + "\n";

                json content_item = {
                    {"type", "text"},
                    {"text", output_text}
                };

                json resp;
                resp["jsonrpc"] = "2.0";
                resp["id"] = id;
                resp["result"] = {
                    {"content", json::array({content_item})},
                    {"isError", pr.exit_code != 0}
                };
                send_response(resp);
                continue;
            }

            json resp;
            resp["jsonrpc"] = "2.0";
            resp["id"] = id;
            resp["error"] = {
                {"code", -32601},
                {"message", "Method not found: " + method}
            };
            send_response(resp);

        } catch (const std::exception& e) {
            std::cerr << "[pcb2gcode MCP Error] " << e.what() << std::endl;
        }
    }
}
