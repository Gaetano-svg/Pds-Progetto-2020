
#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>

#ifdef _WIN32

#include <winsock2.h>
#include <Windows.h>

#else

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <boost/filesystem.hpp>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#endif

#include <sys/types.h>
#include <nlohmann/json.hpp>
#include "configuration.hpp"
#include <spdlog/spdlog.h>
//#include <spdlog/sinks/basic_file_sink.h>
#include <functional>
#include "SimpleSocket.hpp"
#include "ActiveSocket.hpp"
#include "message.hpp"

using namespace nlohmann;
using json = nlohmann::json;

#ifndef CLientClass
#define CLientClass

class Client {

    int sock;
    CActiveSocket socketObj;
    conf::user uc;
    std::shared_ptr <spdlog::logger> myLogger;

    // send bytes
    int sendMessage(message2 msg, std::atomic<bool>& b);
    int sendFileStream(std::string filePath, std::atomic<bool>& b);

    // receive bytes
    int readMessageResponse(std::string& response);
    int readInitialConfStream(int packetsNumber, std::string conf);

    // separator to create correct paths
    inline std::string separator()
    {
#ifdef _WIN32
        return "\\";
#else
        return "/";
#endif
    }

public:

    int readConfiguration();
    int initLogger();

    int serverConnection();
    int send(int operation, std::string folderPath, std::string fileName, std::string content, std::uintmax_t file_size,
        std::string hash, long timestamp, std::atomic<bool>& b);
    int serverDisconnection();

    bool isClosed();

};

#endif
