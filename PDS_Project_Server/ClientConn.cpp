
#include "server.hpp"
#include <algorithm>
#include <vector>


#ifdef _WIN32

#include <winsock2.h>
#include <Windows.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#else

#include <arpa/inet.h>
#include "boost/filesystem.hpp"
namespace fs = std::experimental::filesystem;

#endif
//using namespace boost::filesystem;


//////////////////////////////////
//        PUBLIC METHODS        //
//////////////////////////////////


/*

Client Connection Thread handle.

*/
void Server::ClientConn::handleConnection() {

    std::thread inboundChannel([this]() {

        this->serv.activeConnections++;
        // check if socket is closed
        if (!serv.isClosed(sock)) {

            message2 msg;

            try {

                // set logger for client connection using the server log file
                if (initLogger() == 0) {

                    // send response to CONNECTION MESSAGE by the client
                    msg.body = "socket accepted";
                    sendResponse(0, "Socket Accepted", msg);

                    // while loop to wait for different messages from client
                    waitForMessage();

                    serv.unregisterClient(sock);
                    //sockObject->Close();


                }

            }
            catch (...) {

                 //std::string error = e.what();

                serv.unregisterClient(sock);
                sockObject->Close();

                //std::cout << error << std::endl;

            }

        }
        else {

            // can't log because the logger wasn't initialized; in any case the socket will be closed
            std::cout << "Errore handle" << std::endl;
            serv.unregisterClient(sock);
            sockObject->Close();

        }

        });

    inboundChannel.detach();

}


///////////////////////////////////
//        PRIVATE METHODS        //
///////////////////////////////////


/*

Initialize Client Connection Logger.

RETURN:

 0 -> no error
-1 -> logger initialization error
-2 -> generic error
-3 -> unexpected error

*/
int Server::ClientConn::initLogger() {

    try
    {
        this->log = spdlog::basic_logger_mt("client_" + std::to_string(this->sock), "client_" + std::to_string(this->sock) + ".txt");
        this->log->info("Logger initialized correctly");
        this->log->flush();
        /*
        this->log = spdlog::get("client_" + std::to_string(this->sock));

        if (this->log == nullptr)
            this->log = spdlog::basic_logger_mt("client_" + std::to_string(this->sock), logFile);

        this->log->info("Logger initialized correctly");
        this->log->flush();*/
    }
    catch (const spdlog::spdlog_ex& ex)
    {
         std::string error = ex.what();
         std::cerr << "error during logger initialization: " + error << std::endl;
        return -1;
    }
    catch (const std::exception& ex)
    {
         std::string error = ex.what();
         std::cerr << "generic error during logger initialization: " + error << std::endl;
        return -2;
    }
    catch (...)
    {
        std::cerr << "Unexpected error appened during logger initialization" << std::endl;
        return -3;
    }

    return 0;

}

/*

Wait for client file-message.
If socket or connection errors happen, it will close the socket.

*/
void Server::ClientConn::waitForMessage() {

    message2 msg;
    int resCode;
    uint64_t sizeNumber;
     std::string initialConf;

    // declaration of response message
    message2 response;

    while (running.load()) {

        try {

            log->info("");
            log->info("wait for message from the client");
            log->flush();

             std::string buf;

            milliseconds ms = duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()
                );

            this->activeMS.store(ms.count());

            buf.clear();
            resCode = readMessage(sock, buf);

            if (resCode == 0) {

                ms = duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch()
                    );

                this->activeMS.store(ms.count());

                resCode = fromStringToMessage(buf, msg);

                if (resCode == 0) {

                    log->info("");
                    log->info("[RCV HEADER]: message " + std::to_string(msg.typeCode) + " parsed correctly");
                    log->flush();

                    switch (msg.typeCode) {

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

                        resCode = sendResponse(resCode, buf, msg);

                        log->info("[SND HEADER RESP]: returned code: " + std::to_string(resCode));
                        log->flush();

                        // if there are errors on sending, the thread will exit
                        if (resCode < 0)
                            this->running.store(false);

                        break;

                    }

                }
                else {

                    running.store(false);
                    log->info("[RCV HEADER]: error parsing message received from client");

                }

            }
            else {

                running.store(false);
                log->info("[RCV HEADER]: error receiving message from client");

            }

        }
        catch (...) {

            running.store(false);
            log->error("unexpected error happened");
            log->info("going to sleep for 1 second because of error");

#ifdef _WIN32
            Sleep(1000);
#else
            sleep(1);
#endif
            return;

        }

        log->info("[CLIENT-CONN-" + std::to_string(this->sock) + "]: exited from run\n");
        log->flush();

    }


}

