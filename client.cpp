
#include "client.hpp"

#define PACKET_SIZE 4096

/* 

RETURN:

 0   ---> no error
-1   ---> error opening/creating the file
-2   ---> error converting the JSON
-3   ---> error saving the JSON inside the struct

*/

int Client::readConfiguration () {


    ifstream userConfFile ("userConfiguration.json");
    json jUserConf;

    if(!userConfFile)
    {
        cerr << "User Configuration File could not be opened!\n"; // Report error
        cerr << "Error code: " << strerror(errno); // Get some info as to why
        return -1;
    }

    if(!(userConfFile >> jUserConf))
    {
        cerr << "The User Configuration File couldn't be parsed";
        return -2; 
    }

    // save the user configuration inside a local struct
    try {

        uc = conf::user 
        {
            jUserConf["serverIp"].get<string>(),
            jUserConf["serverPort"].get<string>(),
            jUserConf["name"].get<string>(),
            jUserConf["folderPath"].get<string>(),
            jUserConf["secondTimeout"].get<int>()
        };

    } catch (...) {

        cerr << "Error during the saving of the configuration locally to CLIENT";
        return -3;

    }

    return 0;

}

/* 

RETURN:

 0   ---> no error
-1   ---> error opening/creating the log file

*/

int Client::initLogger () {

    // Logger initialization

    try 
    {
        myLogger = spdlog::basic_logger_mt(uc.name, uc.name+"_log.txt");
        myLogger -> info("Logger initialized correctly with file " + uc.name+"_log.txt");
        myLogger -> flush();
    } 
    catch (const spdlog::spdlog_ex &ex) 
    {
        cout << ex.what() << endl;
        return -1;
    }

    return 0;

}

/*

Check if the connection with the server is CLOSED

*/

bool Client::isClosed(){

    if(!this->socketObj.IsSocketValid())
        return true;

    int sock = this->socketObj.GetSocketDescriptor();
    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);
    timeval tv = {0};
    select(sock + 1, &rfd, 0, 0, &tv);

    if (!FD_ISSET(sock, &rfd))
        return false;
    
    int n = 0;
    ioctl(sock, FIONREAD, &n);

    return n == 0;

}

/*

RETURN:

 0 ---> no error
-1 ---> error creating socket
-2 ---> socket closed

*/

int Client::serverConnection () {

    // Create socket
    string response;

    if(this -> socketObj.Initialize()){

        myLogger -> info ("Initialized socket object");
        myLogger -> flush();

    } else {

        myLogger -> error ("Error Initializing socket object");
        return -1;
    }

    socketObj.SetBlocking();

    socketObj.SetSendTimeout(uc.secondTimeout);
    socketObj.SetReceiveTimeout(uc.secondTimeout);
    socketObj.SetConnectTimeout(uc.secondTimeout);

    // convert port into INT VALUE
    int port = stoi(this -> uc.serverPort);

    while(! this -> socketObj.Open(this -> uc.serverIp.c_str() , port)){

        myLogger -> info ("1 Can't connect to server " + uc.serverIp + " port: " + uc.serverPort);
        myLogger -> info ("1 Try again to connect in 5 seconds");
        myLogger -> flush();

        sleep(5);

    }

    while(readMessageResponse2(response) < 0){

        if(isClosed()){

            // the connection was closed so exit
            myLogger -> info ("The socket was closed on the server side");
            myLogger -> flush();

            this -> socketObj.Close();

            return -2;

        } else {

            myLogger -> info ("2 Can't connect to server " + uc.serverIp + " port: " + uc.serverPort);
            myLogger -> info ("2 Try again to connect in 5 seconds");
            myLogger -> flush();

            sleep(5);

        }

    }

    return 0;
}

/*

RETURN:

 0 -> no error
-1 -> error sending disconnection request
-2 -> error receiving disconnection request RESPONSE
-3 -> error CLOSING SOCKET

*/

int Client::serverDisconnection () {

    int resCode = 0;
    myLogger -> info("");
    myLogger -> info ("try to disconnect from server - IP: " + uc.serverIp + " PORT: " + uc.serverPort);
    myLogger -> flush();

    string response;
    msg::message2 fcu2 = {

            "",
            6,
            0, // timestamp
            "",// hash
            0,
            "",
            "",
            this -> uc.name,
            ""

        };

    resCode = sendMessage2(fcu2);
    
    if(resCode < 0) {

        shutdown(socketObj.GetSocketDescriptor(), 2);
        return -1;

    }

    resCode = readMessageResponse2(response);

    if(resCode < 0) {

        shutdown(socketObj.GetSocketDescriptor(), 2);
        return -2;
        
    }

	// Disconnect from server
    //socketObj.Close();

    myLogger -> info ("disconnected from server " + uc.serverIp + ":" + uc.serverPort);
    myLogger -> flush();

    return resCode;

}

