#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <cstdint>
#include <cerrno>
#include <sys/stat.h>
#include <ctime>
#include <csignal>
#include <ifaddrs.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

// Message types must match headers.py.
constexpr uint8_t CONNECT = 1;
constexpr uint8_t ON_VN = 2;
constexpr uint8_t VN_VN = 3; // Defined for completeness; Oracle does not send LSPs.

struct VirtualNode {
    int socket;
    char name;
    uint32_t ip;       
    uint16_t udpPort;  
};

#pragma pack(push, 1)
struct ConnectMessage {
    uint8_t type;      // Python format: B
    uint32_t ip;       // Python format: 4s
    uint16_t udpPort;  // Python format: H
};

struct LinkStateHeader {
    uint8_t type;   // Python LINK_HEADER_FORMAT: B
    uint8_t count;  // Python LINK_HEADER_FORMAT: B
};

struct LinkStateEntry {
    char nodeName;      // Python NODE_FORMAT: c
    uint32_t ip;        // Python NODE_FORMAT: 4s
    uint16_t udpPort;   // Python NODE_FORMAT: H
    uint16_t cost;      // Python NODE_FORMAT: H
};
#pragma pack(pop)

static_assert(sizeof(ConnectMessage) == 7, "CONNECT must be 7 bytes");
static_assert(sizeof(LinkStateHeader) == 2, "LINK_STATE header must be 2 bytes");
static_assert(sizeof(LinkStateEntry) == 9, "LINK_STATE entry must be 9 bytes");

bool sendAll(int socketFd, const void* buffer, size_t length) {
    const char* data = static_cast<const char*>(buffer);
    size_t totalSent = 0;

    while (totalSent < length) {
        ssize_t sent = send(socketFd, data + totalSent,
                            length - totalSent, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (sent == 0) {
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }

    return true;
}

bool receiveAll(int socketFd, void* buffer, size_t length) {
    char* data = static_cast<char*>(buffer);
    size_t totalReceived = 0;

    while (totalReceived < length) {
        ssize_t received = recv(socketFd, data + totalReceived,
                                length - totalReceived, 0);
        if (received <= 0) {
            return false;
        }
        totalReceived += static_cast<size_t>(received);
    }

    return true;
}

void sendLinkstate(const vector<VirtualNode>& nodes,
                   const vector<vector<int>>& graph) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].socket < 0) {
            continue;
        }

        vector<LinkStateEntry> entries;

        LinkStateEntry self{};
        self.nodeName = nodes[i].name;
        self.ip = htonl(nodes[i].ip);
        self.udpPort = htons(nodes[i].udpPort);
        self.cost = htons(0);
        entries.push_back(self);

        if (i < graph.size()) {
            for (size_t j = 0; j < graph.size() && j < nodes.size(); ++j) {
                if (i == j || graph[i][j] == -1) {
                    continue;
                }

                LinkStateEntry entry{};
                entry.nodeName = nodes[j].name;
                entry.ip = htonl(nodes[j].ip);
                entry.udpPort = htons(nodes[j].udpPort);

                if (graph[i][j] < 0 || graph[i][j] > 65535) {
                    cerr << "Invalid link cost in configuration\n";
                    continue;
                }

                entry.cost = htons(static_cast<uint16_t>(graph[i][j]));
                entries.push_back(entry);
            }
    }

        if (entries.size() > 26) {
            cerr << "Too many LINK_STATE records\n";
            continue;
        }

        // Must match Python LINK_HEADER_FORMAT = "!BB".
        LinkStateHeader header{};
        header.type = ON_VN;
        header.count = static_cast<uint8_t>(entries.size());

        if (!sendAll(nodes[i].socket, &header, sizeof(header))) {
            cerr << "Failed to send LINK_STATE header to node "
                 << nodes[i].name << "\n";
            continue;
        }

        for (const auto& entry : entries) {
            if (!sendAll(nodes[i].socket, &entry, sizeof(entry))) {
                cerr << "Failed to send LINK_STATE entry to node "
                     << nodes[i].name << "\n";
                break;
            }
        }

        cout << "LINK_STATE sent to node " << nodes[i].name << "\n";
    }
}

