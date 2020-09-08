#include <iostream>
#include <fstream>
#include <string>
#include "json.hpp"
#include "message.hpp"

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

    shared_ptr <spdlog::logger> log;

    ClientConn(string& ip, string& logFile, int& sock, conf::server server): ip(ip), logFile(logFile), sock(sock), server(server){};

    int initLogger(){

        try 
        {
            this -> log = spdlog::basic_logger_mt(this -> ip, this -> logFile);
            this -> log -> info("Logger initialized correctly");
            this -> log -> flush();
        } 
        catch (const spdlog::spdlog_ex &ex) 
        {
            cerr << ex.what() << endl;
            return -1;
        }

        return 0;

    }

    void fromStringToUserConf(string uc, msg::connection& userConf){

        auto jsonUC = json::parse(uc);

        jsonUC.at("name").get_to(userConf.userName);
        jsonUC.at("folderPath").get_to(userConf.folderPath);

    }

    void getUserConfiguration(){

        std::string buf;
        char c;
        log -> info("Client Connection on IP: " + ip + " wait for user name");
        
        recv(sock, &buf, sizeof(conf::user), 0);
        fromStringToUserConf(buf, this -> userConf);
            
        log -> info("Client Connection on IP: " + ip + " setted for username: " + userConf.userName);
        
    }

    void handleConnection(){

        // it receive the request from the client and send response
        // è un meccanismo SINCRONO: da quando il sender del client manda la richiesta
        // esso si mette in attesa di una risposta da parte del SERVER
        // quindi non capita di mandare più richieste di seguito al server senza prima ricevere una risposta

        std::thread inboundChannel([this](){
            // this keeps the ChatClient alive until the we dont't exit the thread
            // even if it's removed from the clients map (we "abuse" of RAII)
            
        });

        inboundChannel.detach();

        // it sends the response to the client -> for now not used
        /*std::thread outboundChannel([this](){
            // this keeps the object alive until the we dont't exit the thread
        });

        outboundChannel.detach();*/

    }

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