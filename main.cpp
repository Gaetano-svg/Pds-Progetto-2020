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
/*
    CPassiveSocket socket;
    CActiveSocket *pClient = NULL;

    //--------------------------------------------------------------------------
    // Initialize our socket object 
    //--------------------------------------------------------------------------
    socket.Initialize();

    socket.Listen("127.0.0.1", 3524);

    while (true)
    {
        if ((pClient = socket.Accept()) != NULL)
        {
            //----------------------------------------------------------------------
            // Receive request from the client.
            //----------------------------------------------------------------------
            if (pClient->Receive(MAX_PACKET))
            {
                //------------------------------------------------------------------
                // Send response to client and close connection to the client.
                //------------------------------------------------------------------
                pClient->Send( pClient->GetData(), pClient->GetBytesReceived() );
                pClient->Close();
            }

            delete pClient;
        }
    }*/

    cout << "exit" << endl;

    // bisogna aggiungere il caso di timeout per ricevere la risposta da parte del SERVER
    // lato server non importa ciò -> abbiamo il thread che controlla l'inattività del CLIENT

    return 0;

}


