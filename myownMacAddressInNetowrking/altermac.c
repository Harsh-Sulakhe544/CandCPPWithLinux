#define _GNU_SOURCE
#include <stdio.h> // printf() and scanf()
#include <string.h>
#include <errno.h> // error-number MACRO'S
#include <stdint.h> // for uint16_t and uint32_t == for all unsigned types
#include <stdlib.h> // srand() 
#include <assert.h>  // for handling errors 
#include <stdbool.h> // bolean data-type 
#include <sys/ioctl.h> // ioctl system calls 
#include <net/if.h> // for struct ifreq 
#include <netinet/in.h> // for IPPROTO_IP 

#include <net/if_arp.h> // for ARPHRD_ETHER == arp-header ethernet 

// data-types compatibility 
typedef unsigned char int8; // for storing 8bits (its just a name , u can give anything)
typedef unsigned short int int16; // 2 bytes == 16bits 
typedef unsigned int int32; // 4 bytes == 32 bits 
typedef unsigned long long int int64; // 8bytes

// structure for mac-addresses 
struct s_mac {
	int64 addr:48; // out of 64bits, use only 48 bits 	-- address
};
typedef struct s_mac Mac;

// generate random-mac addresses
bool chmac(int8*, Mac); 
Mac generatemac(void);
int main(int, char**);

Mac generatemac() {
	int64 a , b; // as of now 2 macc address 
	Mac mac;
	a = (long)random();
	b = (long)random();
	// 281474976710656 ==> 2 to the power of 48 
	mac.addr = ((a * b) % 281474976710656);
	
	return mac;
}

bool chmac(int8 *If, Mac mac) {
// SEARCH IF.H FILE 
// 	struct sockaddr *p;
	struct ifreq ir;
	
	int fd, ret; // file-desciptor  , return 
	int8 *addr;
	int16 size; 
	
//	char *_ = "";
	addr = (int8 *)"";
	
	// n the in.h file, the comment says: Dummy protocol for TCP. 
	// "https://stackoverflow.com/questions/24590818/what-is-the-difference-between-ipproto-ip-and-ipproto-raw"
	
	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	assert(fd > 0); // error handling FOR FILE
	
	/*
	size = sizeof(struct sockaddr) + (int16)strlen((char *)addr) + 1;
	p = (struct sockaddr *) malloc(size);
	assert(p);
	*/
	
	// char *strncpy(char *dest, const char *src, size_t n);

	strncpy(ir.ifr_ifrn.ifrn_name, (char *)If, (IFNAMSIZ-1));
	/*
	p->sa_family = ARPHRD_ETHER;
	strncpy(p->sa_data, (char *)addr, (size+1));
	ir.ifr_ifru.ifru_hwaddr = p;
	
	*/
	
	
	ir.ifr_ifru.ifru_hwaddr.sa_family = ARPHRD_ETHER;
	// since address is 12 digits and 6 :'s 0x :23:34:4e:3w:2w:1w
	
	// void *memcpy(void *dest, const void *src, size_t n);
	memcpy(ir.ifr_ifru.ifru_hwaddr.sa_data,&mac, 6); // just copy 6bytes 
	
	
	ret = ioctl(fd, SIOCSIFHWADDR, &mac);
//	free(p);
	close(fd);
	
	return (!ret) ? true : false; // ternary condition
}

int main(int argc , char *argv[]) {
	Mac mac;
	int8 *If; 
	
	if(argc < 2) {
		fprintf(stderr, "Usuage: %s INTERFACE \n", *argv); // file-printf
		return -1;
	}
	
	else {
		If = (int8 *)argv[1];
	}
	
	// get random process id 
	srand(getpid());
	mac = generatemac();
	printf("0x%llx \n", mac.addr); // llx means long-long-string ==> 
	return 0;
}
