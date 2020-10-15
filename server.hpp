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

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include "cryptopp/cryptlib.h"
#include "cryptopp/md5.h"
#include "cryptopp/files.h"
#include "cryptopp/hex.h"
//#include "clientConn.hpp"
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>

#include "socketLibrary/ActiveSocket.h"

using namespace std;
using namespace nlohmann;
using namespace std::chrono;


#define IDLE_TIMEOUT 60

class Server {
    

private:

    std::vector <conf::user> usersPath;
    shared_ptr <spdlog::logger> log;
    nlohmann::json jServerConf;
    atomic_int activeConnections;

    // the server configuration containing ip and port informations
    conf::server sc;

    std::mutex m;
    int sock = -1;

    string ip;
    int port;
    string logFile = "server_log.txt";
    
public:

    atomic_bool running;
    class ClientConn {

        private:

            inline string separator()
            {
                #ifdef _WIN32
                    return "\\";
                #else
                    return "/";
                #endif
            }

            Server & serv;
            conf::server server;
            string logFile;
            string localPath;  // path of client folder

            shared_ptr <spdlog::logger> log;

            void waitUserConfiguration();
            int initLogger();
            void waitForMessage();
            void waitForMessage2();

            int selective_search(string & response, msg::message2 & msg);

            int readMessage(int fd, string & bufString);
            int readMessage2(int fd, string & bufString);
            int fromStringToMessage(string msg, msg::message2& message);
            int fromMessageToString(string & messageString, msg::message2 & msg);
            int handleFileCreation(msg::message msg);
            int handleFileUpdate(msg::message msg);
            int handleFileUpdate2(msg::message2 msg);
            int handleFileDelete(msg::message2 msg);
            int handleFileRename(msg::message2 msg);

            int handleOkResponse(msg::message2 & response, msg::message2 &  msg);
            int handleErrorResponse(msg::message2 &  response, msg::message2 & msg, int errorCode);

            string compute_hash(const std::string file_path);
            void sendResponse(int resCode, msg::message msg );
            int sendResponse2(int resCode, msg::message2 msg );

            int readFileStream(int packetsNumber, int fileFd);
        
        public:

            int sock;
            string ip;
            CActiveSocket* sockObject;

            std::mutex mRunning;
            atomic_bool running;
            atomic_long activeMS;

            ClientConn(Server & serv, string& logFile, conf::server server): serv(serv), logFile(logFile), server(server){
                running.store(true);
            };

            void handleConnection();

    };

    //using pClient = std::shared_ptr<ClientConn>;

    std::map<int, shared_ptr<ClientConn>> clients; // mapping fd -> pClient

    Server (){};
    
    void checkUserInactivity();

    int initLogger();

    int readConfiguration (string file);
    
    int startListening();

    ~Server();

    void unregisterClient(int csock);

    bool isClosed (int sock);

};