/*

RETURN:

 0 ---> no error
-1 ---> error parsing message json
-2 ---> error sending message LENGTH
-3 ---> error sending message DATA

*/

int Client::sendFileStream(string filePath){

    json jMsg;
    int sendCode = 0;
    off_t offset = 0;
    
    try{

        FILE* file = fopen(filePath.c_str(), "r");

        if(file == NULL){

            return -1;

        }

        // take file descriptor from FILE pointer 
        int fd = fileno(file); 

        if(isClosed()){

            return -2;

        }

        // obtain file size
        fseek(file, 0L, SEEK_END);
        int fileSize = ftell(file);

        int sendFileReturnCode = this -> socketObj.SendFile(this -> socketObj.GetSocketDescriptor(), fd, &offset, fileSize);

        if(sendFileReturnCode < 0){

            return -3;

        } 

    } catch(...){

        return -4;

    }

    return 0;

}

int Client::send(int operation, string folderPath, string fileName, string content){

    int resCode = 0;
    string response;
 

    msg::message2 msg;
    
    // UPDATE OR CREATE OPERATION
    if(operation == 1 || operation == 3){


        string filePath = folderPath + separator() + fileName;

        FILE* file = fopen(filePath.c_str(), "r");

        if(file == NULL){

            myLogger -> error("the file " + filePath + " wasn't found! ");
            return -5;

        }
        myLogger -> info("");
        myLogger -> info("[OPERATION_" + to_string(operation) + "]: path " + filePath);
        myLogger -> info("");
        myLogger -> flush();

        // obtain file size
        fseek(file, 0L, SEEK_END);
        int fileSize = ftell(file); 

        int div = fileSize / SOCKET_SENDFILE_BLOCKSIZE;
        int rest = fileSize % SOCKET_SENDFILE_BLOCKSIZE;
        int numberOfPackets = div;
        if(rest > 0)
            numberOfPackets ++;

        msg = {

            "",
            operation,
            0, // timestamp
            "",// hash
            numberOfPackets,
            folderPath,
            fileName,
            this -> uc.name,
            content

        };

        resCode = sendMessage2(msg);
        
        myLogger -> info("[OPERATION_" + to_string(operation) + "]: SND MSG returned code: " + to_string(resCode));
        myLogger -> flush();

        if(resCode < 0)
            return -1;

        resCode = readMessageResponse2(response);

        myLogger -> info("[OPERATION_" + to_string(operation) + "]: RCV RESP returned code: " + to_string(resCode));
        myLogger -> flush();
        
        if(resCode < 0)
            return -2;
        
        resCode = sendFileStream(filePath);

        myLogger -> info("[OPERATION_" + to_string(operation) + "]: SND FILE STREAM returned code: " + to_string(resCode));
        myLogger -> flush();

        if(resCode < 0)
            return -3;

        resCode = readMessageResponse2(response);

        myLogger -> info("[OPERATION_" + to_string(operation) + "]: RCV STREAM RESP returned code: " + to_string(resCode));
        myLogger -> flush();
        
        if(resCode < 0)
            return -4;

    } else {

        myLogger -> info("");
        myLogger -> info("[OPERATION_" + to_string(operation) + "]: path " + folderPath);
        myLogger -> info("");
        myLogger -> flush();

        msg = {

            "",
            operation,
            0, // timestamp
            "",// hash
            0,
            folderPath,
            fileName,
            this -> uc.name,
            content

        };

        resCode = sendMessage2(msg);

        myLogger -> info("[OPERATION_" + to_string(operation) + "]: SEND MSG returned code: " + to_string(resCode));
        myLogger -> flush();

        if(resCode < 0)
            return -1;

        resCode = readMessageResponse2(response);

        myLogger -> info("[OPERATION_" + to_string(operation) + "]: RCV RESP returned code: " + to_string(resCode));
        myLogger -> flush();

        if(resCode < 0)
            return -2;

        // INITIAL CONFIGURATION
        if (operation == 5){
            
            string initialConf;

            // SEND OK RESPONSE
            msg.typeCode = 0;
            msg.type = "ok";

            int numberPackets = msg.packetNumber;

            resCode = sendMessage2(msg);

            myLogger -> info("[OPERATION_" + to_string(operation) + "]: SEND CONF RESP returned code: " + to_string(resCode));
            myLogger -> flush();

            if(resCode < 0)
                return -3;

            resCode = readInitialConfStream(numberPackets, initialConf);

            myLogger -> info("[OPERATION_" + to_string(operation) + "]: RCV CONF STREAM returned code: " + to_string(resCode));
            myLogger -> flush();
            
            if(resCode < 0)
                return -4;

            resCode = sendMessage2(msg);

            myLogger -> info("[OPERATION_" + to_string(operation) + "]: SEND CONF STREAM RESP returned code: " + to_string(resCode));
            myLogger -> flush();

            if(resCode < 0)
                return -5;

            resCode = readMessageResponse2(response);

            myLogger -> info("[OPERATION_" + to_string(operation) + "]: RCV END STREAM returned code: " + to_string(resCode));
            myLogger -> flush();

            if(resCode < 0)
                return -6;

        }

    }

    return 0;

}


