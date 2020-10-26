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

    // send bytes
    int sendMessage (msg::message2 msg);
    int sendFileStream (string filePath);

    // receive bytes
    int readMessageResponse (string & response);
    int readInitialConfStream (int packetsNumber, string conf);

    // parsing method
    int fromStringToMessage(string msg, msg::message& message);

    // separator to create correct paths
    inline string separator()
    {
        #ifdef _WIN32
            return "\\";
        #else
            return "/";
        #endif
    }

public:

    int readConfiguration ();
    int initLogger();

    int serverConnection();
    int send(int operation, string folderPath, string fileName, string content);
    int serverDisconnection();
    
    bool isClosed ();

};