/*

Receive a message from the server.

RETURN:

 0 -> no error
-1 -> message error on receive
-2 -> unexpected error

*/
int Server::ClientConn::readMessage(int fd,  std::string& bufString) {

    uint8_t rcvBuf[PACKET_SIZE];    // Allocate a receive buffer
    int rcvCode = 0;
    fd_set set;
    struct timeval timeout;
    int iResult = 0;

    try {

        bufString.clear();

        // Receive the message header
        memset(rcvBuf, 0, PACKET_SIZE);
        rcvCode = this->sockObject->Receive(PACKET_SIZE, rcvBuf);

        // Receive message error
        if (rcvCode <= 0) {

            sockObject->TranslateSocketError();
             std::string sockError = sockObject->DescribeError();

            bufString = sockError;

            this->running.store(false);
            return -1;

        }

         std::string respString = (char*)rcvBuf;

        bufString = respString;

    }
    catch (...) {

        bufString = "Unexpected error reading message";
        return -2;

    }

    return 0;

}

/*

Read the file stream from the client.

RETURN:

0  -> no error
-1 -> error reading from socket
-2 -> error updating server-local file
-3 -> unexpected error

*/
int Server::ClientConn::readFileStream(int packetsNumber, int fileFd) {

    // per ogni stream ricevuto dal client scrivo su un file temporaneo
    // se lo stream è andato a buon fine e ho ricevuto tutto faccio una copia dal file temporaneo a quello ufficiale
    // ed eliminio il file temporaneo

    int i = 0;
    int sockFd = this->sockObject->GetSocketDescriptor();
    char buffer[BUFSIZ];

    try {

        do {

            i++;

            // reset char array
            memset(buffer, 0, BUFSIZ);

            int read_return = read(sockFd, buffer, BUFSIZ);
            if (read_return == -1) {

                log->error("An error occured reading packet # " + std::to_string(i) + " from socket " + std::to_string(sockFd));
                return -1;

            }

            if (write(fileFd, buffer, read_return) == -1) {

                log->error("An error occured writing packet # " + std::to_string(i) + " to file descriptor: " + std::to_string(fileFd));
                return -2;

            }

        } while (i < packetsNumber);

    }
    catch (...) {

        return -3;

    }

    return 0;

}

/*

Update the file on the server side.

*/
void Server::ClientConn::updateFile(int& resCode,  std::string buf, message2& msg) {

    resCode = sendResponse(resCode, buf, msg);
    log->info("[SND HEADER RESP]: returned code: " + std::to_string(resCode));
    log->flush();
    if (resCode < 0) goto checkCode;

    resCode = handleFileUpdate(msg, buf);
    log->info("[RCV STREAM]: returned code: " + std::to_string(resCode));
    log->flush();
    if (resCode < 0) goto checkCode;

    resCode = sendResponse(resCode, buf, msg);
    log->info("[SND STREAM RESP]: returned code: " + std::to_string(resCode));
    log->flush();

checkCode:

    // if there are errors on sending, the thread will exit
    if (resCode < 0) {

        this->running.store(false);
        return;

    }

    return;

}

/*

Rename the file on the server-side.

*/
void Server::ClientConn::renameFile(int& resCode,  std::string buf, message2& msg) {

    resCode = handleFileRename(msg, buf);
    log->info("[RENAME]: returned code: " +  std::to_string(resCode));
    log->flush();
    if (resCode < 0) goto checkCode;

    resCode = sendResponse(resCode, buf, msg);
    log->info("[SND HEADER RESP]: returned code: " +  std::to_string(resCode));
    log->flush();

checkCode:

    // if there are errors on sending, the thread will exit
    if (resCode < 0) {

        this->running.store(false);
        return;

    }
}

