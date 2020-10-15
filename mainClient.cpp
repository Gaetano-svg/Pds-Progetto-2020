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

    cout <<"try inactivity" << endl;

    client.serverConnection();
/*
    msg::message fcu {
        "initconf",
        5,
        "C:\\Users\\TanoPC",
        "test2",
        "test conf 2",
        "gaetano"
    };
    
    //if(!client.isClosed()){

        client.sendMessage(fcu);
        client.readMessageResponse(response);

    //}*/
  

    cout << "send create message 2" << endl;

    /*msg::message2 fcu2 {
        "create",
        3,
        0, // timestamp
        "",// hash
        "C:\\Users\\TanoPC\\Desktop",
        "test4",
        "test create",
        client.uc.name

    };*/

    client.send(3, "/home/gaetano/Desktop/testRead", "test5.txt", "");

    client.send(1, "/home/gaetano/Desktop/testRead", "test5.txt", "");

    //client.serverDisconnection();

    /*//if(!client.isClosed()){

        client.sendMessage(fcu2);
        client.readMessageResponse(response);

    //}

    fcu2 = {
        "create",
        3,
        "C:\\Users\\TanoPC\\Desktop",
        "test5",
        "test create 2",
        client.uc.name

    };

    //if(!client.isClosed()){

        client.sendMessage(fcu2);
        client.readMessageResponse(response);

    //}
    
    sleep(20);

    fcu2 = {

        "delete",
        4,
        "C:\\Users\\TanoPC\\Desktop",
        "test5",
        "test delete 2",
        client.uc.name

    };

    if (!client.isClosed()){

        client.sendMessage(fcu2);
        client.readMessageResponse(response);

    }

    sleep(20);

    fcu2 = {
        
        "delete",
        2,
        "C:\\Users\\TanoPC\\Desktop",
        "test4",
        "test4rename",
        client.uc.name

    };

    if(!client.isClosed()){

        client.sendMessage(fcu2);
        client.readMessageResponse(response);
        
    }

    cout << "send update message 3" << endl;

    msg::message fcu3 {
        "update",
        1,
        "C:\\Users\\TanoPC",
        "test4",
        "test update",
        client.uc.name

    };

    if(!client.isClosed()){

        client.sendMessage(fcu3);
        client.readMessageResponse(response);
        
    }

    client.serverDisconnection();

    cout << "exit" << endl;*/

    // 5. Threads initialization

    return 0;

}
