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
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "clientConn.hpp"
#include <mutex>
#include <shared_mutex>

using namespace std;
using namespace nlohmann;
using pClient = std::shared_ptr<ClientConn>;

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
    std::map<int, pClient> clients; // mapping fd -> pClient

    string ip;
    int port;
    string logFile = "server_log.txt";

public:


    Server (){};
    
    int initLogger();

    int readConfiguration (string file);
    
    int startListening();

    ~Server();

    void unregisterClient(int csock);

};