/*

Delete the file on the server-side.

*/
void Server::ClientConn::deleteFile(int& resCode,  std::string buf, message2& msg) {

    resCode = handleFileDelete(msg, buf);
    log->info("[DELETE]: returned code: " +  std::to_string(resCode));
    log->flush();
    if (resCode < 0) goto checkCode;

    resCode = sendResponse(resCode, buf, msg);
    log->info("[SND HEADER RESP]: returned code: " +  std::to_string(resCode));
    log->flush();

checkCode:

    // if there are errors on sending, the thread will exit
    if (resCode < 0) {

        this->running.store(false);
        return;

    }

}

/*

Obtain the actual configuration on the server-side.

*/
void Server::ClientConn::initialConfiguration(int& resCode,  std::string buf, message2& msg,  std::string initialConf) {

    initialConf.clear();

    resCode = selective_search(initialConf, buf, msg);

    if (resCode == 0) {

        int div = initialConf.length() / SOCKET_SENDFILE_BLOCKSIZE;
        int rest = initialConf.length() % SOCKET_SENDFILE_BLOCKSIZE;

        if (rest > 0)
            div++;

        // set # packets used 
        msg.packetNumber = div;

        resCode = sendResponse(resCode, buf, msg);
        log->info("[SND HEADER CONF]: returned code: " +  std::to_string(resCode));
        log->flush();
        if (resCode < 0) goto checkCode;

        resCode = readMessage(sock, buf);
        if (resCode < 0) goto checkCode;

        resCode = fromStringToMessage(buf, msg);
        log->info("[RCV HEADER CONF RESP]: returned code: " +  std::to_string(resCode));
        log->flush();
        if (resCode < 0) goto checkCode;

        resCode = SendInitialConf(initialConf, this->sockObject->GetSocketDescriptor(), initialConf.length());
        log->info("[SND STREAM]: returned code: " +  std::to_string(resCode));
        log->flush();
        if (resCode < 0) goto checkCode;

        resCode = readMessage(sock, buf);
        if (resCode < 0) goto checkCode;

        resCode = fromStringToMessage(buf, msg);
        log->info("[RCV STREAM RESP]: returned code: " +  std::to_string(resCode));
        log->flush();
        if (resCode < 0) goto checkCode;

        // if there are errors on the selective search i don't continue with other packets
        resCode = sendResponse(resCode, buf, msg);

    }

checkCode:

    // if there are errors on sending, the thread will exit
    if (resCode < 0) {

        this->running.store(false);
        return;

    }

}

/*

Handle the File Update operation.

RETURN:

 0 -> no error
-1 -> error reading packet from socket
-2 -> error updating local-server file
-3 -> unexpected error

*/
int Server::ClientConn::handleFileUpdate(message2 msg,  std::string buf) {

     std::string serverBackupFolder, msgFolderPath, path;
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

        if (!fs::exists(dstFolder))
            fs::create_directories(dstFolder);

        path += separator() + msg.fileName;

        filePath = path;

        int retryCount = 0;
        bool completed = false;

        int i = 0;
        int sockFd = this->sockObject->GetSocketDescriptor();
        char buffer[BUFSIZ];

        FILE* file = fopen(path.c_str(), "w");

        // take file descriptor from FILE pointer 
        int fileFd = fileno(file);

        do {

            i++;

            // reset char array
            memset(buffer, 0, BUFSIZ);

            int read_return = read(sockFd, buffer, BUFSIZ);
            if (read_return == -1) {

                buf = "Error reading bytes from file- std::string received";
                return -1;

            }

            if (write(fileFd, buffer, read_return) == -1) {

                buf = "Error updating bytes into server-local file";
                return -2;

            }


        } while (i < msg.packetNumber);

    }
    catch (...) {

        buf = "Unexpected error updating file";
        return -3;

    }

    return 0;

}

