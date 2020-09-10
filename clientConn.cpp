
#include "clientConn.hpp"

ClientConn::ClientConn(string ip, string& logFile, int& sock, conf::server server): ip(ip), logFile(logFile), sock(sock), server(server){};

    int ClientConn::initLogger(){

        try 
        {
            this -> log = spdlog::basic_logger_mt(ip+"_client",logFile);

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

    void ClientConn::fromStringToUserConf(string uc, msg::connection& userConf){

        auto jsonUC = json::parse(uc);

        jsonUC.at("name").get_to(userConf.userName);
        jsonUC.at("folderPath").get_to(userConf.folderPath);

    }

    void ClientConn::getUserConfiguration(){

        std::string buf;
        char c;
        log -> info("Client Connection on IP: " + ip + " wait for user name");
        
        recv(sock, &buf, sizeof(conf::user), 0);
        fromStringToUserConf(buf, this -> userConf);
            
        log -> info("Client Connection on IP: " + ip + " setted for username: " + userConf.userName);
        
    }

    void ClientConn::handleConnection(){

        // it receive the request from the client and send response
        // è un meccanismo SINCRONO: da quando il sender del client manda la richiesta
        // esso si mette in attesa di una risposta da parte del SERVER
        // quindi non capita di mandare più richieste di seguito al server senza prima ricevere una risposta

        // set logger for client connection using the server log file
        initLogger();

        std::thread inboundChannel([this](){
            // this keeps the ChatClient alive until the we dont't exit the thread
            // even if it's removed from the clients map (we "abuse" of RAII)

        std::cout << " qua 2" << std:: endl;
            getUserConfiguration();

            
        });

        inboundChannel.detach();

        // it sends the response to the client -> for now not used
        /*std::thread outboundChannel([this](){
            // this keeps the object alive until the we dont't exit the thread
        });

        outboundChannel.detach();*/

    }