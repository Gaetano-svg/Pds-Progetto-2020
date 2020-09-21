#include <iostream>
#include "client.hpp"

using namespace std;

int main()
{

    // Create Server Object
    Client client;
    std::string response;

    // read client configuration file
    client.readConfiguration();

    // initialize client logger
    client.initLogger();

    // connect the client to the server
    client.serverConnection();

    // send user configuration to server (in order to collect users info)
    client.sendConfiguration();

    cout << "send update message" << endl;

    msg::message fcu {
        "update",
        3,
        "test",
        "test",
        "test create"
    };

    client.sendMessage(fcu);
    client.readMessageResponse(response);

    cout << response << endl;

    sleep(200);

    cout << "exit" << endl;

    // 5. Threads initialization

    return 0;

}
