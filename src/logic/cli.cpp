/**
 * @file cli.cpp
 * @brief Serial Command Line Interface — Phase 0 (S0-01).
 *
 * Implements lightweight command parsing and dispatch for serial commands.
 */

#include "cli.h"

#include "log_ext.h"

#include <Arduino.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr size_t CLI_MAX_COMMANDS = 32;
constexpr size_t CLI_MAX_ARGS = 8;

using CliHandler = void (*)(int argc, char* argv[]);

struct CliCommand {
    const char* cmd = nullptr;
    CliHandler handler = nullptr;
    const char* help_short = nullptr;
};

CliCommand s_cli_cmd_table[CLI_MAX_COMMANDS] = {};
size_t s_cli_cmd_count = 0;

void cliCmdHelp(int, char**) {
    cliPrintHelp();
}

void cliCmdVersion(int, char**) {
    Serial.printf("AgSteer Build: %s %s\n", __DATE__, __TIME__);
}

void cliCmdUptime(int, char**) {
    const uint32_t sec = millis() / 1000UL;
    const uint32_t h = sec / 3600UL;
    const uint32_t m = (sec % 3600UL) / 60UL;
    const uint32_t s = sec % 60UL;
    Serial.printf("Uptime: %luh %lum %lus\n",
                  static_cast<unsigned long>(h),
                  static_cast<unsigned long>(m),
                  static_cast<unsigned long>(s));
}

void cliCmdFree(int, char**) {
    Serial.printf("Heap: %lu KB free (largest: %lu KB) PSRAM: %lu KB free\n",
                  static_cast<unsigned long>(ESP.getFreeHeap() / 1024UL),
                  static_cast<unsigned long>(ESP.getMaxAllocHeap() / 1024UL),
                  static_cast<unsigned long>(ESP.getFreePsram() / 1024UL));
}

void cliCmdTasks(int, char**) {
#if (configUSE_TRACE_FACILITY == 1)
    static char task_list[1024];
    task_list[0] = '\0';
    vTaskList(task_list);
    Serial.println("Task         State Prio Stack Num");
    Serial.print(task_list);
#else
    Serial.printf("Tasks: %lu\n", static_cast<unsigned long>(uxTaskGetNumberOfTasks()));
    Serial.println("Task listing unavailable (configUSE_TRACE_FACILITY=0)");
#endif
}

void cliCmdRestart(int, char**) {
    Serial.println("Restarting...");
    Serial.flush();
    ESP.restart();
}

void cliCmdUnknown(const char* cmd) {
    Serial.printf("Unknown command: %s\n", cmd ? cmd : "");
    Serial.println("Type 'help' for available commands.");
}

void cliDispatch(int argc, char* argv[]) {
    if (argc <= 0 || argv == nullptr || argv[0] == nullptr) {
        return;
    }

    for (size_t i = 0; i < s_cli_cmd_count; ++i) {
        if (std::strcmp(argv[0], s_cli_cmd_table[i].cmd) == 0) {
            s_cli_cmd_table[i].handler(argc, argv);
            return;
        }
    }

    cliCmdUnknown(argv[0]);
}

}  // namespace

void cliInit(void) {
    s_cli_cmd_count = 0;
    (void)cliRegisterCommand("help", &cliCmdHelp, "List all commands");
    (void)cliRegisterCommand("version", &cliCmdVersion, "Show firmware build version");
    (void)cliRegisterCommand("uptime", &cliCmdUptime, "Show uptime (h m s)");
    (void)cliRegisterCommand("free", &cliCmdFree, "Show heap/PSRAM memory");
    (void)cliRegisterCommand("tasks", &cliCmdTasks, "Show FreeRTOS task list");
    (void)cliRegisterCommand("restart", &cliCmdRestart, "Restart ESP32");
}

bool cliRegisterCommand(const char* cmd,
                        void (*handler)(int argc, char* argv[]),
                        const char* help_short) {
    if (!cmd || !*cmd || !handler || !help_short) {
        return false;
    }

    for (size_t i = 0; i < s_cli_cmd_count; ++i) {
        if (std::strcmp(s_cli_cmd_table[i].cmd, cmd) == 0) {
            return false;
        }
    }

    if (s_cli_cmd_count >= CLI_MAX_COMMANDS) {
        return false;
    }

    s_cli_cmd_table[s_cli_cmd_count++] = {cmd, handler, help_short};
    return true;
}

void cliPrintHelp(void) {
    Serial.println("Available commands:");
    for (size_t i = 0; i < s_cli_cmd_count; ++i) {
        Serial.printf("  %-10s %s\n", s_cli_cmd_table[i].cmd, s_cli_cmd_table[i].help_short);
    }
    Serial.println("  log ...    Runtime log controls");
    Serial.println("  filter ... Runtime log file:line filter");
}

void cliProcessLine(const char* line) {
    if (!line) {
        return;
    }

    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    if (*line == '\0') {
        return;
    }

    if (std::strncmp(line, "log", 3) == 0 || std::strncmp(line, "filter", 6) == 0) {
        logProcessSerialCmd(line);
        return;
    }

    char buffer[128] = {0};
    std::strncpy(buffer, line, sizeof(buffer) - 1);

    char* argv[CLI_MAX_ARGS] = {};
    int argc = 0;

    char* token = std::strtok(buffer, " \t");
    while (token && argc < static_cast<int>(CLI_MAX_ARGS)) {
        argv[argc++] = token;
        token = std::strtok(nullptr, " \t");
    }

    cliDispatch(argc, argv);
}
