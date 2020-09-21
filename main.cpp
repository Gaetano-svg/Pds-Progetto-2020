#include <iostream>
#include "server.hpp"

using namespace std;

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
