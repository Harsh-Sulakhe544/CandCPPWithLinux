// for sockets 
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

#include <cstdlib> // for EXIT_FAILURE and EXIT_SUCCESS
#include <cstdio> // for perror 

#include <iostream> // for cin, cout 

#include <netinet/in.h> // for sockaddr_in or sockaddr_on and inet_addr()
#include <netinet/in.h> // for inet_addr() 

#include <arpa/inet.h> // htons()

int main() {
	// int socket(int domain, int type, int protocol);
	int sockfd = socket(AF_INET, SOCK_STREAM,0); // 0 MEANS DEFAULT PROTOCOL , fd==> file-descriptor
	
	// check if it is not created 
	if(sockfd == -1) {
		std::cout<<"hello world" << std::endl;
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	// it returns 3 -- successfuly - created socket 
	std::cout<<"socket not executed" << sockfd << std::endl;
	
	sockaddr_in addr; 
	addr.sin_family = AF_INET; 
	addr.sin_addr.s_addr = INADDR_ANY; // Bind to any available 	interface[0.0.0.0]
	addr.sin_port = htons(4000); // Bind to port 4000
	
	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	
	if (listen(sockfd, 5) == -1) { // Allow up to 5 pending connections
		perror("listen");
		exit(EXIT_FAILURE);
	}
	
	// accepting the connection from the server 
	// int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	sockaddr_in client_addr;
	socklen_t client_addrlen = sizeof(client_addr); 
	int client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addrlen);
	if(client_sockfd == -1) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	
	
}

