// REFER : server.cpp 

#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <arpa/inet.h>
// if errror ==> 
// #include <pthread.h>

void receiveMessage(int client_socket) {
    char buff[1024];
    while (true) {
        memset(buff, 0, sizeof(buff));
        int bytes_received = recv(client_socket, buff, sizeof(buff), 0);
        if (bytes_received <= 0) {
            std::cerr << "Server disconnected." << std::endl;
            exit(EXIT_SUCCESS);
        }
        std::cout << "		Message from server: " << buff << std::endl;
    }
}

void sendMessage(int client_socket) {
    char mess[1024];
    while (true) {
        std::cout << "Client: Enter the message to send: ";
        std::cin.getline(mess, sizeof(mess));
        send(client_socket, mess, strlen(mess), 0);
        if (strcmp(mess, "exit") == 0) {
            exit(EXIT_SUCCESS);
            
        }
    }
}

int main() {
    std::cout << "Starting the Client ..." << std::endl;
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Error creating the socket ..." << std::endl;
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(4000);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Change to server IP if needed

    if (connect(client_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error connecting to the server ..." << std::endl;
        return 1;
    }

    // Start threads for sending and receiving messages
    std::thread receiver(receiveMessage, client_socket);
    std::thread sender(sendMessage, client_socket);

    sender.join(); // Wait for the sender thread to finish
    receiver.join(); // Wait for the receiver thread to finish

    close(client_socket);
    return 0;
}

