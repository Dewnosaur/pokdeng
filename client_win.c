#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

int received_card1 = 0, received_card2 = 0;

DWORD WINAPI recv_thread(LPVOID sock_ptr) {
    SOCKET sock = *(SOCKET *)sock_ptr;
    char buffer[1024];

    while (1) {
        char buffer[128] = {0};
        int len = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (len <= 0) break;
        buffer[len] = '\0';
        printf("%s", buffer);

        if (strstr(buffer, "WAIT_FOR_DRAW")) {
            printf("Do you want to draw a third card? (y/n): ");
            char choice[8];
            fgets(choice, sizeof(choice), stdin);
            if (choice[0] == 'y' || choice[0] == 'Y') {
                send(sock, "DRAW3", 5, 0);
            } else {
                send(sock, "NO", 2, 0);
            }
        }
    }

    return 0;
}

int main() {
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in server_addr;
    HANDLE hThread;

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("[Client]: Failed to connect to server.\n");
        return 1;
    }

    printf("[Client]: Connected to server.\n");

    hThread = CreateThread(NULL, 0, recv_thread, &sock, 0, NULL);

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    closesocket(sock);
    WSACleanup();
    return 0;
}
