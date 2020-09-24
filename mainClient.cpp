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

    // send user configuration to server (in order to collect users info)
    //client.sendConfiguration();

    cout << "send initial conf message" << endl;

    msg::message fcu {
        "initial conf",
        3,
        "test2/testInner",
        "test2",
        "test create 2",
        "gaetano"
    };

    client.serverConnection();
    client.sendMessage(fcu);
    client.readMessageResponse(response);
    client.serverDisconnection();

    cout << "send update message 2" << endl;

    msg::message fcu2 {
        "update2",
        3,
        "test",
        "test",
        "test create",
        "gaetano"

    };

    client.serverConnection();
    client.sendMessage(fcu2);
    client.readMessageResponse(response);
    client.serverDisconnection();

    cout << response << endl;

    sleep(200);

    cout << "exit" << endl;

    // 5. Threads initialization

    return 0;

}
