
#include "server.hpp"
#include <algorithm>
#include <vector>
#include <arpa/inet.h>
#include "boost/filesystem.hpp"

using namespace boost::filesystem;
namespace fs = std::experimental::filesystem;

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

int Server::ClientConn::fromMessageToString(string & messageString, msg::message2 & msg){

    try 
    {
        json jMsg = json{{"packetNumber", msg.packetNumber},{"userName", msg.userName},{"type", msg.type}, {"typeCode", msg.typeCode}, {"fileName", msg.fileName}, {"folderPath", msg.folderPath}, {"body", msg.body}};
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

int Server::ClientConn::fromStringToMessage(string msg, msg::message2& message){

    try {

        auto jsonMSG = json::parse(msg);

        jsonMSG.at("type").get_to(message.type);
        jsonMSG.at("typeCode").get_to(message.typeCode);
        jsonMSG.at("folderPath").get_to(message.folderPath);
        jsonMSG.at("userName").get_to(message.userName);
        jsonMSG.at("fileName").get_to(message.fileName);
        jsonMSG.at("body").get_to(message.body);

    } catch (...) {

        log -> error ("An error appened parsing the HEADER received: " + msg);
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

int Server::ClientConn::selective_search(string & response, msg::message2 & msg)
{
    string serverBackupFolder, msgFolderPath, path;

    try{

        serverBackupFolder = server.backupFolder;
        msgFolderPath = msg.folderPath;


        #       ifdef BOOST_POSIX_API   //workaround for user-input files
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\\', '/');         
        #       else
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '/', '\\');         
        #       endif

        path = serverBackupFolder + separator() + msg.userName + separator() + msgFolderPath;


    } catch (...){

    }
    //string path = server.backupFolder + "/" + msg.userName + "/" + msg.folderPath;
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
        
    msg::message2 response;
    string responseString;
    int sendCode = 0;
/*
    if(resCode == 0)
        handleOkResponse(response, msg);
                        
    else 
        handleErrorResponse(response, msg, resCode);*/

    resCode = fromMessageToString(responseString, response);

    if(resCode == 0) {

        uint64_t sizeNumber = responseString.length();
        string size = to_string(sizeNumber);

        log -> info("Sending SIZE msg for file to server: " + size + " bytes");
        log -> flush(); 

        sendCode = this -> sockObject -> Send((const uint8 *) size.c_str(), sizeof(uint64_t));

        if( sendCode < 0){
            
            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("an error occured sending response LENGTH: " + sockError);

            return;

        } else if( sendCode == 0){
            
            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("connection shutted down before sending response LENGTH: " + sockError);
            this -> running.store(false);

            return;
        }

        log -> info("Sending DATA msg for file to server: " + responseString + " length: " + to_string(responseString.length()) + " bytes");
        log -> flush(); 

        sendCode = this -> sockObject -> Send((const uint8 *) responseString.c_str(), sizeNumber);

        if(sendCode < 0){

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("an error occured sending response DATA: " + sockError);

            return;

        } else if (sendCode == 0) {

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("connection shutted down before sending response DATA: " + sockError);
            this -> running.store(false);

        }
                
        log -> info ("response sent: " + responseString );
        log -> flush();

    } else 

        log -> error ("error parsing MESSAGE response to string");

}


int Server::ClientConn::sendResponse2(int resCode, msg::message2 msg) {
        
    msg::message2 response;
    string responseString;
    int sendCode = 0;

    if(resCode == 0)
        handleOkResponse(response, msg);
                        
    else 
        handleErrorResponse(response, msg, resCode);

    resCode = fromMessageToString(responseString, response);

    if(resCode == 0) {

        log -> info("Sending DATA msg for file to server: " + responseString + " length: " + to_string(responseString.length()) + " bytes");
        log -> flush(); 

        sendCode = this -> sockObject -> Send((const uint8 *) responseString.c_str(), responseString.length());

        if(sendCode < 0){

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("an error occured sending response DATA: " + sockError);

            return -1;

        } else if (sendCode == 0) {

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("connection shutted down before sending response DATA: " + sockError);
            this -> running.store(false);

            return -2;

        }
                
        log -> info ("response sent: " + responseString );
        log -> flush();

    } else {

        log -> error ("error parsing MESSAGE response to string");
        return -3;

    }

    return 0;

}

int Server::ClientConn::readFileStream(int packetsNumber, int fileFd){

    // per ogni stream ricevuto dal client scrivo su un file temporaneo
    // se lo stream è andato a buon fine e ho ricevuto tutto faccio una copia dal file temporaneo a quello ufficiale
    // ed eliminio il file temporaneo

    int i = 0;
    int sockFd = this -> sockObject -> GetSocketDescriptor();
    char buffer [BUFSIZ];

    do {

        i++;

        // reset char array
        memset(buffer, 0, BUFSIZ);

        int read_return = read(sockFd, buffer, BUFSIZ);
        if (read_return == -1) {
            
            log -> error("An error occured reading packet # " + to_string(i) + " from socket " + to_string(sockFd));
            return -1;

        }

        if (write(fileFd, buffer, read_return) == -1) {
            
            log -> error("An error occured writing packet # " + to_string(i) + " to file descriptor: " + to_string(fileFd));
            return -2;

        }       

    } while(i < packetsNumber);

    return 0;

}

/// Reads message from fd
int Server::ClientConn::readMessage(int fd, string & bufString) {


    uint64_t rcvDataLength;
    std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
    int rcvCode = 0;
    fd_set set;
    struct timeval timeout;
    int iResult = 0;

    try {

        log -> info("wait For response");
        log -> flush();

        // Receive the message length
        rcvCode = this -> sockObject -> Receive(sizeof(uint64_t));

        if(rcvCode < 0){

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("an error occured receiving message LENGTH: " + sockError);
            return -1;

        } else if(rcvCode == 0){

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("connection shutted down before receiving message LENGTH: " + sockError);
            this -> running.store(false);
            return -4;

        }

        string length = (char *) sockObject -> GetData();
        rcvDataLength = atoi(length.c_str());

        log -> info ("response size received: " + length);
        log -> flush();

        // Receive the string data
        rcvCode = this -> sockObject -> Receive(rcvDataLength);

        if(rcvCode < 0){

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("an error occured receiving message DATA: " + sockError);
            return -2;

        } else if(rcvCode == 0){

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("connection shutted down before receiving message DATA: " + sockError);
            this -> running.store(false);
            return -4;

        }

        string respString = (char *) sockObject -> GetData();
        bufString.clear();
        bufString = respString;

        log -> info ("response received: " + bufString);
        log -> flush();
        
    } catch (...) {

        log -> error ("unexpected error happened reading message response");
        return -3;

    }

    return 0;

}

/// Reads message from fd
int Server::ClientConn::readMessage2(int fd, string & bufString) {

    uint64_t rcvDataLength;
    std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
    int rcvCode = 0;
    fd_set set;
    struct timeval timeout;
    int iResult = 0;

    try {

        log -> info("wait For MESSAGE HEADER");
        log -> flush();

        // Receive the message header
        rcvCode = this -> sockObject -> Receive(SOCKET_SENDFILE_BLOCKSIZE);

        if(rcvCode < 0){

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("an error occured receiving message HEADER: " + sockError);
            return -2;

        } else if(rcvCode == 0){

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            log -> error("connection shutted down before receiving message HEADER: " + sockError);
            this -> running.store(false);
            return -4;

        }

        string respString = (char *) sockObject -> GetData();
        bufString.clear();
        bufString = respString;

        log -> info ("MESSAGE HEADER received: " + bufString);
        log -> flush();
        
    } catch (...) {

        log -> error ("unexpected error happened reading message HEADER");
        return -3;

    }

    return 0;

}




void Server::ClientConn::waitForMessage2(){

    msg::message2 msg;
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

            if (readMessage2(sock, buf) == 0){
           
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
                        case 3:

                            resCode = sendResponse2(resCode, msg); 

                            if(resCode == 0){

                                resCode = handleFileUpdate2(msg);
                                resCode = sendResponse2(resCode, msg); 

                            }

                        break;

                        // file rename
                        case 2:

                            resCode = handleFileRename(msg);
                            resCode = sendResponse2(resCode, msg); 

                        break;
/*
                        // file creation
                        case 3:

                            resCode = sendResponse2(resCode, msg); 

                            if(resCode == 0){

                                resCode = handleFileUpdate2(msg);
                                resCode = sendResponse2(resCode, msg); 
                                
                            }

                        break;*/

                        // file delete
                        case 4:

                            resCode = handleFileDelete(msg);
                            resCode = sendResponse2(resCode, msg); 

                        break;

                        // initial configuration
                        case 5:

                            resCode = 0;
                            sendResponse2(resCode, msg); 

                        break;

                        // close connection
                        case 6:

                            resCode = 0;
                            running.store(false);

                        break;
                        
                    }

                }


            } else { 

                log -> info("going to sleep for 1 second because of error");
                sleep(1);

            }

        } catch (...) {

            log -> error("unexpected error happened");
            log -> info("going to sleep for 1 second because of error");
            sleep(1);
            return;

        }

    }


}


