#include <iostream>
#include <fstream>
#include <string>
#include "json.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "configuration.hpp"

using namespace std;
using namespace nlohmann;

class Server {

    string ip;
    int port;
    shared_ptr <spdlog::logger> log;
    nlohmann::json jServerConf;

    Server (string ip, int port){

        this -> ip = ip;
        this -> port = port;

    }

    // init the Server Logger
    // return -1 in case of error
    int initLogger(){

        try 
        {
            this -> log = spdlog::basic_logger_mt(this -> ip,"server_log.txt");
            log -> info("Logger initialized correctly");
        } 
        catch (const spdlog::spdlog_ex &ex) 
        {
            cerr << ex.what() << endl;
            return -1;
        }

        return 0;

    }

    int readConfiguration (string file) {

        // Read SERVER configuration file in the local folder

        ifstream serverConfFile(file);

        if(!serverConfFile)
        {
            string error = strerror(errno);
            cerr << "Server Configuration File: " << file << " could not be opened!";
            cerr << "Error code opening Server Configuration File: " << error;
            return -1;
        }
        
        if(!(serverConfFile >> jServerConf))
        {
            cerr << "The Server Configuration File couldn't be parsed";
            return -2; 
        }

        // save the server configuration inside a local struct
        conf::server sc
        {
            jServerConf["ip"].get<string>(),
            jServerConf["port"].get<string>()
        };

    }

};