#include <iostream>
#include "client.hpp"

using namespace std;

int main(int argc, char** args)
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

    cout <<"try inactivity" << endl;

    msg::message fcu {
        "initconf",
        5,
        "test2",
        "test2",
        "test conf 2",
        "gaetano"
    };

    client.serverConnection();
    
    client.sendMessage(fcu);
    client.readMessageResponse(response);
    //client.serverDisconnection();

    cout << "send create message 2" << endl;

    msg::message fcu2 {
        "create",
        3,
        "test4",
        "test4",
        "test create",
        client.uc.name

    };

    sleep(2);
    //client.serverConnection();
    client.sendMessage(fcu2);
    client.readMessageResponse(response);


    cout << "send create message 3" << endl;

    msg::message fcu3 {
        "create",
        3,
        "test5",
        "test5",
        "test create",
        client.uc.name

    };

    sleep(20);
    //client.serverConnection();
    client.sendMessage(fcu3);
    client.readMessageResponse(response);

    client.serverDisconnection();

    cout << response << endl;

    sleep(1200);
    client.serverDisconnection();

    cout << "exit" << endl;

    // 5. Threads initialization

    return 0;

}
