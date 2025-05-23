﻿#include "system_info.h"
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <string>
#include <curl/curl.h> 
#include <fstream>
#include <ctime>

static ULONGLONG prevIdleTime = 0, prevKernelTime = 0, prevUserTime = 0;
static auto startTime = std::chrono::steady_clock::now();


std::string getServerAddress() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) return "Неизвестно";

    struct hostent* host = gethostbyname(hostname);
    if (!host || !host->h_addr_list[0]) return "Неизвестно";

    struct in_addr addr;
    memcpy(&addr, host->h_addr_list[0], sizeof(struct in_addr));
    return inet_ntoa(addr);
}

std::string getUptime() {
    static auto startTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int sec = seconds % 60;

    std::ostringstream uptime;
    uptime << hours << "h " << minutes << "m " << sec << "s";
    return uptime.str();
}

std::string getServerStatus() {
    std::wstring status = L"Сервер работает";
    std::string utf8Status = wstringToUtf8(status);


    return utf8Status;
}


std::string wstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);

    if (size_needed <= 1) return "";

    std::string strTo(size_needed - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &strTo[0], size_needed, nullptr, nullptr);

    strTo.erase(std::find(strTo.begin(), strTo.end(), '\0'), strTo.end());

    return strTo;
}


double calculateCpuUsage(FILETIME idleTime, FILETIME kernelTime, FILETIME userTime) {
    static ULONGLONG prevIdleTime = 0, prevKernelTime = 0, prevUserTime = 0;

    ULONGLONG idle = ((ULONGLONG)idleTime.dwHighDateTime << 32) | idleTime.dwLowDateTime;
    ULONGLONG kernel = ((ULONGLONG)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;
    ULONGLONG user = ((ULONGLONG)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime;

    ULONGLONG totalSystem = (kernel - prevKernelTime) + (user - prevUserTime);
    ULONGLONG totalIdle = idle - prevIdleTime;

    prevIdleTime = idle;
    prevKernelTime = kernel;
    prevUserTime = user;

    if (totalSystem == 0) return 0.0;
    return 100.0 * (1.0 - (double)totalIdle / totalSystem);
}

std::string getCpuUsage() {
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        ULONGLONG idle = ((ULONGLONG)idleTime.dwHighDateTime << 32) | idleTime.dwLowDateTime;
        ULONGLONG kernel = ((ULONGLONG)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;
        ULONGLONG user = ((ULONGLONG)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime;

        ULONGLONG totalSystem = (kernel - prevKernelTime) + (user - prevUserTime);
        ULONGLONG totalIdle = idle - prevIdleTime;

        prevIdleTime = idle;
        prevKernelTime = kernel;
        prevUserTime = user;

        double cpuUsage = totalSystem ? 100.0 * (1.0 - (double)totalIdle / totalSystem) : 0.0;
        cpuUsage *= 2.0;
        if (cpuUsage > 100.0) cpuUsage = 100.0;

        std::ostringstream oss;
        oss.precision(2);
        oss << std::fixed << cpuUsage << " / 100%";

        return oss.str();
    }
    return "Ошибка CPU";
}



std::string getMemoryStatus() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    DWORDLONG totalPhysMem = memInfo.ullTotalPhys / (1024 * 1024 * 1024);
    DWORDLONG usedPhysMem = (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024 * 1024);

    std::ostringstream oss;
    oss << usedPhysMem << " GB / " << totalPhysMem << " GB";
    return oss.str();
}

std::string getDiskStatus() {
    ULARGE_INTEGER freeBytes, totalBytes, usedBytes;
    GetDiskFreeSpaceEx(NULL, &freeBytes, &totalBytes, &usedBytes);

    DWORDLONG totalDisk = totalBytes.QuadPart / (1024 * 1024 * 1024);
    DWORDLONG usedDisk = (totalBytes.QuadPart - freeBytes.QuadPart) / (1024 * 1024 * 1024);

    std::ostringstream oss;
    oss << usedDisk << " GB / " << totalDisk << " GB";
    return oss.str();
}

void logSystemData(const std::string& data) {
    std::ofstream logFile("server_log.txt", std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);

        std::tm tm;
        localtime_s(&tm, &now_time);

        char timeStr[20];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm);

        logFile << "[" << timeStr << "] " << data << "\n";
        logFile.close();
    }
    else {
        std::cerr << "Ошибка открытия файла лога\n";
    }
}

void sendTelegramNotification(const std::string& message) {
    const std::string token = "";
    const std::string chat_id = "";

    const std::string url = "https://api.telegram.org/bot" + token + "/sendMessage";

    const std::string payload = "chat_id=" + chat_id + "&text=" + message;

    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "Ошибка отправки сообщения в Telegram: " << curl_easy_strerror(res) << std::endl;
        }
        else {
            std::cout << "Сообщение успешно отправлено!" << std::endl;
        }

        curl_easy_cleanup(curl);
    }
    else {
        std::cerr << "Ошибка инициализации CURL!" << std::endl;
    }
}