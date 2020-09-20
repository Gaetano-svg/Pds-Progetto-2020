#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <boost/filesystem.hpp>
#include <unistd.h>
#include "json.hpp"
#include "message.hpp"
#include "configuration.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

using namespace std;
using namespace nlohmann;
using json = nlohmann::json;

enum ClientStatus {
    starting, active, terminating
};

class ClientConn {


public:

    ClientStatus status;
    conf::server server;
    msg::connection userConf;
    int sock;
    string ip;
    string logFile;
    bool running;

    shared_ptr <spdlog::logger> log;

    ClientConn(string& logFile, int& sock, conf::server server);

    void handleConnection();

    void waitUserConfiguration();
    int initLogger();
    void waitForMessage();

    string readUserConfiguration(int fd);
    int fromStringToUserConf(string uc, msg::connection& userConf);

    string readMessage(int fd);
    int fromStringToMessage(string msg, msg::message& message);
    void fromMessageToString(string & messageString, msg::message & msg);
    int handleFileCreation(msg::message msg);
    int handleFileUpdate(msg::message msg);
    int handleFileDelete(msg::message msg);
    int handleFileRename(msg::message msg);

    void sendResponse(int resCode, msg::message msg );
    
    void handleOkResponse(msg::message & response, msg::message &  msg);
    void handleErrorResponse(msg::message &  response, msg::message & msg, int errorCode);

    void fromStringToCreationMsgBody(string msg, msg::fileCreate& message);
    
    bool readNBytes(int fd, void *buf, std::size_t n);

};