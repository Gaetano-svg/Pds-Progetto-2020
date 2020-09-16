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

using namespace std;
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
    int initLogger();
    int fromStringToUserConf(string uc, msg::connection& userConf);
    int fromStringToMessage(string msg, msg::message& message);
    void fromStringToCreationMsgBody(string msg, msg::fileCreate& message);
    void getUserConfiguration();
    void handleConnection();
    int handleFileCreation(msg::message msg);
    int handleFileUpdate(msg::message msg);
    int handleFileDelete(msg::message msg);
    int handleFileRename(msg::message msg);

    void fromMessageToString(string & messageString, msg::message & msg);
    void handleOkResponse(msg::message & response, msg::message &  msg);
    void handleErrorResponse(msg::message &  response, msg::message & msg, int errorCode);

    void waitForMessage();

 /*   /// Reads n bytes from fd.
bool readNBytes(int fd, void *buf, std::size_t n) {
    std::size_t offset = 0;
    char *cbuf = reinterpret_cast<char*>(buf);
    while (true) {
        ssize_t ret = recv(fd, cbuf + offset, n - offset, MSG_WAITALL);
        if (ret < 0) {
            if (errno != EINTR) {
                // Error occurred
                throw IOException(strerror(errno));
            }
        } else if (ret == 0) {
            // No data available anymore
            if (offset == 0) return false;
            else             throw ProtocolException("Unexpected end of stream");
        } else if (offset + ret == n) {
            // All n bytes read
            return true;
        } else {
            offset += ret;
        }
    }
}

/// Reads message from fd
std::vector<char> readMessage(int fd) {
    std::uint64_t size;
    if (readNBytes(fd, &size, sizeof(size))) {
        std::vector buf(size);
        if (readNBytes(fd, buf.data(), size)) {
            return buf;
        } else {
            throw ProtocolException("Unexpected end of stream");
        }
    } else {
        // connection was closed
        return std::vector<char>();
    }
}*/

};