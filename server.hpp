#include <iostream>
#include <fstream>
#include <string>
#include "json.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "configuration.hpp"
#include "clientConn.hpp"
#include <mutex>
#include <shared_mutex>

using namespace std;
using namespace nlohmann;
using pClient = std::shared_ptr<ClientConn>;

#define IDLE_TIMEOUT 60

class Server {

public:

    shared_ptr <spdlog::logger> log;
    nlohmann::json jServerConf;

    // the server configuration containing ip and port informations
    conf::server sc;

    static std::mutex m;
    int sock = -1;
    bool running;
    std::map<int, pClient> clients; // mapping fd -> pClient

    string ip;
    int port;
    string logFile = "server_log.txt";

    Server (){}

    // init the Server Logger
    // return -1 in case of error
    int initLogger(){

        try 
        {
            this -> log = spdlog::basic_logger_mt(this -> sc.ip, this -> logFile);
            this -> log -> info("Logger initialized correctly");
            this -> log -> flush();
        } 
        catch (const spdlog::spdlog_ex &ex) 
        {
            cerr << ex.what() << endl;
            return -1;
        }

        return 0;

    }

    int readConfiguration (string file) {

        // Read SERVER configuration file in the local folder

        ifstream serverConfFile(file);
        json jServerConf;

        if(!serverConfFile)
        {
            string error = strerror(errno);
            cerr << "Server Configuration File: " << file << " could not be opened!";
            cerr << "Error code opening Server Configuration File: " << error;
            return -1;
        }
        
        if(!(serverConfFile >> jServerConf))
        {
            cerr << "The Server Configuration File couldn't be parsed";
            return -2; 
        }

        // save the server configuration inside a local struct
        this -> sc = {
            jServerConf["ip"].get<string>(),
            jServerConf["port"].get<string>()
        };

        return 0;

    }

    /*void checkIdleClients() {

        // we terminate the server with SIGINT
        std::thread idleClients([this](){

            while(running){
                // che idle client each seconds
                sleep(1);

                std::time_t now = time(nullptr);
                // store terminating clients and after terminate them to avoid a deadlock
                std::vector<pClient> terminating;
                {
                    std::lock_guard<mutex> lg(m);
                    for(std::pair<int,pClient> client: clients){
                        pClient c = client.second;
                        if (c->status == active){
                            terminating.push_back(c);
                        }
                    }
                }
                for(auto c: terminating)
                    c->close();

            }
        });

        idleClients.detach();

    }
    */
    // a while loop used to listen to all the connection request
    int startListening(){

        running = true;
        int opt = 1;
        if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR , &opt, sizeof(opt))<0){
            log -> error("sockopt error");
            log -> error(strerror(errno));
            return -1;
        }

        sockaddr_in saddr;
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(port);
        saddr.sin_addr.s_addr = INADDR_ANY;

        if(bind(sock, (struct sockaddr *)&saddr, sizeof(saddr))<0){
            log -> error("bind error");
            log -> error(strerror(errno));
            return -1;
        }

        if(::listen(sock, 10)<0){
            log -> error("listen error");
            log -> error(strerror(errno));
            return -1;
        }

        // start idle clients loop -> for now it is not used
        // checkIdleClients();

        // we and the server with SIGINT
        while(running) {
            sockaddr_in caddr;
            socklen_t addrlen = sizeof(caddr);

            int csock = accept(sock, (struct sockaddr*) &caddr, &addrlen);
            if(csock<0){
                log -> error("accept error");
                log -> error(strerror(errno));
            } else {
                char buff[8];
                std::lock_guard<mutex> lg(m);

                //get socket ip address
                struct sockaddr* ccaddr = (struct sockaddr*)&caddr;
                string clientIp = ccaddr -> sa_data;

                pClient client = pClient(new ClientConn(clientIp, this -> logFile, csock, this));

                // set logger for client connection using the server log file
                client -> initLogger();

                // this keeps the client alive until it's destroyed
                clients[csock] = client;

                // handle connection should return immediately
                client->handleConnection();
            }

        }
        return 0;

    }

    ~Server(){
        if(sock!=-1)
            close(sock);
    }

    void unregisterClient(int csock){
        std::lock_guard <mutex> lg(m);
        clients.erase(csock);
    }

};