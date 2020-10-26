#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include <iostream>
#include <fstream>
#include <cstdint>
#include <experimental/filesystem>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
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
#include "cryptopp/cryptlib.h"
#include "cryptopp/md5.h"
#include "cryptopp/files.h"
#include "cryptopp/hex.h"
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include "socketLibrary/ActiveSocket.h"

using namespace std;
using namespace nlohmann;
using namespace std::chrono;

#define IDLE_TIMEOUT 60
#define PACKET_SIZE SOCKET_SENDFILE_BLOCKSIZE 

class Server {

    // Client Connection CLASS
    class ClientConn {

        private:

            Server & serv;
            conf::server server;
            string logFile;
            shared_ptr <spdlog::logger> log;

            inline string separator()
            {
                #ifdef _WIN32
                    return "\\";
                #else
                    return "/";
                #endif
            }

            int initLogger();

            void waitForMessage();

            int readMessage(int fd, string & bufString);
            int readFileStream(int packetsNumber, int fileFd);

            void updateFile(int& resCode, string buf, msg::message2& msg);
            void renameFile(int& resCode, string buf, msg::message2& msg);
            void deleteFile(int& resCode, string buf, msg::message2& msg);
            void initialConfiguration(int& resCode, string buf, msg::message2& msg, string initConf);

            int handleFileUpdate(msg::message2 msg, string buf);
            int handleFileDelete(msg::message2 msg, string buf);
            int handleFileRename(msg::message2 msg, string buf);

            int selective_search(string & response, string buf, msg::message2 & msg);
            string compute_hash(const std::string file_path);

            int sendResponse(int resCode, string buf, msg::message2 msg);
            int32 SendInitialConf(string conf, int32 nOutFd,int32 nCount);

            int handleOkResponse(msg::message2 & response, msg::message2 &  msg);
            int handleErrorResponse(msg::message2 &  response, msg::message2 & msg, int errorCode);

            int fromStringToMessage(string msg, msg::message2& message);
            int fromMessageToString(string & messageString, msg::message2 & msg);
        
        public:

            int sock;
            string ip;
            CActiveSocket* sockObject;
            atomic_bool running;
            atomic_long activeMS;

            ClientConn(Server & serv, string& logFile, conf::server server): serv(serv), logFile(logFile), server(server){
                running.store(true);
            };

            void handleConnection();

    };

private:

    // log reference
    shared_ptr <spdlog::logger> log;

    // server JSON configuration
    nlohmann::json jServerConf;

    // number of active connections to check if the next connection request will be satisfied
    atomic_int activeConnections;

    // atomic variable to controll the server from shutting down
    atomic_bool running;

    // the server configuration containing ip and port informations
    conf::server sc;

    // mutex to access shared structures like "clients"
    std::mutex m;

    int sock = -1, port;
    string ip, logFile = "server_log.txt";

    // structure to map socket to ClientConnection object
    std::map<int, shared_ptr<ClientConn>> clients; 

    void checkUserInactivity();
    void unregisterClient(int csock);
    bool isClosed (int sock);
    
public:

    Server (){};

    int readConfiguration (string file);
    int initLogger();
    int startListening();

    ~Server();

};