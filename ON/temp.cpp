#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

int main(int argc, char *argv[]) {

    // ------------------------------------------------------------
    // 1. Check command-line arguments
    // ------------------------------------------------------------

    if (argc != 3) {
        cerr << "Usage: " << argv[0]
             << " <config-file> <ON-IP-address>\n";
        return 1;
    }

    string configFile = argv[1];
    string serverIP = argv[2];


    // ------------------------------------------------------------
    // 2. Open configuration file
    // ------------------------------------------------------------

    ifstream file(configFile);

    if (!file) {
        cerr << "Error: Could not open configuration file\n";
        return 1;
    }


    // ------------------------------------------------------------
    // 3. Read the triangular configuration
    // ------------------------------------------------------------

    vector<vector<int>> rows;

    string line;

    while (getline(file, line)) {

        // Remove comments.
        // Everything after '#' is ignored.
        size_t commentPos = line.find('#');

        if (commentPos != string::npos) {
            line = line.substr(0, commentPos);
        }

        // Extract integers from the line.
        stringstream ss(line);

        vector<int> row;
        int value;

        while (ss >> value) {
            row.push_back(value);
        }

        // Ignore empty lines.
        if (!row.empty()) {
            rows.push_back(row);
        }
    }

    file.close();


    // ------------------------------------------------------------
    // 4. Determine number of nodes
    // ------------------------------------------------------------

    int N = rows.size() + 1;

    cout << "Number of nodes: " << N << "\n\n";


    // ------------------------------------------------------------
    // 5. Construct complete adjacency matrix
    // ------------------------------------------------------------

    vector<vector<int>> graph(N, vector<int>(N, -1));

    // Cost from a node to itself is 0.
    for (int i = 0; i < N; i++) {
        graph[i][i] = 0;
    }

    // Configuration contains only the upper triangle.
    for (int i = 0; i < N - 1; i++) {

        for (int j = i + 1; j < N; j++) {

            int cost = rows[i][j - i - 1];

            graph[i][j] = cost;
            graph[j][i] = cost;
        }
    }


    // ------------------------------------------------------------
    // 6. Print the graph
    // ------------------------------------------------------------

    cout << "Graph:\n\n";

    cout << "    ";

    for (int i = 0; i < N; i++) {
        cout << char('A' + i) << "   ";
    }

    cout << "\n";

    for (int i = 0; i < N; i++) {

        cout << char('A' + i) << "   ";

        for (int j = 0; j < N; j++) {
            cout << graph[i][j] << "   ";
        }

        cout << "\n";
    }


    // ------------------------------------------------------------
    // 7. Create TCP socket
    // ------------------------------------------------------------

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0) {
        cerr << "Error: Could not create TCP socket\n";
        return 1;
    }


    // ------------------------------------------------------------
    // 8. Allow quick restart of the server
    // ------------------------------------------------------------

    int reuse = 1;

    if (setsockopt(serverSocket,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &reuse,
                   sizeof(reuse)) < 0) {

        cerr << "Error: setsockopt failed\n";
        close(serverSocket);
        return 1;
    }


    // ------------------------------------------------------------
    // 9. Configure server address
    // ------------------------------------------------------------

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(5000);

    if (inet_pton(AF_INET,
                  serverIP.c_str(),
                  &serverAddress.sin_addr) <= 0) {

        cerr << "Error: Invalid ON IP address\n";
        close(serverSocket);
        return 1;
    }


    // ------------------------------------------------------------
    // 10. Bind TCP socket
    // ------------------------------------------------------------

    if (bind(serverSocket,
             (struct sockaddr *)&serverAddress,
             sizeof(serverAddress)) < 0) {

        cerr << "Error: Could not bind TCP socket\n";
        close(serverSocket);
        return 1;
    }


    // ------------------------------------------------------------
    // 11. Listen for incoming VN connections
    // ------------------------------------------------------------

    if (listen(serverSocket, 10) < 0) {

        cerr << "Error: listen() failed\n";
        close(serverSocket);
        return 1;
    }

    cout << "\nOracle Node listening on "
         << serverIP << ":5000\n";


    // ------------------------------------------------------------
    // 12. Accept VN connections
    // ------------------------------------------------------------

    int nodeCount = 0;

    while (true) {

        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);

        int clientSocket = accept(
            serverSocket,
            (struct sockaddr *)&clientAddress,
            &clientLength
        );

        if (clientSocket < 0) {
            cerr << "Error: accept() failed\n";
            continue;
        }


        // First connection = A
        // Second connection = B
        // Third connection = C
        // etc.

        char nodeName = 'A' + nodeCount;

        cout << "VN connected as node "
             << nodeName << "\n";


        nodeCount++;


        // We don't use clientSocket yet.
        //
        // In the next step, this socket will be stored
        // and used to receive the CONNECT message from
        // the VN.
    }


    // This code is currently unreachable because
    // the server runs continuously.

    close(serverSocket);

    return 0;
}