/*

Handle the File Delete operation.

RETURN:

 0 -> no error
-1 -> folder doesn't exists locally to the server
-2 -> file doesn't exists locally to the server
-3 -> delete operation couldn't be completed
-4 -> unexpected error

*/
int Server::ClientConn::handleFileDelete(message2 msg,  std::string buf) {

     std::string serverBackupFolder, msgFolderPath, path;
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
        if (!fs::exists(dstFolder)) {

            buf = "folder doesn't exist locally to the server";
            return -1;

        }

        path += separator() + msg.fileName;

        filePath = path;


        // the file doesn't exist
        if (!fs::exists(filePath)) {

            buf = "file doesn't exist locally to the server";
            return -2; // file doesn't exist

        }

        int retryCount = 0;
        bool completed = false;

        while (!completed && retryCount < 5)
        {
            try
            {
                retryCount++;
                fs::remove_all(path);
                completed = true;
            }
            catch (...)
            {
#ifdef _WIN32
                Sleep(5000);
#else
                sleep(5);
#endif
            }
        }

        if (!completed || retryCount >= 5) {

            buf = "couldn't be able to complete delete-operation";
            return -3;

        }

    }
    catch (...) {

        buf = "unexpected error during delete-operation";
        return -4;

    }

    return 0;

}

/*

Handle the File Rename operation.

RETURN:

 0 -> no error
-1 -> folder doesn't exists locally to the server
-2 -> file doesn't exists locally to the server
-3 -> rename operation couldn't be completed
-4 -> unexpected error

*/
int Server::ClientConn::handleFileRename(message2 msg,  std::string buf) {

     std::string serverBackupFolder, msgFolderPath, path, oldPathString, newPathString;
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
        if (!fs::exists(dstFolder))
            return -1;

        oldPathString = path + separator() + msg.fileName;
        newPathString = path + separator() + msg.body;

        fs::path oldPath = oldPathString;
        fs::path newPath = newPathString;

        // if the file doesn't exist return 
        if (!fs::exists(oldPath)) {

            buf = "file doesn't exists locally to the server";
            return -2;

        }

        bool completed = false;
        int retryCount = 0;
        while (!completed && retryCount < 5)
        {

            try
            {
                retryCount++;
                fs::rename(oldPath, newPath);
                completed = true;
            }
            catch (...)
            {
#ifdef _WIN32
                Sleep(5000);
#else
                sleep(5);
#endif
            }

        }

        if (!completed || retryCount >= 5) {

            buf = "couldn't be able to complete renaming-operation";
            return -3;

        }

    }
    catch (...) {

        buf = "unexpected error during the renaming-operation";
        return -4;

    }

    return 0;

}