int Client::readInitialConfStream(int packetsNumber, string conf){

    // per ogni stream ricevuto dal client scrivo su un file temporaneo
    // se lo stream è andato a buon fine e ho ricevuto tutto faccio una copia dal file temporaneo a quello ufficiale
    // ed eliminio il file temporaneo

    int i = 0;
    int sockFd = this -> socketObj.GetSocketDescriptor();
    char buffer [BUFSIZ];

    do {

        i++;

        // reset char array
        memset(buffer, 0, BUFSIZ);

        int read_return = read(sockFd, buffer, BUFSIZ);

        if (read_return == -1) {
            
            return -1;

        }

        try {

            string tempBuf = buffer;
            conf += tempBuf;

        } catch (...) {

            return -2;
        }   

    } while(i < packetsNumber);

    return 0;

}

/*

RETURN:

 0 ---> no error
-1 ---> error parsing message json
-2 ---> error sending message LENGTH
-3 ---> error sending message DATA
-4 ---> socket CLOSED

*/

int Client::sendMessage2(msg::message2 msg){

    json jMsg;
    int sendCode = 0;
    
    try {

        jMsg = json{{"packetNumber", msg.packetNumber},{"userName", msg.userName},{"type", msg.type}, {"typeCode", msg.typeCode}, {"fileName", msg.fileName}, {"folderPath", msg.folderPath}, {"body", msg.body}};
    
    } catch (...) {

        return -1;

    }

    // SENDING MESSAGE HEADER

    try{

        string jMsgString = jMsg.dump();

        int sendCode = this -> socketObj.Send((const uint8 *) jMsgString.c_str(), jMsgString.length());

        if(sendCode < 0){

            socketObj.TranslateSocketError();
            string sockError = socketObj.DescribeError();

            return -2;

        } else if (sendCode == 0) {

            socketObj.TranslateSocketError();
            string sockError = socketObj.DescribeError();

            return -3;

        }

    } catch(...){

        return -4;

    }

    return 0;

}

/*

RETURN:

 0 ---> no error
-1 ---> error receiving message LENGTH
-2 ---> error receiving message DATA
-3 ---> unexpected error
-4 ---> socket CLOSED
-5 ---> TIMEOUT

*/

int Client::readMessageResponse2(string & response){

    std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
    int rcvCode = 0;
    fd_set set;
    int iResult = 0;

    try {
        
        int rcvCode = this -> socketObj.Receive(SOCKET_SENDFILE_BLOCKSIZE);

        if(rcvCode < 0){

            socketObj.TranslateSocketError();
            string sockError = socketObj.DescribeError();
            
            return -1;

        } else if(rcvCode == 0){

            socketObj.TranslateSocketError();
            string sockError = socketObj.DescribeError();
            
            return -2;

        }
        
        string respString = (char *) socketObj.GetData();

        response.clear();
        response = respString;
        
    } catch (...) {

        return -4;

    }

    return 0;

}

/*

RETURN:

 0  ---> no error
-10 ---> error parsing message received into JSON

*/

int Client::fromStringToMessage(string msg, msg::message& message){

    try {

        auto jsonMSG = json::parse(msg);

        jsonMSG.at("type").get_to(message.type);
        jsonMSG.at("typeCode").get_to(message.typeCode);
        jsonMSG.at("folderPath").get_to(message.folderPath);
        jsonMSG.at("fileName").get_to(message.fileName);
        jsonMSG.at("fileContent").get_to(message.fileContent);

    } catch (...) {

        myLogger -> error ("An error appened parsing the message received: " + msg);
        myLogger -> flush();
        return -10;

    }
 
    return 0;

}

