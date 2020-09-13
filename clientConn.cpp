
#include "clientConn.hpp"
#include <algorithm>

#define MSG_SIZE 1024

ClientConn::ClientConn( string& logFile, int& sock, conf::server server):logFile(logFile), sock(sock), server(server){
    running = true;
};

    int ClientConn::initLogger(){

        try 
        {
            this -> log = spdlog::basic_logger_mt(userConf.userName+"_client",logFile);

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

        jsonUC.at("userName").get_to(userConf.userName);
        jsonUC.at("folderPath").get_to(userConf.folderPath);

    }

    void ClientConn::fromStringToMessage(string msg, msg::message& message){

        auto jsonMSG = json::parse(msg);

        jsonMSG.at("type").get_to(message.type);
        jsonMSG.at("typeCode").get_to(message.typeCode);
        jsonMSG.at("folderPath").get_to(message.folderPath);
        jsonMSG.at("fileName").get_to(message.fileName);
        jsonMSG.at("fileContent").get_to(message.fileContent);

    }

    void ClientConn::fromStringToCreationMsgBody(string msg, msg::fileCreate& message){

        msg.erase(std::remove(msg.begin(), msg.end(), '\\'), msg.end());
        std::cout << msg << std::endl;
        auto jsonMSG = json::parse(msg);

        jsonMSG.at("folderPath").get_to(message.folderPath);
        jsonMSG.at("name").get_to(message.name);
        jsonMSG.at("content").get_to(message.content);

    }

    void ClientConn::getUserConfiguration(){

        char buf[MSG_SIZE];
        
        recv(sock, buf, MSG_SIZE, 0);
        
        fromStringToUserConf(buf, this -> userConf);
        
    }

    void ClientConn::waitForMessage(){

        while(running){

            char buf[MSG_SIZE];
            msg::message msg;
            
            log -> info ("wait for message from the client");
            log -> flush();

            recv(sock, buf, MSG_SIZE, 0);

            log -> info ("message received from the client");
            log -> flush();

            fromStringToMessage(buf, msg);

            log -> info ("message parsed" );
            log -> flush();

            // switch case to differentiate different messages
            switch(msg.typeCode){

                // file creation
                case 3:

                    handleFileCreation(msg);

                break;

            }

        }
        
    }

    void ClientConn::handleFileCreation(msg::message msg){

        string path = server.backupFolder + "/" + userConf.userName + "/" + msg.folderPath;
        boost::filesystem::path dstFolder = path;
        boost::filesystem::create_directories(dstFolder);

        path +=  + "/" + msg.fileName;
                    
        std::ofstream file(path); //open in constructor

        log -> info("creation of file: " + path + " content: " + msg.fileContent);
        log -> flush();

        file << msg.fileContent;

    }

    void ClientConn::handleConnection(){

        // it receive the request from the client and send response
        // è un meccanismo SINCRONO: da quando il sender del client manda la richiesta
        // esso si mette in attesa di una risposta da parte del SERVER
        // quindi non capita di mandare più richieste di seguito al server senza prima ricevere una risposta

        std::thread inboundChannel([this](){
            // this keeps the ChatClient alive until the we dont't exit the thread
            // even if it's removed from the clients map (we "abuse" of RAII)

            // first of all, get the user configuration
            getUserConfiguration();

            // set logger for client connection using the server log file
            initLogger();

            // while loop to wait for different messages from client
            waitForMessage();
            
        });

        inboundChannel.detach();

    }