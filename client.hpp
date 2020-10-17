#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <boost/filesystem.hpp>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "json.hpp"
#include "message.hpp"
#include "configuration.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "socketLibrary/SimpleSocket.h"
#include "socketLibrary/ActiveSocket.h"

using namespace std;
using namespace nlohmann;
using json = nlohmann::json;

class Client {

	int sock;

    CActiveSocket socketObj;
    conf::user uc;
    
    std::shared_ptr <spdlog::logger> myLogger;

    int sendMessage2(msg::message2 msg);
    int readMessageResponse2 (string & response);
    int sendFileStream(string filePath);
    int readInitialConfStream(int packetsNumber, string conf);
    int fromStringToMessage(string msg, msg::message& message);

    inline string separator()
    {
        #ifdef _WIN32
            return "\\";
        #else
            return "/";
        #endif
    }

public:

    int send(int operation, string folderPath, string fileName, string content);
    
    int readConfiguration ();
    int initLogger();
    int serverConnection();
    int serverDisconnection();
    bool isClosed ();

};