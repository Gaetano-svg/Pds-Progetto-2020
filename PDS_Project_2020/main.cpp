#include <iostream>
#include "client.hpp"

using namespace std;

int main(int argc, char** args)
{
    std::atomic <bool> a(false);

    // Create Server Object
    Client client;
    std::string response;

    // read client configuration file
    client.readConfiguration();

    // initialize client logger
    client.initLogger();

    // send user configuration to server (in order to collect users info)

    cout << "try inactivity" << endl;

    client.serverConnection();

    cout << "send create message 2" << endl;

    client.send(5, "C:/Users/gabuscema/Desktop/UserFolder/OtherFolder", "ciao.txt", "", 0, "", 0, a);


    //client.send(3, "/home/gaetano/Desktop/testRead", "test5.txt", "", 0, "", 0, a);
    client.serverDisconnection();

    client.serverConnection();
    client.send(3, "C:/Users/gabuscema/Desktop/UserFolder/OtherFolder", "ciao.txt", "", 0, "", 0, a);
    client.serverDisconnection();

    client.serverConnection();
    client.send(2, "C:/Users/gabuscema/Desktop/UserFolder/OtherFolder", "ciao.txt", "testRename", 0, "", 0, a);
    client.serverDisconnection();

    // 5. Threads initialization

    // far girare filewatcher -> passare CLIENT

    return 0;

}
