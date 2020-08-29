#include <iostream>
#include <fstream>
#include <string>

enum ClientStatus {
    starting, active, terminating
};

class ClientConn {

public:

    ClientStatus status;
    Server *server;
    int sock;
    string ip;
    string logFile;

    shared_ptr <spdlog::logger> log;

    ClientConn(string ip, string logFile, int sock, Server *server): ip(ip), logFile(logFile), sock(sock), server(server), status(starting) {};

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

};