/*

Recursive search in order to get the local configuration.

RETURN:

0 -> no error
-1 -> error initializing recursive search locally to the server
-2 -> error during recursive search

*/
int Server::ClientConn::selective_search( std::string& response,  std::string buf, message2& msg)
{
     std::string serverBackupFolder, msgFolderPath, path;

    try {

        // reset response  std::string
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


    }
    catch (...) {

        response = "[]";
        buf = "unexpected error initializing recursive search locally to the server";
        return -1;

    }

    boost::filesystem::path dstFolder = path;

    if (!boost::filesystem::exists(dstFolder)) {

        response = "[]";
        buf = "path doesn't exists -> no configuration available";
        return 0;

    }

    try
    {

        fs::recursive_directory_iterator dir(path), end;
        auto jsonObjects = json::array();

        // iterating over the local folder of the client
        while (dir != end)
        {
            initialConf conf{
                dir->path().filename().string(),
                compute_hash(dir->path().filename().string())
            };

            json obj = json{ {"path", conf.path},{"hash", conf.hash} };
            std::string  stringa = obj.dump();
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

/*

Compute the file hash.

RETURN:

( std::string) -> hash  std::string or empty  std::string in case of error.

*/
std::string Server::ClientConn::compute_hash(const std::string file_path)
{
    try
    {

        std::string result;

        CryptoPP::Weak::MD5 hash;
        CryptoPP::FileSource(file_path.c_str(), true, new
            CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(new
                CryptoPP::StringSink(result), false)));

        return result;

    }
    catch (...) {

        log->error("an error happened creating hash code from " + file_path);
        return "";

    }

}

/*

Send response message to the client.

RETURN:

0 -> no error
-1 -> error sending response to the client
-2 -> error parsing response into  std::string
-3 -> unexpected error

*/
int Server::ClientConn::sendResponse(int resCode,  std::string buf, message2 msg) {

    message2 response;
     std::string responseString;
    int sendCode = 0;

    try {

        response.body = buf;

        if (resCode == 0)
            handleOkResponse(response, msg);

        else
            handleErrorResponse(response, msg, resCode);

        resCode = fromMessageToString(responseString, response);

        if (resCode == 0) {

            sendCode = this->sockObject->Send((const uint8*)responseString.c_str(), responseString.length());

            if (sendCode <= 0) {

                return -1;

            }

        }
        else {

            return -2;

        }

    }
    catch (...) {

        return -3;

    }

    return 0;

}

/*

Send initial configuration to the client side.

RETURN:

0 -> no error
-1 -> error sending configuration Packet
-2 -> unexpected error

*/
int32 Server::ClientConn::SendInitialConf( std::string conf, int32 nOutFd, int32 nCount)
{

    int32  nOutCount = 0;
    int32       nInCount = 0;
    char szData[PACKET_SIZE];

    try {

        while (nOutCount < nCount)
        {
            nInCount = (nCount - nOutCount) < PACKET_SIZE ? (nCount - nOutCount) : PACKET_SIZE;

            // iterate over the  std::string piece by piece
             std::string subConfString = conf.substr(nOutCount, nInCount);

            if ((SEND(nOutFd, subConfString.c_str(), nInCount, 0)) != (int32)nInCount)
            {
                conf.clear();
                return -1;
            }

            nOutCount += nInCount;
        }

        conf.clear();

    }
    catch (...) {

        return -2;

    }

    return nOutCount;
}

/*

Handle Ok Response if there were no error.

RETURN:

 0 -> no error
-1 -> unexpected error

*/
int Server::ClientConn::handleOkResponse(message2& response, message2& msg) {

    try {

        response.type = "ok";
        response.typeCode = 0;
        response.folderPath = msg.folderPath;
        response.fileName = msg.fileName;
        response.userName = msg.userName;
        response.packetNumber = 1;
        response.body = "";

    }
    catch (...) {

        return -1;

    }

    return 0;

}

/*

Handle Error Response in case of error.

RETURN:

0 -> no error
-1 -> unexpected error

*/
int Server::ClientConn::handleErrorResponse(message2& response, message2& msg, int errorCode) {

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

Convert the  std::string received into a message object.

RETURN:

0 -> no error
-1 -> unexpected error

*/
int Server::ClientConn::fromStringToMessage( std::string msg, message2& message) {

    try {

        auto jsonMSG = json::parse(msg);

        jsonMSG.at("type").get_to(message.type);
        jsonMSG.at("typeCode").get_to(message.typeCode);
        jsonMSG.at("folderPath").get_to(message.folderPath);
        jsonMSG.at("userName").get_to(message.userName);
        jsonMSG.at("fileName").get_to(message.fileName);
        jsonMSG.at("packetNumber").get_to(message.packetNumber);
        //if(jsonMSG.at("timestamp") != NULL) jsonMSG.at("timestamp").get_to(message.timestamp);
        //jsonMSG.at("hash").get_to(message.hash);
        jsonMSG.at("body").get_to(message.body);

        msg.clear();
        //msg = "ok";

    }
    catch (...) {

        msg.clear();
        msg = "Error parsing  std::string received into Message Object";
        return -1;

    }

    return 0;

}

/*

Convert the message received into a  std::string, in order to be sent through a socket channel.

RETURN:

0 -> no error
-1 -> unexpected error

*/
int Server::ClientConn::fromMessageToString( std::string& messageString, message2& msg) {

    try
    {
        json jMsg = json{ {"packetNumber", msg.packetNumber},{"userName", msg.userName},{"type", msg.type}, {"typeCode", msg.typeCode}, {"fileName", msg.fileName}, {"folderPath", msg.folderPath}, {"body", msg.body} };
        messageString = jMsg.dump();
    }
    catch (...)
    {

        log->error("An error appened converting the message to  std::string: " + msg.type);
        log->flush();

        return -1;
    }

    return 0;

}

