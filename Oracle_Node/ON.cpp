#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

#define PORT 5000
#define CONNECT 1
#define ON_VN 2

struct VirtualNode {
    int socket;
    char name;
    uint32_t ip;
    uint16_t port;
};

bool loadConfig(const string& file, vector<vector<int>>& g) {
    ifstream f(file);
    if (!f) return false;

    vector<vector<int>> rows;
    string line;

    while (getline(f, line)) {
        size_t p = line.find('#');
        if (p != string::npos) line.erase(p);

        stringstream ss(line);
        vector<int> row;
        int x;

        while (ss >> x) row.push_back(x);
        if (!row.empty()) rows.push_back(row);
    }

    if (rows.empty()) return false;

    int N = rows.size() + 1;
    g.assign(N, vector<int>(N, -1));

    for (int i = 0; i < N; i++)
        g[i][i] = 0;

    for (int i = 0; i < N - 1; i++) {
        if (rows[i].size() != N - i - 1)
            return false;

        for (int j = i + 1; j < N; j++) {
            int c = rows[i][j - i - 1];
            if (c != -1 && c <= 0) return false;

            g[i][j] = g[j][i] = c;
        }
    }

    return true;
}

void sendLinkstate(const vector<VirtualNode>& nodes,
                   const vector<vector<int>>& g) {

    for (int i = 0; i < nodes.size(); i++) {

        int count = 1;
        for (int j = 0; j < nodes.size(); j++)
            if (i != j && g[i][j] >= 0)
                count++;

        char header[2] = {ON_VN, (char)count};
        send(nodes[i].socket, header, 2, 0);

        char buf[9];

        // Own entry
        buf[0] = nodes[i].name;

        uint32_t ip = htonl(nodes[i].ip);
        uint16_t port = htons(nodes[i].port);
        uint16_t cost = 0;

        memcpy(buf + 1, &ip, 4);
        memcpy(buf + 5, &port, 2);
        memcpy(buf + 7, &cost, 2);

        send(nodes[i].socket, buf, 9, 0);

        // Neighbours
        for (int j = 0; j < nodes.size(); j++) {
            if (i == j || g[i][j] < 0) continue;

            buf[0] = nodes[j].name;

            ip = htonl(nodes[j].ip);
            port = htons(nodes[j].port);
            cost = htons(g[i][j]);

            memcpy(buf + 1, &ip, 4);
            memcpy(buf + 5, &port, 2);
            memcpy(buf + 7, &cost, 2);

            send(nodes[i].socket, buf, 9, 0);
        }
    }
}

int main(int argc, char* argv[]) {

    signal(SIGPIPE, SIG_IGN);

    if (argc != 3) {
        cout << "Usage: " << argv[0] << " CONFIG_FILE SERVER_IP\n";
        return -1;
    }

    vector<vector<int>> graph;

    if (!loadConfig(argv[1], graph))
        return -1;

    int N = graph.size();

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0){
        perror("socket");
        return -1;
    };

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(argv[2]);

    if (bind(server, (sockaddr*)&addr, sizeof(addr)) < 0){
        perror("bind");
        return -1;
    }

    if (listen(server, 5) < 0) {
        perror("listen");
        return -1;
    }
    cout << "Listening for VN connections on port " << PORT << "...\n\n";

    vector<VirtualNode> nodes;

    while (nodes.size() < N) {

        sockaddr_in client{};
        socklen_t len = sizeof(client);

        int s = accept(server, (sockaddr*)&client, &len);
        if (s < 0) continue;

        char buf[7];

        if (read(s, buf, 7) != 7) {
            close(s);
            continue;
        }

        if (buf[0] != CONNECT) {
            close(s);
            continue;
        }

        uint32_t ip;
        uint16_t port;

        memcpy(&ip, buf + 1, 4);
        memcpy(&port, buf + 5, 2);

        VirtualNode node;
        node.socket = s;
        node.name = 'A' + nodes.size();
        node.ip = ntohl(ip);
        node.port = ntohs(port);

        nodes.push_back(node);

        // cout << "Connected: " << node.name << "\n";
        cout << "VN " << node.name << " connected successfully.\n";
    }

    cout << "\nAll " << N << " VNs connected.\n";
    cout << "Sending initial LINK_STATE...\n\n";

    sendLinkstate(nodes, graph);

    cout << "LINK_STATE update complete.\n";

    while (true) {

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server, &fds);

        timeval tv{};
        tv.tv_sec = 1;

        int ret = select(server+1, &fds, nullptr, nullptr, &tv);

        if (ret > 0 && FD_ISSET(server, &fds)) {
            int s = accept(server, nullptr, nullptr);
            if (s >= 0) {
                cout << "Extra VN connected. Ignoring it.\n\n";
                close(s);
            }
        }

        vector<vector<int>> newGraph;

        if (loadConfig(argv[1], newGraph) &&
            newGraph.size() == graph.size() &&
            newGraph != graph) {

            graph = newGraph;

            cout << "Configuration changed.\n";
            cout << "Updating LINK_STATE for all VNs...\n\n";

            sendLinkstate(nodes, graph);

            cout << "LINK_STATE update complete.\n\n";
        }

        sleep(1);
    }
}