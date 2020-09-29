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

    // bisogna aggiungere il caso di timeout per ricevere la risposta da parte del SERVER
    // lato server non importa ciò -> abbiamo il thread che controlla l'inattività del CLIENT

    return 0;

}


