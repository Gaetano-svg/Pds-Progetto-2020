#include <iostream>
#include <fstream>
#include "server.hpp"
#include "json.hpp"

using namespace std;
using namespace nlohmann;

int main()
{

    // Create Server Object
    Server server;

    // read server configuration from file
    server.readConfiguration("serverConfiguration.json");

    // initialize the server logger file
    server.initLogger();

    // start the server listening to client connections
    server.startListening();

    cout << "exit" << endl;

    return 0;

}