/*

RETURN:

*/
/*
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
            log -> info("going to sleep for 1 second because of error");
            sleep(1);
            return;

        }

    }
        
}
*/
/*

RETURN:

*/

int Server::ClientConn::handleOkResponse(msg::message2 & response, msg::message2 & msg){

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

            response.body = stringConf;
        }
        else
            response.body = "";

    } catch (...) {

        log -> error("an error occured handling OK response");
        return -2;

    }

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::handleErrorResponse(msg::message2 & response, msg::message2 & msg, int errorCode){

    try 
    {
            
        response.typeCode = errorCode;
        response.folderPath = msg.folderPath;
        response.fileName = msg.fileName;
        response.userName = msg.userName;

        switch(errorCode){

            case -1:
                response.type = "missingFolderError";
                response.body = "Folder doesn't exist!";
            break;

            case -2:
                response.type = "missingFileError";
                response.body = "File doesn't exist!";
            break;

            case -10:
                response.type = "messageParsingStringError";
                response.body = "An error happened during the parsing of the message received from the client";
            break;

            case -11:
                response.type = "updateBackupFileError";
                response.body = "An error happened during the update of server backup folder";
            break;

            case -20:
                response.type = "genericError";
                response.body = "A generic error";
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

    string serverBackupFolder, msgFolderPath, path;
    fs::path dstFolder, filePath;

    serverBackupFolder = server.backupFolder;
    msgFolderPath = msg.folderPath;

    //fs::path dstFolder = path, filePath;
    
    try {
        

        serverBackupFolder = server.backupFolder;
        msgFolderPath = msg.folderPath;


        #       ifdef BOOST_POSIX_API   //workaround for user-input files
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\\', '/');         
        #       else
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '/', '\\');         
        #       endif

        path = serverBackupFolder + separator() + msg.userName + separator() + msgFolderPath;

        dstFolder = path;

        if( ! fs::exists(dstFolder))
            fs::create_directories(dstFolder);

        path += separator() + msg.fileName;

        filePath = path;
        

        // if the file already exist doesn't need to be created
        if(fs::exists(filePath)){
                
            log -> info("the file " + path + " already exists: the output will be redirected on it");
            log -> flush();

        }

        int retryCount = 0;
        bool completed = false;

        while(!completed && retryCount < 5)
        {
            try
            {
                retryCount ++;
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

        if(!completed || retryCount >= 5){

            log -> error("file: " + path + " couldn't be updated because of internal errors ");
            log -> flush();
            return -11;

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

int Server::ClientConn::handleFileUpdate2(msg::message2 msg){

    string serverBackupFolder, msgFolderPath, path;
    fs::path dstFolder, filePath;

    serverBackupFolder = server.backupFolder;
    msgFolderPath = msg.folderPath;

    //fs::path dstFolder = path, filePath;
    
    try {
        

        serverBackupFolder = server.backupFolder;
        msgFolderPath = msg.folderPath;


        #       ifdef BOOST_POSIX_API   //workaround for user-input files
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\\', '/');         
        #       else
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '/', '\\');         
        #       endif

        path = serverBackupFolder + separator() + msg.userName + separator() + msgFolderPath;

        dstFolder = path;

        if( ! fs::exists(dstFolder))
            fs::create_directories(dstFolder);

        path +=  separator() + msg.fileName;

        filePath = path;
        
        // if the file already exist doesn't need to be created
        if(!fs::exists(filePath)){
                
            log -> info("the file " + path + " doesn't exists: the output will be redirected on it");
            log -> flush();

        }
        
        int retryCount = 0;
        bool completed = false;

        int i = 0;
        int sockFd = this -> sockObject -> GetSocketDescriptor();
        char buffer [BUFSIZ];

        FILE* file = fopen(filePath.c_str(), "w");

        // take file descriptor from FILE pointer 
        int fileFd = fileno(file); 

        do {

            i++;

            // reset char array
            memset(buffer, 0, BUFSIZ);

            int read_return = read(sockFd, buffer, BUFSIZ);
            if (read_return == -1) {
                
                log -> error("An error occured reading packet # " + to_string(i) + " from socket " + to_string(sockFd));
                return -1;

            }

            if (write(fileFd, buffer, read_return) == -1) {
                
                log -> error("An error occured writing packet # " + to_string(i) + " to file descriptor: " + to_string(fileFd));
                return -2;

            }       

        } while(i < msg.packetNumber);

        log -> info("file " + path + " correctly updated");
        log -> flush();

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

int Server::ClientConn::handleFileUpdate(msg::message msg){

    string serverBackupFolder, msgFolderPath, path;
    fs::path dstFolder, filePath;

    serverBackupFolder = server.backupFolder;
    msgFolderPath = msg.folderPath;

    //fs::path dstFolder = path, filePath;
    
    try {
        

        serverBackupFolder = server.backupFolder;
        msgFolderPath = msg.folderPath;


        #       ifdef BOOST_POSIX_API   //workaround for user-input files
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\\', '/');         
        #       else
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '/', '\\');         
        #       endif

        path = serverBackupFolder + separator() + msg.userName + separator() + msgFolderPath;

        dstFolder = path;

        if( ! fs::exists(dstFolder))
            fs::create_directories(dstFolder);

        path +=  separator() + msg.fileName;

        filePath = path;
        

        // if the file already exist doesn't need to be created
        if(!fs::exists(filePath)){
                
            log -> info("the file " + path + " doesn't exists: the output will be redirected on it");
            log -> flush();

        }
        
        int retryCount = 0;
        bool completed = false;

        while(!completed && retryCount < 5)
        {
            try
            {
                retryCount ++;
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

        if(!completed || retryCount >= 5){

            log -> error("file: " + path + " couldn't be updated because of internal errors ");
            log -> flush();
            return -11;

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

int Server::ClientConn::handleFileRename(msg::message2 msg){

    string serverBackupFolder, msgFolderPath, path, oldPathString, newPathString;
    fs::path dstFolder;

    try {

        serverBackupFolder = server.backupFolder;
        msgFolderPath = msg.folderPath;

        #       ifdef BOOST_POSIX_API   //workaround for user-input files
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\\', '/');         
        #       else
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '/', '\\');         
        #       endif

        path = serverBackupFolder + separator() + msg.userName + separator() + msgFolderPath;

        dstFolder = path;

        // if the path doesn't exist return because there isn't any files inside
        if( ! fs::exists(dstFolder))
            return -1;

        oldPathString = path + separator() + msg.fileName;
        newPathString = path + separator() + msg.body;

        fs::path oldPath = oldPathString;
        fs::path newPath = newPathString;

        // if the file doesn't exist return 
        if( ! fs::exists(oldPath)) {

            log -> error("file: " + oldPathString + " doesn't exist!");
            log -> flush();

            return -2;
        }

        bool completed = false;
        int retryCount = 0;
        while(!completed && retryCount < 5)
        {

            try
            {
                retryCount ++;
                fs::rename(oldPath, newPath);
                completed = true;
            }
            catch(...) 
            {
                log -> error("file: " + oldPathString + " currently used by others; sleep for 5 seconds and retry later ");
                log -> flush();
                sleep(5);
            }

        }

        if(!completed || retryCount >= 5){

            log -> error("file: " + oldPathString + " couldn't be updated because of internal errors ");
            log -> flush();

            return -11;

        }    

    } catch (...) {

        log -> error("an error occured handling file update: " + path);
        return -20;

    }

    log -> info("rename of file: " + oldPathString + " into " + newPathString + " completed ");
    log -> flush();

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::handleFileDelete(msg::message2 msg){
    string serverBackupFolder, msgFolderPath, path;
    fs::path dstFolder, filePath;

    serverBackupFolder = server.backupFolder;
    msgFolderPath = msg.folderPath;

    //fs::path dstFolder = path, filePath;
    
    try {
        

        serverBackupFolder = server.backupFolder;
        msgFolderPath = msg.folderPath;


        #       ifdef BOOST_POSIX_API   //workaround for user-input files
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '\\', '/');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '\\', '/');         
        #       else
                    std::replace(serverBackupFolder.begin(), serverBackupFolder.end(), '/', '\\');   
                    std::replace(msgFolderPath.begin(), msgFolderPath.end(), '/', '\\');         
        #       endif

        path = serverBackupFolder + separator() + msg.userName + separator() + msgFolderPath;

        dstFolder = path;

        if( ! fs::exists(dstFolder))
            return -1; // folder doesn't exist

        path +=  separator() + msg.fileName;

        filePath = path;
        

        // if the file already exist doesn't need to be created
        if(!fs::exists(filePath)){
                
            log -> error("the file " + path + " doesn't exists!");

            return -2; // file doesn't exist
        }

        int retryCount = 0;
        bool completed = false;

        while(!completed && retryCount < 5)
        {
            try
            {
                retryCount ++;
                fs::remove_all(path);
                completed = true;
            }
            catch(...) 
            {
                log -> error("file: " + path + " currently used by others; sleep for 5 seconds and retry later ");
                log -> flush();
                sleep(5);
            }
        }

        if(!completed || retryCount >= 5) {

            log -> error("file: " + path + " not updated because of internal error ");
            log -> flush();
            return -11;

        }

    } catch(...){

        log -> error("an error occured handling file update: " + path);
        return -20;

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
        
        this -> serv.activeConnections ++;
        // check if socket is closed
        if( !serv.isClosed(sock) ){

            msg::message2 msg;

            try{

                // set logger for client connection using the server log file
                if(initLogger() == 0){

                    // send response to CONNECTION MESSAGE by the client
                    msg.body = "socket accepted";
                    sendResponse2(0, msg);

                    // while loop to wait for different messages from client
                    waitForMessage2();

                    serv.unregisterClient(sock);
                    sockObject -> Close();

                    log -> info ("[CLIENT-CONN-" + to_string(this -> sock) + "]: exited from run");
                    log -> flush();

                }

            } catch (exception e){

                string error = e.what();

                serv.unregisterClient(sock);
                sockObject -> Close();
                
                cout << error << endl;

            }

        } else {

            // can't log because the logger wasn't initialized; in any case the socket will be closed
            serv.unregisterClient(sock);
            sockObject -> Close();

        }
        
    });

    inboundChannel.detach();
    
}