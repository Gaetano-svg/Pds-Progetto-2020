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
#include "json.hpp"
#include "message.hpp"
#include "configuration.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
//#include "clientConn.hpp"
#include <mutex>
#include <shared_mutex>

using namespace std;
using namespace nlohmann;


#define IDLE_TIMEOUT 60

class Server {
    

private:

    std::vector <conf::user> usersPath;
    shared_ptr <spdlog::logger> log;
    nlohmann::json jServerConf;

    // the server configuration containing ip and port informations
    conf::server sc;

    std::mutex m;
    int sock = -1;
    bool running;

    string ip;
    int port;
    string logFile = "server_log.txt";
    
public:

    class ClientConn {

        public:

            Server & serv;
            conf::server server;
            int sock;
            string ip;
            string logFile;
            string localPath;  // path of client folder
            bool running;

            shared_ptr <spdlog::logger> log;

            
            ClientConn(Server & serv, string& logFile, int& sock, conf::server server, string clientIp): serv(serv), ip(clientIp), logFile(logFile), sock(sock), server(server){
                running = true;
            };

            void handleConnection();

            void waitUserConfiguration();
            int initLogger();
            void waitForMessage();

            void selective_search(string & response, msg::message & msg);

            string readMessage(int fd);
            int fromStringToMessage(string msg, msg::message& message);
            int fromMessageToString(string & messageString, msg::message & msg);
            int handleFileCreation(msg::message msg);
            int handleFileUpdate(msg::message msg);
            int handleFileDelete(msg::message msg);
            int handleFileRename(msg::message msg);

            int handleOkResponse(msg::message & response, msg::message &  msg);
            int handleErrorResponse(msg::message &  response, msg::message & msg, int errorCode);

            void sendResponse(int resCode, msg::message msg );
            
    };

    //using pClient = std::shared_ptr<ClientConn>;

    std::map<int, shared_ptr<ClientConn>> clients; // mapping fd -> pClient

    Server (){};
    
    int initLogger();

    int readConfiguration (string file);
    
    int startListening();

    ~Server();

    void unregisterClient(int csock);

};