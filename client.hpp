#include <iostream>
#include <fstream>
#include <string>
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

using namespace std;
using namespace nlohmann;
using json = nlohmann::json;

class Client {

    std::shared_ptr <spdlog::logger> myLogger;

public:

    conf::user uc;
    
	int sock;
    int readConfiguration ();
    int initLogger();
    int serverConnection();
    int serverDisconnection();
    bool isClosed ();

    int sendMessage(msg::message msg);
    int readMessageResponse (string & response);
    
    int fromStringToMessage(string msg, msg::message& message);

};