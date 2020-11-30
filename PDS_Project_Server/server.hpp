#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#define _CRT_SECURE_NO_WARNINGS 1
#define _CRT_NONSTDC_NO_DEPRECATE 1

#include <iostream>
#include <fstream>
#include <cstdint>
#include <sys/types.h>
#include <stdexcept>
#include <sys/types.h>
#include <string>
#include <thread>
#include "../PDS_Project_2020/json.hpp"
#include "../PDS_Project_2020/configuration.hpp"
#include "../PDS_Project_2020/message.hpp"
#include "spdlog/spdlog.h"
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include "../PDS_Project_2020/cryptopp/cryptlib.h"
#include "../PDS_Project_2020/cryptopp/md5.h"
#include "../PDS_Project_2020/cryptopp/files.h"
#include "../PDS_Project_2020/cryptopp/hex.h"
#include "SimpleSocket.hpp"
#include "PassiveSocket.hpp"
#include "ActiveSocket.hpp"

#ifdef _WIN32

#include <winsock2.h>
#include <Windows.h>
#include <filesystem>
#include <functional>

#else

#include <experimental/filesystem>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#endif

using namespace nlohmann;
using namespace std::chrono;

#define IDLE_TIMEOUT 60
#define PACKET_SIZE SOCKET_SENDFILE_BLOCKSIZE 

class Server {

    // Client Connection CLASS
    class ClientConn {

    private:

        Server& serv;
        conf::server server;
        std::string logFile;
        std::shared_ptr <spdlog::logger> log;

        inline std::string separator()
        {
#ifdef _WIN32
            return "\\";
#else
            return "/";
#endif
        }

        int initLogger();

        void waitForMessage();

        int readMessage(int fd, std::string& bufString);
        int readFileStream(int packetsNumber, int fileFd);

        void updateFile(int& resCode, std::string buf, message2& msg);
        void renameFile(int& resCode, std::string buf, message2& msg);
        void deleteFile(int& resCode, std::string buf, message2& msg);
        void initialConfiguration(int& resCode, std::string buf, message2& msg, std::string initConf);

        int handleFileUpdate(message2 msg, std::string buf);
        int handleFileDelete(message2 msg, std::string buf);
        int handleFileRename(message2 msg, std::string buf);

        int selective_search(std::string& response, std::string buf, message2& msg);
        std::string compute_hash(const std::string file_path);

        int sendResponse(int resCode, std::string buf, message2 msg);
        int32 SendInitialConf(std::string conf, int32 nOutFd, int32 nCount);

        int handleOkResponse(message2& response, message2& msg);
        int handleErrorResponse(message2& response, message2& msg, int errorCode);

        int fromStringToMessage(std::string msg, message2& message);
        int fromMessageToString(std::string& messageString, message2& msg);

    public:

        int sock;
        std::string ip;
        CActiveSocket* sockObject;
        std::atomic_bool running;
        std::atomic_long activeMS;

        ClientConn(Server& serv, std::string& logFile, conf::server server) : serv(serv), logFile(logFile), server(server) {
            running.store(true);
        };

        void handleConnection();

    };

private:

    // log reference
    std::shared_ptr <spdlog::logger> log;

    // server JSON configuration
    nlohmann::json jServerConf;

    // number of active connections to check if the next connection request will be satisfied
    std::atomic_int activeConnections;

    // atomic variable to controll the server from shutting down
    std::atomic_bool running;

    // the server configuration containing ip and port informations
    conf::server sc;

    // mutex to access shared structures like "clients"
    std::mutex m;

    int sock = -1, port;
    std::string ip, logFile = "server_log.txt";

    // structure to map socket to ClientConnection object
    std::map<int, std::shared_ptr<ClientConn>> clients;

    void checkUserInactivity();
    void unregisterClient(int csock);
    bool isClosed(int sock);

public:

    Server() {};

    int readConfiguration(std::string file);
    int initLogger();
    int startListening();

    ~Server();

};

