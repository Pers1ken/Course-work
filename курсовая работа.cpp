#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <winsock2.h>
#include "system_info.h"

#pragma comment(lib, "ws2_32.lib")

std::string loadHtml(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) return "<h1>Ошибка: Не найден " + filename + "</h1>";

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    size_t pos;
    std::string serverStatus = getServerStatus();
    std::string cpuLoad = getCpuUsage();
    std::string ramInfo = getMemoryStatus();
    std::string diskInfo = getDiskStatus();
    std::string serverIp = getServerAddress();
    std::string uptime = getUptime();

    while ((pos = content.find("{{SERVER_STATUS}}")) != std::string::npos)
        content.replace(pos, 16, serverStatus);
    while ((pos = content.find("{{CPU_LOAD}}")) != std::string::npos)
        content.replace(pos, 11, cpuLoad);
    while ((pos = content.find("{{RAM_INFO}}")) != std::string::npos)
        content.replace(pos, 11, ramInfo);
    while ((pos = content.find("{{DISK_INFO}}")) != std::string::npos)
        content.replace(pos, 12, diskInfo);
    while ((pos = content.find("{{SERVER_IP}}")) != std::string::npos)
        content.replace(pos, 12, serverIp);
    while ((pos = content.find("{{UPTIME}}")) != std::string::npos)
        content.replace(pos, 9, uptime);

    return content;
}

std::string getJsonStatus() {
    std::string uptime = getUptime();
    if (uptime.empty()) uptime = "Загрузка..."; 

    return "{\"cpu\":\"" + getCpuUsage() +
        "\", \"ram\":\"" + getMemoryStatus() +
        "\", \"disk\":\"" + getDiskStatus() +
        "\", \"ip\":\"" + getServerAddress() +
        "\", \"status\":\"" + getServerStatus() +
        "\", \"uptime\":\"" + uptime + "\"}";
}

int main() {
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket;
    sockaddr_in serverAddr, clientAddr;
    int clientSize = sizeof(clientAddr);

    WSAStartup(MAKEWORD(2, 2), &wsaData);
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 5);

    std::cout << "Сервер запущен на порту 8080\n";

    while (true) {
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        if (clientSocket == INVALID_SOCKET) continue;

        char buffer[1024] = { 0 };
        recv(clientSocket, buffer, sizeof(buffer), 0);

        std::string request(buffer);
        std::string response;

        if (request.find("GET /data") != std::string::npos) {
            response = "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json; charset=UTF-8\r\n"
                "Connection: close\r\n\r\n" + getJsonStatus();
        }
        else {
            response = "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Connection: close\r\n\r\n" + loadHtml("status.html");
        }

        send(clientSocket, response.c_str(), response.size(), 0);
        closesocket(clientSocket);
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}