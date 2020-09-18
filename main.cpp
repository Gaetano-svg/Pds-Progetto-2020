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

std::shared_ptr <spdlog::logger> myLogger;

      /// Reads n bytes from fd.
    bool readNBytes(int fd, void *buf, std::size_t n) {
        std::size_t offset = 0;
        char *cbuf = reinterpret_cast<char*>(buf);
        while (true) {
            ssize_t ret = recv(fd, cbuf + offset, n - offset, MSG_WAITALL);
            if (ret < 0) {
                if (errno != EINTR) {
                    // Error occurred
                    //throw IOException(strerror(errno));
                    return false;
                }
            } else if (ret == 0) {
                // No data available anymore
                if (offset == 0) return false;
                else             return false;//throw ProtocolException("Unexpected end of stream");
            } else if (offset + ret == n) {
                // All n bytes read
                return true;
            } else {
                offset += ret;
            }
        }
    }

    /// Reads message from fd
    string readResponse(int fd) {
        uint64_t size;
        std::string bufString;
            myLogger -> info("wait for length ");
        if (readNBytes(fd, &size, sizeof(size))) {

            myLogger -> info("size Risposta ricevuta " + to_string(size));
            char buf[size];
            memset(buf, 0, size);
            if (readNBytes(fd, buf, size)) {
                bufString = buf;
                myLogger -> info("Risposta ricevuta " + bufString);
            } else {
                //throw ProtocolException("Unexpected end of stream");
                bufString = buf;
            }
        }

        return bufString;
    }

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

    send(sock, jUcString.c_str(), 1024, 0);
    sleep(20);

    msg::message responseMsg;
    char responseChar[1024];

    memset(responseChar, 0, 1024);
    recv(sock, responseChar,  1024, 0);
    string responseString = responseChar;

    myLogger -> info("message received: " + responseString);
    myLogger -> flush();

    msg::message fcu {
        "update",
        3,
        "test",
        "test",
        "test create chico"
    };
    json jMsgU = json{{"type", fcu.type}, {"typeCode", fcu.typeCode}, {"fileName", fcu.fileName}, {"folderPath", fcu.folderPath}, {"fileContent", fcu.fileContent}};
    string jMsgStringU = jMsgU.dump();
    uint64_t sizeNumber = jMsgStringU.length();

    myLogger -> info("Sending create SIZE msg for file to server: " + to_string(jMsgStringU.length()) + " bytes");
    myLogger -> flush(); 
    
    send(sock, &sizeNumber, sizeof(sizeNumber), 0);

    myLogger -> info("Sending create msg for file to server: " + jMsgStringU + " length: " + to_string(jMsgStringU.length()) + " bytes");
    myLogger -> flush(); 

    send(sock, jMsgStringU.c_str(), sizeNumber, 0);
    
    myLogger -> info("wait For response");
    myLogger -> flush();
/*
    memset(responseChar, 0, 1024);
    recv(sock, responseChar,  1024, 0);*/
    responseString = readResponse(sock);

    myLogger -> info("message received: " + responseString);
    myLogger -> flush();

/*        auto jsonMSG = json::parse(responseChar);

        jsonMSG.at("type").get_to(responseMsg.type);
        jsonMSG.at("typeCode").get_to(responseMsg.typeCode);
        jsonMSG.at("folderPath").get_to(responseMsg.folderPath);
        jsonMSG.at("fileName").get_to(responseMsg.fileName);
        jsonMSG.at("fileContent").get_to(responseMsg.fileContent);


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

    myLogger -> info("wait For response");
    myLogger -> flush();

            memset(responseChar, 0, 1024);
    recv(sock, responseChar,  1024, 0);
    responseString = responseChar;
    myLogger -> info("go to sleep for 20 sec; then will send a delete message again");
    myLogger -> flush();

    sleep(20);

    send(sock, jMsgStringD.c_str(), 1024, 0);

    myLogger -> info("wait For response");
    myLogger -> flush();

            memset(responseChar, 0, 1024);
    recv(sock, responseChar,  1024, 0);
    responseString = responseChar;

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

    myLogger -> info("wait For response");
    myLogger -> flush();

            memset(responseChar, 0, 1024);
    recv(sock, responseChar,  1024, 0);
    responseString = responseChar;
*/
    serverThread.join();

    cout << "exit" << endl;

    //////////////////////////////////////////////////////////////
    //// si deve implementare la risposta da parte del server ////
    //////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////
    //// implementare logica invio pacchetti per lunghezza multipla di 1024 B ////
    //////////////////////////////////////////////////////////////////////////////

    // 5. Threads initialization

    return 0;
}
