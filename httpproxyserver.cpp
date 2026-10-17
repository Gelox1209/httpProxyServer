#define WIN32_LEAN_AND_MEAN

#include <Winsock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <chrono>
#include <thread>

unsigned long nonBlockingEnabled = 1;
unsigned long blockingEnabled = 0;

SOCKET createSocket(int protocol)
{
    return socket(AF_INET, protocol == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM, protocol);
}

void gotoSleep() 
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

char* GetHostName(char* request, char* string) 
{
    char* host = strstr(request, "Host: ");
    for (int i = 0; host[i + 6] != '\r'; i++) 
    {
        string[i] = host[i + 6];
        string[i + 1] = '\0';
    }
    return string;
}

void HandleClient(SOCKET clientSocket) {
    //Receive the HTTP request from the client
    char buffer[4096];
    int bytesRead;
    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0) > 0)) 
    {
    }
    //Shutdown the client socket for receiving
    shutdown(clientSocket, SD_RECEIVE);
    //Parse the "Host" header to extract the hostname
    char string[4096] = { 0 };
    char* theHost = GetHostName(buffer, string);
    std::string hostHeader(theHost);
    //Resolve the IP address for the hostname
    struct sockaddr_in serverAddress;
    serverAddress.sin_port = htons(80);
    serverAddress.sin_family = AF_INET;
    int result = inet_pton(AF_INET, hostHeader.c_str(), &(serverAddress.sin_addr));
    if (result != 1) 
    {
        ADDRINFO hint;
        PADDRINFOA addrinfo = nullptr;
        hint.ai_family = AF_INET;
        if (getaddrinfo(hostHeader.c_str(), NULL, &hint, &addrinfo) != 0 || addrinfo == nullptr)
        {
            std::cout << "Failed to resolve the hostname." << std::endl;
            closesocket(clientSocket);
            return;
        }
        serverAddress.sin_addr = ((SOCKADDR_IN*)addrinfo->ai_addr)->sin_addr;
        freeaddrinfo(addrinfo);
    }
    //Build a socket and connect to the correct webserver
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 6);
    if (serverSocket == INVALID_SOCKET) 
    {
        std::cout << "Failed to create a server socket." << std::endl;
        closesocket(clientSocket);
        return;
    }
    //Connect to the server
    if (connect(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) 
    {
        std::cout << "Failed to connect to the web server." << std::endl;
        closesocket(clientSocket);
        closesocket(serverSocket);
        return;
    }
    //Send the entire HTTP request to the correct server
    send(serverSocket, buffer, strlen(buffer), 0);
    //Forward the response from the web server to the client
    while ((bytesRead = recv(serverSocket, buffer, sizeof(buffer), 0)) > 0) 
    {
        send(clientSocket, buffer, bytesRead, 0);
    }
    //Shutdown the web-server socket for both sending and receiving
    shutdown(serverSocket, SD_BOTH);
    //Shutdown the client socket for sending
    shutdown(clientSocket, SD_SEND);
    //Close both the client and webserver socket
    closesocket(serverSocket);
    closesocket(clientSocket);
}

int main(int argc, char* argv[])
{
    //Parse the argument (argv[1]) to determine the port you should bind to
    int port = atoi(argv[1]);
    if (port < 0 || port > 65535) 
    {
        std::cout << "Invalid port number. Port must be between 0 and 65535." << std::endl;
        return 1;
    }
    //Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cout << "Error in WSAStartup: " << WSAGetLastError() << std::endl;
        return 1;
    }
    //Construct a listening address, using INADDR_ANY
    SOCKADDR_IN serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);
    //Create TCP Socket
    SOCKET tcpSocketListen = createSocket(IPPROTO_TCP);
    if (tcpSocketListen == INVALID_SOCKET)
    {
        std::cout << "Error creating socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }
    //Set listening socket to non blocking
    if (ioctlsocket(tcpSocketListen, FIONBIO, &nonBlockingEnabled) != 0) 
    {
        std::cout << "Error making socket non blocking: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }
    //Bind the listening socket to the listening address
    if (bind(tcpSocketListen, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) 
    {
        std::cout << "Failed to bind the listening socket." << std::endl;
        closesocket(tcpSocketListen);
        WSACleanup();
        return 1;
    }
    //Listen on the TCP socket
    if (listen(tcpSocketListen, SOMAXCONN) == SOCKET_ERROR) 
    {
        std::cout << "Failed to listen on the listening socket." << std::endl;
        closesocket(tcpSocketListen);
        WSACleanup();
        return 1;
    }
    //Print the port for assignment
    std::cout << "Listening on port " << port << std::endl;
    //Infinite loop to accept clients
    while (true) 
    {
        SOCKET clientSocket = accept(tcpSocketListen, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            //Handle the client request on a separate thread
            std::thread clientThread(HandleClient, clientSocket);
            clientThread.detach();  // Detach the thread to run it independently
        }
        //Wait for 100 milliseconds before continuing
        Sleep(100);
    }

    //Close the socket
    shutdown(tcpSocketListen, SD_BOTH);
    
    if (closesocket(tcpSocketListen) != 0)
    {
        std::cout << "Error closing socket : " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }
    //Shutdown Winsock
    WSACleanup();
}