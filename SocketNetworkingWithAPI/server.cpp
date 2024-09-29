// REFER main.cpp 

#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
// if errror ==> 
// #include <pthread.h>

void sendMessage(int client_socket) {
    char message[1024];
    while (true) {
        std::cout << "Server: Enter message to send to client: ";
        std::cin.getline(message, sizeof(message));
        send(client_socket, message, strlen(message), 0);
        if (strcmp(message, "exit") == 0) {
            exit(EXIT_SUCCESS);
        }
    }
}

void receiveMessage(int client_socket) {
    char buff[1024];
    while (true) {
        memset(buff, 0, sizeof(buff));
        int bytes_received = recv(client_socket, buff, sizeof(buff), 0);
        if (bytes_received <= 0) {
            std::cerr << "Client disconnected." << std::endl;
            exit(EXIT_SUCCESS);
        }
        std::cout << "		Message from client: " << buff << std::endl;
    }
}

int main() {
    std::cout << "Starting the server ...." << std::endl;
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Error creating the socket !" << std::endl;
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(4000);

    if (bind(server_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error binding the socket !" << std::endl;
        close(server_socket);
        return 1;
    }

    listen(server_socket, 1);
    int client_socket = accept(server_socket, NULL, NULL);
    std::cout << "Client connected." << std::endl;

    // Start threads for sending and receiving messages
    std::thread sender(sendMessage, client_socket);
    std::thread receiver(receiveMessage, client_socket);

    sender.join(); // Wait for the sender thread to finish
    receiver.join(); // Wait for the receiver thread to finish

    close(client_socket);
    close(server_socket);
    
    return 0;
}


