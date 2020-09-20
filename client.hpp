#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
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

    conf::user uc;
    std::shared_ptr <spdlog::logger> myLogger;

public:

	int sock;
    int readConfiguration ();
    int initLogger();
    int serverConnection();

    int sendConfiguration();
    string readConfigurationResponse (int fd);

    int sendMessage(msg::message msg);
    string readMessageResponse (int fd);
    
    int fromStringToMessage(string msg, msg::message& message);

    bool readNBytes(int fd, void *buf, std::size_t n);

};