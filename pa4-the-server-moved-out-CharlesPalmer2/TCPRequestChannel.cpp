#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "TCPRequestChannel.h"

using namespace std;


TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const std::string _port_no) {
    if (_ip_address == ""){ // server implementation
        struct sockaddr_in sock_addr;
        // create a socket on the specified port
        //   - specify domain, type, and protocol
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            cout << "Error creating server socket.\n";
            exit(1);
        }
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
            cout << "setsockopt failed.\n";
            exit(1);
        }
        memset(&sock_addr, 0, sizeof(sock_addr));    // zero structure out
        sock_addr.sin_family = AF_INET; // match socket call
        sock_addr.sin_addr.s_addr = INADDR_ANY;  // bind to any address
        sock_addr.sin_port = htons(stoi(_port_no)); // specify port
        // bind the socket to address; sets up listening
        if ((bind(sockfd, (struct sockaddr *) &sock_addr, sizeof(sock_addr))) < 0){
            cout << "Error binding server socket. \n";
            exit(1);
        }
        // mark socket as listening
        if (listen(sockfd, 20) < 0){
            cout << "Error in server listening\n";
            exit(1);
        }
    }
    else{   // client implementation
        struct sockaddr_in sock_addr;
        // create a socket on the specified port
        //   - specify domain, type, and protocol
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            cout << "Error creating client socket.\n";
            exit(1);
        }
        memset(&sock_addr, 0, sizeof(sock_addr));    // zero structure out
        sock_addr.sin_family = AF_INET; // match socket call
        sock_addr.sin_port = htons(stoi(_port_no)); // specify port
        if (inet_pton(AF_INET, _ip_address.c_str(), &(sock_addr.sin_addr)) <= 0){    // set ip address
            cout << "Invalid Address. \n";
            exit(1);
        }
        // connect socket to the IP address of the server
        if ((connect(sockfd, (struct sockaddr*)&sock_addr, sizeof(sock_addr))) < 0){
            cout << "Error connecting client.\n";
            exit(1);
        }
    }
}

TCPRequestChannel::TCPRequestChannel (int _sockfd) {
    sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel () {
    // close the sockfd
    close(sockfd);
}

int TCPRequestChannel::accept_conn () {
    // struct sockaddr_storage
    struct sockaddr_storage sock_stor;
    socklen_t soc_size = sizeof(sock_stor);
    // implementing accept(...) - retval the sockfd of client
    int nsock = accept(sockfd, (struct sockaddr*)&sock_stor, &soc_size);
    if (nsock < 0){
        cout << "Error at TCPRequest::accept_conn().\n";
        exit(1);
    }
    return nsock;
}

// read/write, recv/send
int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    return recv(sockfd, msgbuf, msgsize, 0);
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    return send(sockfd, msgbuf, msgsize, 0);
}
