#include <iostream>
#include <string>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

int main(int argc, char *argv[]) {

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <ON-IP-address>\n";
        return 1;
    }

    string serverIP = argv[1];

    // Create TCP socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket < 0) {
        cerr << "Error: Could not create socket\n";
        return 1;
    }

    // ON address
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(5000);

    if (inet_pton(AF_INET,
                  serverIP.c_str(),
                  &serverAddress.sin_addr) <= 0) {

        cerr << "Error: Invalid ON IP address\n";
        close(clientSocket);
        return 1;
    }

    // Connect to ON
    if (connect(clientSocket,
                (struct sockaddr *)&serverAddress,
                sizeof(serverAddress)) < 0) {

        cerr << "Error: Could not connect to ON\n";
        close(clientSocket);
        return 1;
    }

    cout << "Connected to Oracle Node at "
         << serverIP << ":5000\n";

    // Keep connection alive for now
    while (true) {
        // Nothing yet
    }

    close(clientSocket);

    return 0;
}