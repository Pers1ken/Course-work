#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <string>

std::string wstringToUtf8(const std::wstring& wstr);
std::string getServerStatus();
std::string getCpuUsage();
std::string getMemoryStatus();
std::string getDiskStatus();
std::string getServerAddress();
std::string getUptime();
void logSystemData(const std::string& data);
void sendTelegramNotification(const std::string& message);

#endif