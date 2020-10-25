
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
        jsonMSG.at("packetNumber").get_to(message.packetNumber);
        /*jsonMSG.at("timestamp").get_to(message.timestamp);
        jsonMSG.at("hash").get_to(message.hash);*/
        jsonMSG.at("body").get_to(message.body);

        msg.clear();
        //msg = "ok";

    } catch (...) {

        msg.clear();
        msg = "Error parsing string received into Message Object";
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

int Server::ClientConn::selective_search(string & response, string buf, msg::message2 & msg)
{
    string serverBackupFolder, msgFolderPath, path;

    try{

        // reset response string
        response.clear();
        buf.clear();

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

        response = "[]";
        buf = "unexpected error initializing recursive search locally to the server";
        return -1;

    }

    boost::filesystem::path dstFolder = path;

    if( !boost::filesystem::exists(dstFolder)){

        response = "[]";
        buf = "path doesn't exists -> no configuration available";
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

        response = "[]";
        buf = "unexpected error during recursive search -> no configuration available";
        return -2;

    }

    buf = "server configuration read correctly";
    return 0;

}

int32 Server::ClientConn::SendInitialConf(string conf, int32 nOutFd, int32 nCount)
{
    int32  nOutCount = 0;

    char szData[SOCKET_SENDFILE_BLOCKSIZE];
    int32       nInCount = 0;

    while (nOutCount < nCount)
    {
        nInCount = (nCount - nOutCount) < SOCKET_SENDFILE_BLOCKSIZE ? (nCount - nOutCount) : SOCKET_SENDFILE_BLOCKSIZE;

        // iterate over the string piece by piece
        string subConfString = conf.substr(nOutCount, nInCount);

        if ((SEND(nOutFd, subConfString.c_str(), nInCount, 0)) != (int32)nInCount)
        {
            conf.clear();
            return -1;
        }

        nOutCount += nInCount;
    }

    conf.clear();
    return nOutCount;
}

int Server::ClientConn::sendResponse2(int resCode, string buf, msg::message2 msg) {
        
    msg::message2 response;
    string responseString;
    int sendCode = 0;

    response.body = buf;

    if(resCode == 0)
        handleOkResponse(response, msg);
                        
    else 
        handleErrorResponse(response, msg, resCode);

    resCode = fromMessageToString(responseString, response);

    if(resCode == 0) {

        sendCode = this -> sockObject -> Send((const uint8 *) responseString.c_str(), responseString.length());

        if(sendCode <= 0){
            return -1;
        }

    } else {

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

/*

Receive message from the server

RETURN:

 0 -> no error
-1 -> message error on receive
-2 -> socket was closed
-3 -> unexpected error

*/
int Server::ClientConn::readMessage2(int fd, string & bufString) {

    uint64_t rcvDataLength;
    uint8_t rcvBuf [SOCKET_SENDFILE_BLOCKSIZE];    // Allocate a receive buffer
    int rcvCode = 0;
    fd_set set;
    struct timeval timeout;
    int iResult = 0;

    try {

        bufString.clear();

        // Receive the message header
        memset(rcvBuf, 0, SOCKET_SENDFILE_BLOCKSIZE);
        rcvCode = this -> sockObject -> Receive(SOCKET_SENDFILE_BLOCKSIZE, rcvBuf);

        // Receive message error
        if(rcvCode <= 0) {

            sockObject -> TranslateSocketError();
            string sockError = sockObject -> DescribeError();
            
            bufString = sockError;

            this -> running.store(false);
            return -2;

        }

        string respString = (char *) rcvBuf;

        bufString = respString;
        
    } catch (...) {

        bufString = "Unexpected error reading message";
        return -3;

    }

    //bufString = "Message correctly read";
    return 0;

}

void Server::ClientConn::updateFile(int& resCode, string buf, msg::message2& msg){

    resCode = sendResponse2(resCode, buf, msg);
    log -> info ("[SND HEADER RESP]: returned code: " + to_string(resCode));
    log -> flush();
    if(resCode < 0) goto checkCode;
                  
    resCode = handleFileUpdate2(msg, buf);
    log -> info ("[RCV STREAM]: returned code: " + to_string(resCode));
    log -> flush();
    if(resCode < 0) goto checkCode;

    resCode = sendResponse2(resCode, buf, msg); 
    log -> info ("[SND STREAM RESP]: returned code: " + to_string(resCode));
    log -> flush();

    checkCode:

    // if there are errors on sending, the thread will exit
    if(resCode < 0) {

        this -> running.store(false);
        return;

    }

    return;

}

void Server::ClientConn::renameFile(int& resCode, string buf, msg::message2& msg){

    resCode = handleFileRename(msg, buf);
    log -> info ("[RENAME]: returned code: " + to_string(resCode));
    log -> flush();
    if(resCode < 0) goto checkCode;

    resCode = sendResponse2(resCode, buf, msg);
    log -> info ("[SND HEADER RESP]: returned code: " + to_string(resCode));
    log -> flush();

    checkCode:

    // if there are errors on sending, the thread will exit
    if(resCode < 0) {

        this -> running.store(false);
        return;

    }
}

void Server::ClientConn::deleteFile(int& resCode, string buf, msg::message2& msg){

    resCode = handleFileDelete(msg, buf);
    log -> info ("[DELETE]: returned code: " + to_string(resCode));
    log -> flush();
    if(resCode < 0) goto checkCode;

    resCode = sendResponse2(resCode, buf, msg); 
    log -> info ("[SND HEADER RESP]: returned code: " + to_string(resCode));
    log -> flush();

    checkCode:

    // if there are errors on sending, the thread will exit
    if(resCode < 0) {

        this -> running.store(false);
        return;

    }

}

void Server::ClientConn::initialConfiguration(int& resCode, string buf, msg::message2& msg, string initialConf){

    initialConf.clear();

    resCode = selective_search(initialConf, buf, msg);

    if(resCode == 0) {

        int div = initialConf.length() / SOCKET_SENDFILE_BLOCKSIZE;
        int rest = initialConf.length() % SOCKET_SENDFILE_BLOCKSIZE;

        if(rest > 0)
            div ++;

        // set # packets used 
        msg.packetNumber = div;

        resCode = sendResponse2(resCode, buf, msg); 
        log -> info ("[SND HEADER CONF]: returned code: " + to_string(resCode));
        log -> flush();
        if (resCode < 0) goto checkCode;
        
        resCode = readMessage2(sock, buf);          
        if(resCode < 0) goto checkCode;

        resCode = fromStringToMessage(buf, msg);
        log -> info ("[RCV HEADER CONF RESP]: returned code: " + to_string(resCode));
        log -> flush();    
        if(resCode < 0) goto checkCode;

        resCode = SendInitialConf(initialConf, this -> sockObject -> GetSocketDescriptor(), initialConf.length());
        log -> info ("[SND STREAM]: returned code: " + to_string(resCode));
        log -> flush();
        if(resCode < 0) goto checkCode;

        resCode = readMessage2(sock, buf);          
        if(resCode < 0) goto checkCode;

        resCode = fromStringToMessage(buf, msg);
        log -> info ("[RCV STREAM RESP]: returned code: " + to_string(resCode));
        log -> flush();
        if(resCode < 0) goto checkCode;
        
        // if there are errors on the selective search i don't continue with other packets
        resCode = sendResponse2(resCode, buf, msg);     

    } 

    checkCode: 

    // if there are errors on sending, the thread will exit
    if(resCode < 0) {

        this -> running.store(false);
        return;

    }

}

void Server::ClientConn::waitForMessage2(){

    msg::message2 msg;
    int resCode;
    uint64_t sizeNumber;
    string initialConf;

    // declaration of response message
    msg::message response;

    while(running.load()){   

        try {

            log -> info("");
            log -> info ("wait for message from the client");
            log -> flush();

            string buf;
            
            milliseconds ms = duration_cast< milliseconds >(
                system_clock::now().time_since_epoch()
            );

            this -> activeMS.store(ms.count());

            buf.clear();
            resCode = readMessage2(sock, buf);

            if (resCode == 0){
                
                ms = duration_cast< milliseconds >(
                    system_clock::now().time_since_epoch()
                );

                this -> activeMS.store(ms.count());

                resCode = fromStringToMessage(buf, msg);

                if(resCode == 0){
                    
                    log -> info("");
                    log -> info ("[RCV HEADER]: message " + to_string(msg.typeCode) + " parsed correctly" );
                    log -> flush();

                    switch(msg.typeCode){
                                
                        // file update
                        case 1:
                        case 3:

                            updateFile(resCode, buf, msg);

                        break;

                        // file rename
                        case 2:

                            renameFile(resCode, buf, msg);

                        break;

                        // file delete
                        case 4:

                            deleteFile(resCode, buf, msg);

                        break;

                        // initial configuration
                        case 5:

                            initialConfiguration(resCode, buf, msg, initialConf);

                        break;

                        // close connection
                        case 6:

                            running.store(false);

                            resCode = sendResponse2(resCode, buf, msg); 
                            
                            log -> info ("[SND HEADER RESP]: returned code: " + to_string(resCode));
                            log -> flush();

                            // if there are errors on sending, the thread will exit
                            if(resCode < 0) 
                                this -> running.store(false);

                        break;
                        
                    }

                } else {

                    running.store(false);
                    log -> info("[RCV HEADER]: error parsing message received from client");

                }

            } else { 

                running.store(false);
                log -> info("[RCV HEADER]: error receiving message from client");

            }

        } catch (...) {

            running.store(false);
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

int Server::ClientConn::handleOkResponse(msg::message2 & response, msg::message2 & msg){

    try{

        response.type = "ok";
        response.typeCode = 0;
        response.folderPath = msg.folderPath;
        response.fileName = msg.fileName;
        response.userName = msg.userName;
        response.packetNumber = 1;
        response.body = "";

    } catch (...) {

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
        response.packetNumber = 1;
        response.type = "error";

    } 
    catch (...) 
    {
        
        return -1;

    }

    return 0;

}

/*

RETURN:

 0 ---> no error
-1 ---> error parsing message json
-2 ---> error sending message LENGTH
-3 ---> error sending message DATA

*/

int Server::ClientConn::sendFileStream(string filePath){

    json jMsg;
    int sendCode = 0;
    off_t offset = 0;
    int socket = this -> sockObject -> GetSocketDescriptor();
    
    try{

        FILE* file = fopen(filePath.c_str(), "r");

        if(file == NULL){

            return -1;

        }

        // take file descriptor from FILE pointer 
        int fd = fileno(file); 

        if(serv.isClosed(socket)){

            return -2;

        }

        // obtain file size
        fseek(file, 0L, SEEK_END);
        int fileSize = ftell(file);    

        int sendFileReturnCode = this -> sockObject -> SendFile(this -> sockObject -> GetSocketDescriptor(), fd, &offset, fileSize);

        if(sendFileReturnCode < 0){

            return -3;

        } 

    } catch(...){

        return -4;

    }

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::handleFileUpdate2(msg::message2 msg, string buf){

    string serverBackupFolder, msgFolderPath, path;
    fs::path dstFolder, filePath;

    serverBackupFolder = server.backupFolder;
    msgFolderPath = msg.folderPath;
    
    try {
        
        buf.clear();

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
                
                buf = "Error reading bytes from file-string received";
                return -1;

            }

            if (write(fileFd, buffer, read_return) == -1) {
                
                buf = "Error updating bytes into server-local file";
                return -2;

            }   


        } while(i < msg.packetNumber);

    } catch (...) {

        buf = "Unexpected error updating file";
        return -20;

    }

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::handleFileRename(msg::message2 msg, string buf){

    string serverBackupFolder, msgFolderPath, path, oldPathString, newPathString;
    fs::path dstFolder;

    try {

        buf.clear();

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

            buf = "file doesn't exists locally to the server";
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
                sleep(5);
            }

        }

        if(!completed || retryCount >= 5){

            buf = "couldn't be able to complete renaming-operation";
            return -11;

        }    

    } catch (...) {

        buf = "unexpected error during the renaming-operation";
        return -20;

    }

    return 0;

}

/*

RETURN:

*/

int Server::ClientConn::handleFileDelete(msg::message2 msg, string buf){
    string serverBackupFolder, msgFolderPath, path;
    fs::path dstFolder, filePath;

    serverBackupFolder = server.backupFolder;
    msgFolderPath = msg.folderPath;
    
    try {
        
        buf.clear();

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

        // folder doesn't exist
        if( ! fs::exists(dstFolder)){

            buf = "folder doesn't exist locally to the server";
            return -1; 

        }

        path +=  separator() + msg.fileName;

        filePath = path;
        

        // the file doesn't exist
        if(!fs::exists(filePath)){
            
            buf = "file doesn't exist locally to the server";
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
                sleep(5);
            }
        }

        if(!completed || retryCount >= 5) {

            buf = "couldn't be able to complete delete-operation";
            return -11;

        }

    } catch(...){

        buf = "unexpected error during delete-operation";
        return -20;

    }

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
                    sendResponse2(0, "Socket Accepted", msg);

                    // while loop to wait for different messages from client
                    waitForMessage2();

                    serv.unregisterClient(sock);
                    sockObject -> Close();

                    log -> info ("[CLIENT-CONN-" + to_string(this -> sock) + "]: exited from run\n");
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