bool loadConfig(const string& filename, vector<vector<int>>& graph) {
    ifstream configFile(filename);

    if (!configFile.is_open()) {
        cerr << "Error: Could not open config file\n";
        return false;
    }

    vector<vector<int>> rows;
    string line;

    while (getline(configFile, line)) {
        size_t commentPos = line.find('#');
        if (commentPos != string::npos) {
            line = line.substr(0, commentPos);
        }

        stringstream ss(line);
        vector<int> row;
        int value;

        while (ss >> value) {
            row.push_back(value);
        }

        if (!row.empty()) {
            rows.push_back(row);
        }
    }

    if (rows.empty()) {
        cerr << "Error: Configuration file contains no link costs\n";
        return false;
    }

    const size_t N = rows.size() + 1;
    if (N > 26) {
        cerr << "Error: At most 26 nodes are supported\n";
        return false;
    }

    // The config is upper-triangular: row i must contain N-i-1 costs.
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].size() != N - i - 1) {
            cerr << "Error: Incorrect number of costs in config row "
                 << i + 1 << "\n";
            return false;
        }

        for (int cost : rows[i]) {
            if (cost != -1 && cost <= 0) {
                cerr << "Error: Link costs must be positive or -1\n";
                return false;
            }
        }
    }

    graph.assign(N, vector<int>(N, -1));

    for (size_t i = 0; i < N; ++i) {
        graph[i][i] = 0;
    }

    for (size_t i = 0; i < N - 1; ++i) {
        for (size_t j = i + 1; j < N; ++j) {
            int cost = rows[i][j - i - 1];
            graph[i][j] = cost;
            graph[j][i] = cost;
        }
    }

    return true;
}

// helper function for getting own pc's ip address directly from os by including ifaddrs.h 
string getLocalNonLoopbackIP() {
    struct ifaddrs *interfaces = nullptr, *ifa = nullptr;
    string ipAddress = "";
    if (getifaddrs(&interfaces) == 0) {
        for (ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            char ip[INET_ADDRSTRLEN];
            void* addrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addrPtr, ip, INET_ADDRSTRLEN);
            string ipStr(ip);
            if (ipStr != "127.0.0.1" && ipStr != "0.0.0.0") {
                ipAddress = ipStr;
                break;
            }
        }
        freeifaddrs(interfaces);
    }
    return ipAddress;
}



