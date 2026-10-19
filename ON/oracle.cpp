#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


using namespace std;
int main(int argc, char *argv[]){
    if(argc != 3){
        cerr << "Wrong Input format\n";
        return 1;
    }
    string filename = argv[1];
    string serverIP = argv[2];

    ifstream file(filename);

    if(!file){
        cerr << "Some file reading error\n";
        return 1;
    }

    vector<vector<int>> rows;
    string line;

    while(getline(file, line)){
        auto pos = line.find('#'); //comment lines
        
        if(pos != string::npos){
            line = line.substr(0,pos);
        }

        stringstream ss(line);

        vector<int> row;
        int value;

        while(ss >> value){
            row.push_back(value);
        }

        if(!row.empty()){     //ignore empty lines/comment lines
            rows.push_back(row);
        }
    }
    file.close();


    int N = rows.size() + 1;

    cout << "No. of nodes: " << N << "\n";

    vector<vector<int>> graph(N, vector<int>(N, -1));

    for(int i = 0; i < N; i++){
        graph[i][i] = 0;              //diagnol
    }

    for(int i = 0; i < N-1; i++){
        for(int j = i+1; j < N; j++){
            int cost = rows[i][j-i-1];

            graph[i][j] = cost;
            graph[j][i] = cost;
        }
    }


    // cout << "Graph:\n\n";

    // cout << "    ";
    // for (int i = 0; i < N; i++) {
    //     cout << char('A' + i) << "   ";
    // }
    // cout << "\n";

    // for (int i = 0; i < N; i++) {
    //     cout << char('A' + i) << "   ";

    //     for (int j = 0; j < N; j++) {
    //         cout << graph[i][j] << "   ";
    //     }

    //     cout << "\n";
    // }


    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if(serverSocket < 0){
        cerr << "Socket creation error\n";
        return 1;
    }

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

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(5000);
    // serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (inet_pton(AF_INET, serverIP.c_str(),
              &serverAddress.sin_addr) <= 0) {

        cerr << "Error: Invalid IP address\n";
        close(serverSocket);
        return 1;
    }


    if (bind(serverSocket,
             (struct sockaddr *)&serverAddress,
             sizeof(serverAddress)) < 0) {

        cerr << "Error: Could not bind TCP socket\n";
        close(serverSocket);
        return 1;
    }


    if(listen(serverSocket, 10) < 0){
        cerr << "Listen Failed";
        close(serverSocket);
        return 1;
    }

    // cout << "Oracle node listening on port 5000\n";
    cout << "\nOracle Node listening on "
         << serverIP << ":5000\n";


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

        char nodeName = 'A' + nodeCount;

        cout << "VN connected as node "
             << nodeName << "\n";


        nodeCount++;
    }

    close(serverSocket);

    return 0;
}