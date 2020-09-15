#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <thread>
#include "server.hpp"
#include "json.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

using namespace nlohmann;
using pClient = std::shared_ptr<ClientConn>;
using namespace std;
using json = nlohmann::json;

// per ora stiamo considerando un solo utente e un solo folder

// in futuro dovremo gestire un array di utenti e per ciascuno un solo folder (o array di folder)
// in modo tale dal main, istanziare un thread per ogni utente il quale a sua volta si connetterà al server ...

int main()
{

    // Create Server Object

    Server server;
    server.readConfiguration("serverConfiguration.json");
    //server.readUsersPath();
    server.initLogger();

        std::thread serverThread([&server](){
            // this keeps the ChatClient alive until the we dont't exit the thread
            // even if it's removed from the clients map (we "abuse" of RAII)

            server.startListening();

            
        });


    // 1. Read USER configuration file in the local folder

    std::ifstream userConfFile("userConfiguration.json");

    if(!userConfFile)
    {
        cerr << "User Configuration File could not be opened!\n"; // Report error
        cerr << "Error code: " << strerror(errno); // Get some info as to why
        return -1;
    }

    json jUserConf;

    if(!(userConfFile >> jUserConf))
    {
        cerr << "The User Configuration File couldn't be parsed";
        return -1; 
    }

    // save the user configuration inside a local struct
    conf::user uc
    {
        jUserConf["serverIp"].get<string>(),
        jUserConf["serverPort"].get<string>(),
        jUserConf["name"].get<string>(),
        jUserConf["folderPath"].get<string>()
    };

    // 2. Logger initialization

    std::shared_ptr <spdlog::logger> myLogger;

    try 
    {
        myLogger = spdlog::basic_logger_mt(uc.name, uc.name+"_log.txt");
        myLogger -> info("Logger initialized correctly");
        myLogger -> flush();
    } 
    catch (const spdlog::spdlog_ex &ex) 
    {
        cout << ex.what() << endl;
        return -1;
    }

    myLogger -> info("Starting read Server configuration file in the local folder");
    myLogger -> flush();

    // 4. Server connection

	// Create socket
	int sock ;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
        string error = strerror(errno);
		myLogger -> error("Can't create socket, Error: " + error);
        close(sock);
		return -1;
	}

    myLogger -> info("Socket " + to_string(sock) + " was created");

	// Fill in a hint structure
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(atoi(server.sc.port.c_str()));
	
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if ((inet_pton(AF_INET, "127.0.0.1", &hint.sin_addr)) <= 0)
    {
        myLogger -> error("Invalid address: address " + server.sc.ip + " not supported");
        close(sock);
        return -1;
    }

	// Connect to server
	while (connect(sock, (sockaddr*)&hint, sizeof(hint)) < 0)
	{
        string error = strerror(errno);
        myLogger -> error("Can't connect to server, Error: " + error);
        myLogger -> flush();
        sleep(5);
	}

    myLogger -> info ("connected to server");
    myLogger -> flush();

    // Send Configuration 

    msg::connection connMess
    {
        jUserConf["name"].get<string>(),
        jUserConf["folderPath"].get<string>()
    };

    json jConnMess = json{{"userName", connMess.userName}, {"folderPath", connMess.folderPath}};

    string jUcString = jConnMess.dump();

    myLogger -> info("Sending user configuration to server: " + jUcString + " length: " + to_string(jUcString.length()) + " bytes");
    myLogger -> flush();

    send(sock, jUcString.c_str(), jUcString.length(), 0);
    sleep(5);
    // test msgForCreation

    /*msg::message fc {
        "creation",
        3,
        "test",
        "test",
        "test234"
    };
    json jMsg = json{{"type", fc.type}, {"typeCode", fc.typeCode}, {"fileName", fc.fileName}, {"folderPath", fc.folderPath}, {"fileContent", fc.fileContent}};
    string jMsgString = jMsg.dump();

    myLogger -> info("Sending creation msg for file to server: " + jMsgString + " length: " + to_string(jMsgString.length()) + " bytes");
    myLogger -> flush(); 
    send(sock, jMsgString.c_str(), jMsgString.length(), 0);
    // test msgForCreation

    myLogger -> info("go to sleep for 10 sec; then will send an update message");
    myLogger -> flush();

    sleep(10);*/

    msg::message fcu {
        "update",
        1,
        "test",
        "test",
        "test update chico"
    };
    json jMsgU = json{{"type", fcu.type}, {"typeCode", fcu.typeCode}, {"fileName", fcu.fileName}, {"folderPath", fcu.folderPath}, {"fileContent", fcu.fileContent}};
    string jMsgStringU = jMsgU.dump();

    myLogger -> info("Sending update msg for file to server: " + jMsgStringU + " length: " + to_string(jMsgStringU.length()) + " bytes");
    myLogger -> flush(); 
    send(sock, jMsgStringU.c_str(), 1024, 0);

    myLogger -> info("go to sleep for 20 sec; then will send a delete message");
    myLogger -> flush();

    sleep(20);

    msg::message fcd {
        "delete",
        4,
        "test",
        "test",
        ""
    };
    json jMsgD = json{{"type", fcd.type}, {"typeCode", fcd.typeCode}, {"fileName", fcd.fileName}, {"folderPath", fcd.folderPath}, {"fileContent", fcd.fileContent}};
    string jMsgStringD = jMsgD.dump();

    myLogger -> info("Sending update msg for file to server: " + jMsgStringD + " length: " + to_string(jMsgStringD.length()) + " bytes");
    myLogger -> flush(); 
    send(sock, jMsgStringD.c_str(), 1024, 0);

    myLogger -> info("go to sleep for 20 sec; then will send a delete message again");
    myLogger -> flush();

    sleep(20);

    send(sock, jMsgStringD.c_str(), 1024, 0);

    myLogger -> info("go to sleep for 20 sec; then will send a rename message");
    myLogger -> flush();

    sleep(20);
    msg::message fcr {
        "rename",
        2,
        "test",
        "test",
        "testRenamed"
    };
    json jMsgR = json{{"type", fcr.type}, {"typeCode", fcr.typeCode}, {"fileName", fcr.fileName}, {"folderPath", fcr.folderPath}, {"fileContent", fcr.fileContent}};
    string jMsgStringR = jMsgR.dump();

    ///////////////////////////////////////////////////////////////
    // bisogna mandare tutta la lunghezza disponibile col socket //
    ///////////////////////////////////////////////////////////////

    myLogger -> info("Sending update msg for file to server: " + jMsgStringR + " length: " + to_string(jMsgStringR.length()) + " bytes");
    myLogger -> flush(); 
    send(sock, jMsgStringR.c_str(), 1024, 0);

    serverThread.join();

    cout << "exit" << endl;

    //////////////////////////////////////////////////////////////
    //// si deve implementare la risposta da parte del server ////
    //////////////////////////////////////////////////////////////

    // 5. Threads initialization

    return 0;
}
