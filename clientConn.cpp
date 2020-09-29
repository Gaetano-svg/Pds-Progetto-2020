
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


std::string Server::ClientConn::compute_hash(const std::string file_path)
{
    try
    {

        std::string result;
        CryptoPP::Weak1::MD5 hash;
        CryptoPP::FileSource(file_path.c_str(), true, new
                CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(new
                                                                            CryptoPP::StringSink(result), false)));
        return result;

    } catch (...){

        log -> error("an error happened creating hash code from " + file_path);
        return "";

    }

}

/*

RETURN:

*/

int Server::ClientConn::selective_search(string & response, msg::message & msg)
{

    string path = server.backupFolder + "/" + msg.userName + "/" + msg.folderPath;
    boost::filesystem::path dstFolder = path;

    log -> info("trying to read folder path: " + path);

    if( ! boost::filesystem::exists(dstFolder)){

        // it isn't an error because the path doesn't exist
        log -> info("the path doesn't exist:" + path);
        response = "[]";
        return 0;

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
                compute_hash(dir -> path().c_str())
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

        log -> error("an error occured computing hash from path folder: " + path);

        // fill the response with an empty array
        response = "[]";
        return -1;

    }

    return 0;

}

void Server::ClientConn::sendResponse(int resCode, msg::message msg) {
        
    msg::message response;
    string responseString;
    int sendCode = 0;

    if(resCode == 0)
        handleOkResponse(response, msg);
                        
    else 
        handleErrorResponse(response, msg, resCode);

    resCode = fromMessageToString(responseString, response);

    if(resCode == 0) {

        // send LENGTH of response
        uint64_t sizeNumber = responseString.length();
        
        sendCode = send(sock, &sizeNumber, sizeof(sizeNumber), 0);
        if (sendCode < 0){

            string error = strerror(errno);
            log -> error("error sending RESPONSE LENGTH: " + error);
            return;

        } else if (sendCode == 0){


            log -> error("socket was closed before sending RESPONSE LENGTH");
            this -> running.store(false);
            return;

        }

        // send STRING response
        sendCode = send(sock, responseString.c_str(), sizeNumber, 0);
        if (sendCode < 0){

            string error = strerror(errno);
            log -> error("error sending RESPONSE DATA: " + error);
            return;

        } else if (sendCode == 0){


            log -> error("socket was closed before sending RESPONSE DATA");
            this -> running.store(false);
            return;

        }
                
        log -> info ("response sent: " + responseString );
        log -> flush();

    } else 

        log -> error ("error parsing MESSAGE response to string");

}


/// Reads message from fd
int Server::ClientConn::readMessage(int fd, string & bufString) {
        
    uint64_t rcvDataLength;
    std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
    std::string receivedString;     // assign buffered data to a 
    int resCode = 0;
    
    try {

        // Receive the message length
        resCode = recv(fd,&rcvDataLength,sizeof(uint64_t),0);
        
        if (resCode < 0){
        //if (select(fd,&rcvDataLength,sizeof(uint64_t),0, &m_timeInterval) < 0){

            string error = strerror(errno);
            
            log -> error("error receiving MESSAGE LENGTH: " + error);
            return -1;

        } else if (resCode == 0){

            log -> error("socket was closed before receiving MESSAGE LENGTH: ");
            this -> running.store(false);
            return -2;
        }
            
        log -> info ("message size received: " + to_string(rcvDataLength));
        log -> flush();
        
        if(rcvDataLength > 0){

            // resize string with the necessary size
            rcvBuf.resize(rcvDataLength,0x00); 

            // Receive the string data
            resCode = recv(fd,&(rcvBuf[0]),rcvDataLength,0);
            
            if (resCode < 0){

                string error = strerror(errno);
                
                log -> error("error receiving MESSAGE: " + error);
                return -3;

            } else if (resCode == 0){

                log -> error("socket was closed before receiving MESSAGE DATA: ");
                this -> running.store(false);
                return -2;

            }

            receivedString.assign(rcvBuf.begin(), rcvBuf.end());
            bufString = receivedString.c_str();

            log -> info ("message received: " + receivedString);
            log -> flush();

        } else {

            log -> error("Message length received wasn't correct: " + to_string(rcvDataLength));
            return -4;

        }

    } catch (const std::exception & e) {

        string excString = e.what();

        log -> error ("An exception occured reading Message received from client: " + excString);
        
        return -5;

    }
    
    return 0;

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

    while(running.load()){   

        try {

            log -> info ("wait for message from the client");
            log -> flush();

            string buf;
            
            milliseconds ms = duration_cast< milliseconds >(
                system_clock::now().time_since_epoch()
            );
            this -> activeMS.store(ms.count());

            if (readMessage(sock, buf) == 0){
           
                string sBuf = buf;
                string responseString;

                ms = duration_cast< milliseconds >(
                    system_clock::now().time_since_epoch()
                );

                this -> activeMS.store(ms.count());

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

                        // close connection
                        case 6:

                            resCode = 0;
                            running.store(false);

                        break;
                        
                    }

                }

                sendResponse(resCode, msg); 

            } else { 

                log -> info("going to sleep for 1 second because of error");
                sleep(1);

            }

        } catch (...) {

            log -> error("unexpected error happened");
            return;

        }

    }
        
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

            if(selective_search(stringConf, msg) < 0){

                log -> error("an error occured exploring path: " + msg.folderPath);
                return -1;

            }

            response.fileContent = stringConf;
        }
        else
            response.fileContent = "";

    } catch (...) {

        log -> error("an error occured handling OK response");
        return -2;

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
        
        try{

            this -> serv.activeConnections ++; 
            // set logger for client connection using the server log file
            if(initLogger() == 0){

                // while loop to wait for different messages from client
                waitForMessage();
                serv.unregisterClient(sock);

                log -> info ("[CLIENT-CONN-" + to_string(this -> sock) + "]: exited from run");
                log -> flush();

            }

        } catch (exception e){

            string error = e.what();
            cout << error << endl;

        }
        
    });

    inboundChannel.detach();
    
}