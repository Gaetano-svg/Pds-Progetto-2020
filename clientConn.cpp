
#include "server.hpp"
#include <algorithm>
#include <vector>
#include <arpa/inet.h>
#include "boost/filesystem.hpp"

using namespace boost::filesystem;

/*

RETURN:

 0 -> no error
-1 -> logger initialization error
-2 -> generic error
-3 -> unexpected error

*/

int Server::ClientConn::initLogger(){

    try 
    {
        // check if log already exists
        //this -> log = spdlog::get(userConf.userName+"_client");
        this -> log = spdlog::get("client_" + to_string(this -> sock));

        if(this -> log == nullptr)
            this -> log = spdlog::basic_logger_mt("client_" + to_string(this -> sock),logFile);

        this -> log -> info("Logger initialized correctly");
        this -> log -> flush();
    } 
    catch (const spdlog::spdlog_ex &ex) 
    {
        string error = ex.what();
        cerr << "error during logger initialization: " + error << endl;
        return -1;
    }
    catch (const exception &ex) 
    {
        string error = ex.what();
        cerr << "generic error during logger initialization: " + error << endl;
        return -2;
    }
    catch (...)
    {
        cerr << "Unexpected error appened during logger initialization" << endl;
        return -3;
    }

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::fromMessageToString(string & messageString, msg::message & msg){

    try 
    {
        json jMsg = json{{"type", msg.type}, {"typeCode", msg.typeCode}, {"fileName", msg.fileName}, {"folderPath", msg.folderPath}, {"fileContent", msg.fileContent}};
        messageString = jMsg.dump();
    } 
    catch (...) 
    {

        log -> error ("An error appened converting the message to string: " + msg.type);
        log -> flush();

        return -1;
    }

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::fromStringToMessage(string msg, msg::message& message){

    try {

        auto jsonMSG = json::parse(msg);

        jsonMSG.at("type").get_to(message.type);
        jsonMSG.at("typeCode").get_to(message.typeCode);
        jsonMSG.at("folderPath").get_to(message.folderPath);
        jsonMSG.at("userName").get_to(message.userName);
        jsonMSG.at("fileName").get_to(message.fileName);
        jsonMSG.at("fileContent").get_to(message.fileContent);

    } catch (...) {

        log -> error ("An error appened parsing the message received: " + msg);
        log -> flush();
        return -10;

    }
    
    return 0;

}

/*

RETURN:

*/

void Server::ClientConn::selective_search(string & response, msg::message & msg)
{

    string path = server.backupFolder + "/" + msg.userName + "/" + msg.folderPath;
    boost::filesystem::path dstFolder = path;

    log -> info("trying to read folder path: " + path);
    
    if( ! boost::filesystem::exists(dstFolder)){

        log -> info("the path doesn't exist:" + path);
        response = "[]";
        return;

    }
    
    try
    {

        recursive_directory_iterator dir(path), end;
        auto jsonObjects = json::array();

        // iterating over the local folder of the client
        while (dir != end)
        {
            msg::initialConf conf {
                dir -> path().c_str(),
                dir -> path().c_str()
            };

            json obj = json {{"path", conf.path},{"hash", conf.hash}};
            string stringa = obj.dump();
            jsonObjects.push_back(obj);

            ++dir;
        }

        response = jsonObjects.dump();

    } 
    catch (...)
    {

        log -> error("an error occured reading path folder: " + path);

        // fill the response with an empty array
        response = "[]";

    }

}

void Server::ClientConn::sendResponse(int resCode, msg::message msg) {
        
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

/// Reads message from fd
string Server::ClientConn::readMessage(int fd) {
        
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

/*

RETURN:

*/

void Server::ClientConn::waitForMessage(){

    //while(running){

    msg::message msg;
    int resCode;
    uint64_t sizeNumber;

    // declaration of response message
    msg::message response;
            
    try {

        log -> info ("wait for message from the client");
        log -> flush();

        string buf = readMessage(sock);

        string sBuf = buf;
        string responseString;

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

                // initial configuration
                case 5:

                    resCode = 0;

                break;
            }

        }

        sendResponse(resCode, msg);

    } catch (...) {

        log -> error("unexpected error happened");
        return;

    }
                        
    //}
        
}

/*

RETURN:

*/

int Server::ClientConn::handleOkResponse(msg::message & response, msg::message & msg){

    try{

        response.type = "ok";
        response.typeCode = 0;
        response.folderPath = msg.folderPath;
        response.fileName = msg.fileName;
        response.userName = msg.userName;

        if(msg.typeCode == 5) {
            string stringConf;
            selective_search(stringConf, msg);
            response.fileContent = stringConf;
        }
        else
            response.fileContent = "";

    } catch (...) {

        log -> error("an error occured handling OK response");
        return -1;

    }

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::handleErrorResponse(msg::message & response, msg::message & msg, int errorCode){

    try 
    {
            
        response.typeCode = errorCode;
        response.folderPath = msg.folderPath;
        response.fileName = msg.fileName;
        response.userName = msg.userName;

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

            case -20:
                response.type = "genericError";
                response.fileContent = "A generic error";
            break;
        }

    } 
    catch (...) 
    {

        log -> error("an error occured handling ERROR response");
        return -1;

    }

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::handleFileCreation(msg::message msg){

    string path = server.backupFolder + "/" + msg.userName + "/" + msg.folderPath;
    boost::filesystem::path dstFolder = path, filePath;

    try {
        
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

    } catch (...) {

        log -> error ("an error occured handling file creation " + path);
        return -20;

    }
    
    log -> info("creation of file: " + path);
    log -> flush();

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::handleFileUpdate(msg::message msg){

    string path = server.backupFolder + "/" + msg.userName + "/" + msg.folderPath;
    boost::filesystem::path dstFolder = path, filePath;

    try {

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

    } catch (...) {

        log -> error("an error occured handling file update: " + path);
        return -20;

    }         

    log -> info("update of file: " + path);
    log -> flush();

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::handleFileRename(msg::message msg){

    string path = server.backupFolder + "/" + msg.userName + "/" + msg.folderPath;
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

/*

RETURN:

*/

int Server::ClientConn::handleFileDelete(msg::message msg){

    string path = server.backupFolder + "/" + msg.userName + "/" + msg.folderPath;
    boost::filesystem::path dstFolder = path;

    // if the path doesn't exist return because there isn't any files inside
    if( ! boost::filesystem::exists(dstFolder))
        return -1; // folder doesn't exist

    path +=  + "/" + msg.fileName;

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

/*

RETURN:

*/

void Server::ClientConn::handleConnection(){

    std::thread inboundChannel([this](){
        
        // set logger for client connection using the server log file
        initLogger();

        // while loop to wait for different messages from client
        waitForMessage();
            
        serv.unregisterClient(sock);
            
    });

    inboundChannel.detach();

}