int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);

    if (argc < 2 || argc > 3) {
        cerr << "Usage: " << argv[0] << " CONFIG_FILE [SERVER_IP]\n";
        return 1;
    }

    const string configFileName = argv[1];
    string serverIP = (argc == 3) ? argv[2] : getLocalNonLoopbackIP();
    if (serverIP.empty()) {
        cerr << "Error: Could not determine non-loopback IP. Please pass SERVER_IP.\n";
        return 1;
    }


    vector<vector<int>> graph;
    if (!loadConfig(configFileName, graph)) {
        return 1;
    }

    const size_t N = graph.size();
    cout << "Number of nodes: " << N << "\n";

    struct stat configStat{};
    if (stat(configFileName.c_str(), &configStat) != 0) {
        cerr << "Error: Could not stat config file\n";
        return 1;
    }
    time_t lastModified = configStat.st_mtime;

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Socket creation failed\n";
        return 1;
    }

    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) < 0) {
        cerr << "Error: setsockopt failed\n";
        close(serverSocket);
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(5000);

    if (inet_pton(AF_INET, serverIP.c_str(),
                  &serverAddress.sin_addr) != 1) {
        cerr << "Error: Invalid IPv4 address\n";
        close(serverSocket);
        return 1;
    }

    if (::bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress),
             sizeof(serverAddress)) < 0) {
        cerr << "Error: Could not bind TCP socket\n";
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 10) < 0) {
        cerr << "Error: listen failed\n";
        close(serverSocket);
        return 1;
    }

    cout << "Oracle Node listening on " << serverIP << ":5000\n";

    vector<VirtualNode> nodes;
    size_t nodeCount = 0;

    while (nodeCount < N) {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);

        int clientSocket = accept(
            serverSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientLength
        );

        if (clientSocket < 0) {
            cerr << "accept() failed\n";
            continue;
        }

        ConnectMessage msg{};
        if (!receiveAll(clientSocket, &msg, sizeof(msg))) {
            cerr << "Failed to receive CONNECT message\n";
            close(clientSocket);
            continue;
        }

        if (msg.type != CONNECT) {
            cerr << "Invalid message type from connecting VN\n";
            close(clientSocket);
            continue;
        }

        VirtualNode node{};
        node.socket = clientSocket;
        node.name = static_cast<char>('A' + nodeCount);
        node.ip = ntohl(msg.ip);
        node.udpPort = ntohs(msg.udpPort);
        nodes.push_back(node);

        char ipString[INET_ADDRSTRLEN];
        uint32_t ipNetworkOrder = htonl(nodes.back().ip);
        if (inet_ntop(AF_INET, &ipNetworkOrder,
                      ipString, sizeof(ipString)) == nullptr) {
            cerr << "Could not format VN IP address\n";
            close(clientSocket);
            nodes.pop_back();
            continue;
        }

        cout << "VN connected as node " << node.name << "\n";
        cout << "  IP: " << ipString << "\n";
        cout << "  UDP port: " << nodes.back().udpPort << "\n";
        ++nodeCount;
    }

    cout << "\nAll virtual nodes connected.\n";
    cout << "Sending LINK_STATE messages...\n\n";
    sendLinkstate(nodes, graph);

    cout << "\nOracle is now monitoring the config file...\n";

   while (true) {
        // Check for additional incoming VN connections (> N) with a 1-second timeout
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(serverSocket, &readFds);
        struct timeval tv{1, 0}; // 1 second timeout acts as our loop sleep

        int activity = select(serverSocket + 1, &readFds, nullptr, nullptr, &tv);
        if (activity > 0 && FD_ISSET(serverSocket, &readFds)) {
            sockaddr_in clientAddress{};
            socklen_t clientLength = sizeof(clientAddress);
            int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientLength);
            if (clientSocket >= 0) {
                ConnectMessage msg{};
                if (receiveAll(clientSocket, &msg, sizeof(msg)) && msg.type == CONNECT && nodeCount < 26) {
                    VirtualNode extraNode{};
                    extraNode.socket = clientSocket;
                    extraNode.name = static_cast<char>('A' + nodeCount);
                    extraNode.ip = ntohl(msg.ip);
                    extraNode.udpPort = ntohs(msg.udpPort);
                    nodes.push_back(extraNode);
                    ++nodeCount;

                    cout << "Extra VN connected as disconnected island node " << extraNode.name << "\n";

                    // Send LINK_STATE with only its own record (cost 0, no neighbors)
                    LinkStateHeader header{ON_VN, 1};
                    LinkStateEntry selfEntry{extraNode.name, htonl(extraNode.ip), htons(extraNode.udpPort), htons(0)};
                    sendAll(clientSocket, &header, sizeof(header));
                    sendAll(clientSocket, &selfEntry, sizeof(selfEntry));
                } else {
                    close(clientSocket);
                }
            }
        }

        struct stat currentStat{};
        if (stat(configFileName.c_str(), &currentStat) != 0) {
            cerr << "Error: Could not stat config file\n";
            continue;
        }

        if (currentStat.st_mtime != lastModified) {
            cout << "\nConfiguration file changed!\n";

            vector<vector<int>> newGraph;
            if (!loadConfig(configFileName, newGraph)) {
                cerr << "Error: Could not reload config; keeping old graph\n";
                continue;
            }

            if (newGraph.size() != graph.size()) {
                cerr << "Error: Number of nodes cannot change during emulation\n";
                continue;
            }

            graph = newGraph;
            lastModified = currentStat.st_mtime;

            cout << "Sending updated LINK_STATE messages...\n\n";
            sendLinkstate(nodes, graph);
        }
    }

    close(serverSocket);
    return 0;
}
