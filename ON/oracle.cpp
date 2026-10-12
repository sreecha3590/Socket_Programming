#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>

using namespace std;
int main(int argc, char *argv[]){
    if(argc != 2){
        cerr << "Wrong Input format\n";
        return 1;
    }
    string filename = argv[1];

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

    return 0;
}