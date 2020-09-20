
#include "clientConn.hpp"
#include <algorithm>
#include <vector>
#include <arpa/inet.h>

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

    void ClientConn::fromMessageToString(string & messageString, msg::message & msg){

        json jMsg = json{{"type", msg.type}, {"typeCode", msg.typeCode}, {"fileName", msg.fileName}, {"folderPath", msg.folderPath}, {"fileContent", msg.fileContent}};
        messageString = jMsg.dump();

    }

    int ClientConn::fromStringToUserConf(string uc, msg::connection& userConf){

        try {

            auto jsonUC = json::parse(uc);

            jsonUC.at("userName").get_to(userConf.userName);
            jsonUC.at("folderPath").get_to(userConf.folderPath);

        } catch (...) {

            log -> error ("An error appened parsing the user configuration received: " + uc);
            log -> flush();
            return -11;

        }

        return 0;

    }

    int ClientConn::fromStringToMessage(string msg, msg::message& message){

        try {

            auto jsonMSG = json::parse(msg);

            jsonMSG.at("type").get_to(message.type);
            jsonMSG.at("typeCode").get_to(message.typeCode);
            jsonMSG.at("folderPath").get_to(message.folderPath);
            jsonMSG.at("fileName").get_to(message.fileName);
            jsonMSG.at("fileContent").get_to(message.fileContent);

        } catch (...) {

            log -> error ("An error appened parsing the message received: " + msg);
            log -> flush();
            return -10;

        }

        return 0;

    }

    void ClientConn::fromStringToCreationMsgBody(string msg, msg::fileCreate& message){

        msg.erase(std::remove(msg.begin(), msg.end(), '\\'), msg.end());
        
        auto jsonMSG = json::parse(msg);

        jsonMSG.at("folderPath").get_to(message.folderPath);
        jsonMSG.at("name").get_to(message.name);
        jsonMSG.at("content").get_to(message.content);

    }

    void ClientConn::waitUserConfiguration(){

        msg::message msg, response;
        string responseString, bufString;

        bufString = readUserConfiguration(sock);

        //log -> info("received user configuration: " + bufString);

        int resCode = fromStringToUserConf(bufString, this -> userConf);

            response.folderPath = "";
            response.fileContent = "";
            response.fileName = "";

        if(resCode == 0){
            response.typeCode = 0;
            response.type = "Ok";
        }
                        
        else {
            response.typeCode = -1;
            response.type = "Error parsing user configuration";
        }

            fromMessageToString(responseString, response);

            // send LENGTH of response
            uint64_t sizeNumber = responseString.length();
            send(sock, &sizeNumber, sizeof(sizeNumber), 0);

            // send STRING response
            send(sock, responseString.c_str(), sizeNumber, 0);

    }

      /// Reads n bytes from fd.
    bool ClientConn::readNBytes(int fd, void *buf, std::size_t n) {

        std::size_t offset = 0;
        char *cbuf = reinterpret_cast<char*>(buf);
        while (true) {
            ssize_t ret = recv(fd, cbuf + offset, n - offset, MSG_WAITALL);
            if (ret < 0) {
                if (errno != EINTR) {
                    // Error occurred
                    //throw IOException(strerror(errno));
                    return false;
                }
            } else if (ret == 0) {
                // No data available anymore
                if (offset == 0) return false;
                else             return false;//throw ProtocolException("Unexpected end of stream");
            } else if (offset + ret == n) {
                // All n bytes read
                return true;
            } else {
                offset += ret;
            }
        }
    }

    /// Reads message from fd
    string ClientConn::readMessage(int fd) {
        
        uint64_t rcvDataLength;
        string bufString;
        std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
        std::string receivedString;                        // assign buffered data to a 

        recv(fd,&rcvDataLength,sizeof(uint64_t),0); // Receive the message length
        rcvBuf.resize(rcvDataLength,0x00); // with the necessary size

        log -> info ("message size received: " + to_string(rcvDataLength));

        recv(fd,&(rcvBuf[0]),rcvDataLength,0); // Receive the string data
        receivedString.assign(rcvBuf.begin(), rcvBuf.end());

        log -> info ("message received: " + receivedString);

        bufString = receivedString.c_str();
        return bufString;

    }

    string ClientConn::readUserConfiguration(int fd) {
        
        uint64_t rcvDataLength;
        string bufString;
        std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
        std::string receivedString;                        // assign buffered data to a 

        recv(fd,&rcvDataLength,sizeof(uint64_t),0); // Receive the message length
        rcvBuf.resize(rcvDataLength,0x00); // with the necessary size

        recv(fd,&(rcvBuf[0]),rcvDataLength,0); // Receive the string data
        receivedString.assign(rcvBuf.begin(), rcvBuf.end());

        bufString = receivedString.c_str();
        return bufString;

    }

    void ClientConn::sendResponse(int resCode, msg::message msg) {
        
        msg::message response;
        string responseString;

            if(resCode == 0)
                handleOkResponse(response, msg);
                        
            else 
                handleErrorResponse(response, msg, resCode);

            fromMessageToString(responseString, response);

            // send LENGTH of response
            uint64_t sizeNumber = responseString.length();
            send(sock, &sizeNumber, sizeof(sizeNumber), 0);

            // send STRING response
            send(sock, responseString.c_str(), sizeNumber, 0);
            
            log -> info ("response sent: " + responseString );
            log -> flush();

    }

    void ClientConn::waitForMessage(){

        while(running){

            msg::message msg;
            int resCode;
            uint64_t sizeNumber;

            // declaration of response message
            msg::message response;
            
            log -> info ("wait for message from the client");
            log -> flush();

            string buf = readMessage(sock);

            string sBuf = buf;
            string responseString;

            log -> info ("message received from the client " + sBuf);
            log -> flush();

            resCode = fromStringToMessage(buf, msg);

            if(resCode == 0){
                
                log -> info ("message parsed" );
                log -> flush();

                switch(msg.typeCode){
                    
                    // file update
                    case 1:

                        resCode = handleFileUpdate(msg);

                    break;

                    // file rename
                    case 2:

                        resCode = handleFileRename(msg);

                    break;

                    // file creation
                    case 3:

                        resCode = handleFileCreation(msg);

                    break;

                    // file delete
                    case 4:

                        resCode = handleFileDelete(msg);

                    break;
                }

            }

            sendResponse(resCode, msg);
            
        }
        
    }

    void ClientConn::handleOkResponse(msg::message & response, msg::message & msg){

        response.type = "ok";
        response.typeCode = 0;
        response.folderPath = msg.folderPath;
        response.fileName = msg.fileName;
        response.fileContent = "";

    }

    void ClientConn::handleErrorResponse(msg::message & response, msg::message & msg, int errorCode){


        response.typeCode = errorCode;
        response.folderPath = msg.folderPath;
        response.fileName = msg.fileName;

        switch(errorCode){

            case -1:
                response.type = "missingFolderError";
                response.fileContent = "Folder doesn't exist!";
            break;


            case -2:
                response.type = "missingFileError";
                response.fileContent = "File doesn't exist!";
            break;

            case -10:
                response.type = "messageParsingStringError";
                response.fileContent = "An error happened during the parsing of the message received from the client";
            break;

            case -11:
                response.type = "userConfigurationParsingStringError";
                response.fileContent = "An error happened during the parsing of the user configuration received from the client";
            break;

        }

    }

    int ClientConn::handleFileCreation(msg::message msg){

        string path = server.backupFolder + "/" + userConf.userName + "/" + msg.folderPath;
        boost::filesystem::path dstFolder = path, filePath;

        if( ! boost::filesystem::exists(dstFolder))
            boost::filesystem::create_directories(dstFolder);

        path +=  + "/" + msg.fileName;

        filePath = path;

        // if the file already exist doesn't need to be created
        if(boost::filesystem::exists(filePath)){
            
            log -> info("the file " + path + " already exists: the output will be redirected on it");
            log -> flush();

        }

        bool completed = false;
        while(!completed)
        {
            try
            {
                std::ofstream file(path); //open in constructor
                file << msg.fileContent;
                completed = true;
            }
            catch(...) 
            {

                log -> error("file: " + path + " currently used by others; sleep for 5 seconds and retry later ");
                log -> flush();
                sleep(5);
            }
        }  

        log -> info("creation of file: " + path);
        log -> flush();

        return 0;

    }

    int ClientConn::handleFileUpdate(msg::message msg){

        string path = server.backupFolder + "/" + userConf.userName + "/" + msg.folderPath;
        boost::filesystem::path dstFolder = path, filePath;

        if( ! boost::filesystem::exists(dstFolder))
            boost::filesystem::create_directories(dstFolder);
        
        path +=  + "/" + msg.fileName;
        
        filePath = path;

        // if the file already doesn't exist must be created 
        if(!boost::filesystem::exists(filePath)){
            
            log -> info("the file " + path + " doesn't exist: the output will be redirected on it");
            log -> flush();

        }
       
        bool completed = false;
        while(!completed)
        {
            try
            {
                std::ofstream file(path); //open in constructor
                file << msg.fileContent;
                completed = true;
            }
            catch(...) 
            {

                log -> error("file: " + path + " currently used by others; sleep for 5 seconds and retry later ");
                log -> flush();
                sleep(5);
            }
        }             

        log -> info("update of file: " + path);
        log -> flush();

        return 0;

    }

    int ClientConn::handleFileRename(msg::message msg){

        string path = server.backupFolder + "/" + userConf.userName + "/" + msg.folderPath;
        boost::filesystem::path dstFolder = path;

        string oldPathString, newPathString;

        // if the path doesn't exist return because there isn't any files inside
        if( ! boost::filesystem::exists(dstFolder))
            return -1;

        oldPathString = path + "/" + msg.fileName;
        newPathString = path + "/" + msg.fileContent;

        boost::filesystem::path oldPath = oldPathString;
        boost::filesystem::path newPath = newPathString;

        // if the file doesn't exist return 
        if( ! boost::filesystem::exists(oldPath)) {

                log -> error("file: " + oldPathString + " doesn't exist!");
                log -> flush();
            return -2;
        }

        bool completed = false;
        while(!completed)
        {
            try
            {
                boost::filesystem::rename(oldPath, newPath);
                completed = true;
            }
            catch(...) 
            {

                log -> error("file: " + oldPathString + " currently used by others; sleep for 5 seconds and retry later ");
                log -> flush();
                sleep(5);
            }
        }

        log -> info("rename of file: " + oldPathString + " into " + newPathString + " completed ");
        log -> flush();

        return 0;

    }

    int ClientConn::handleFileDelete(msg::message msg){

        string path = server.backupFolder + "/" + userConf.userName + "/" + msg.folderPath;
        boost::filesystem::path dstFolder = path;

        // if the path doesn't exist return because there isn't any files inside
        if( ! boost::filesystem::exists(dstFolder))
            return -1; // folder doesn't exist

        path +=  + "/" + msg.fileName;
        //newPathString = path + "/" + msg.fileContent;

        boost::filesystem::path oldPath = path;

        // if the file doesn't exist return 
        if( ! boost::filesystem::exists(oldPath)) {

                log -> error("file: " + path + " doesn't exist!");
                log -> flush();
                return -2; // file doesn't exist
        }


        bool completed = false;
        while(!completed)
        {
            try
            {
                boost::filesystem::remove_all(path);
                completed = true;
            }
            catch(...) 
            {

                log -> error("file: " + path + " currently used by others; sleep for 5 seconds and retry later ");
                log -> flush();
                sleep(5);
            }
        }

        log -> info("delete of file: " + path + " completed ");
        log -> flush();

        return 0;

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
            waitUserConfiguration();

            // set logger for client connection using the server log file
            initLogger();

            // while loop to wait for different messages from client
            waitForMessage();
            
        });

        inboundChannel.detach();

    }