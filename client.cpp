
#include "client.hpp"

#define PACKET_SIZE SOCKET_SENDFILE_BLOCKSIZE


//////////////////////////////////
//        PUBLIC METHODS        //
//////////////////////////////////


/* 

Read the user configuration JSON file.

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

Initialize the Log File for the client.

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

Open a connection channel with the Server.

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

    while(readMessageResponse(response) < 0){

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
Send a message to the server.
Connection must be opened in order to send message correctly.

PARAMETERS: 
    operation: (int) the operation code type number
    folderPath: (string) the folder Path of the file
    fileName: (string) the name of the file to update on the server
    content: (string) optional content to send with the request (Rename)

RETURN:
    0 -> no error
    -1 -> error sending msg header
    -2 -> error receiving msg header response
    -3 -> error sending msg STREAM
    -4 -> error receiving msg STREAM response
    -5 -> error sending CONF response
    -6 -> error receiving CONF STREAM
    -7 -> error sending CONF STREAM response
    -8 -> error receving END STREAM 
    -10 -> filePath wasn't found on the client side
*/
int Client::send(int operation, string folderPath, string fileName, string content){

    int resCode = 0;
    string response;
 
    msg::message2 msg;

    string filePath = folderPath + separator() + fileName;
    FILE* file = fopen(filePath.c_str(), "r");
    
    // check if the File exists in the local folder
    if(file == NULL){

        myLogger -> error("the file " + filePath + " wasn't found! ");
        return -10;

    }

    myLogger -> info("");
    myLogger -> info("[OPERATION_" + to_string(operation) + "]: path " + filePath);
    myLogger -> info("");
    myLogger -> flush();

    // CREATE the Message Object
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

    // Set the # of packets because of FILE size
    if(operation == 1 || operation == 3){

        // obtain file size
        fseek(file, 0L, SEEK_END);
        int fileSize = ftell(file); 

        int div = fileSize / PACKET_SIZE;
        int rest = fileSize % PACKET_SIZE;
        int numberOfPackets = div;
        if(rest > 0)
            numberOfPackets ++;
        
        // set the # of packets 
        msg.packetNumber = numberOfPackets;

    }

    resCode = sendMessage(msg);
        
    myLogger -> info("[OPERATION_" + to_string(operation) + "]: SND MSG returned code: " + to_string(resCode));
    myLogger -> flush();

    if(resCode < 0)
        return -1;

    resCode = readMessageResponse(response);

    myLogger -> info("[OPERATION_" + to_string(operation) + "]: RCV RESP returned code: " + to_string(resCode));
    myLogger -> flush();
        
    if(resCode < 0)
        return -2;

    // UPDATE OR CREATE OPERATION
    if(operation == 1 || operation == 3){
        
        resCode = sendFileStream(filePath);

        myLogger -> info("[OPERATION_" + to_string(operation) + "]: SND FILE STREAM returned code: " + to_string(resCode));
        myLogger -> flush();

        if(resCode < 0)
            return -3;

        resCode = readMessageResponse(response);

        myLogger -> info("[OPERATION_" + to_string(operation) + "]: RCV STREAM RESP returned code: " + to_string(resCode));
        myLogger -> flush();
        
        if(resCode < 0)
            return -4;

    } else if (operation == 5){
        
        // INITIAL CONFIGURATION

            string initialConf;

            // SEND OK RESPONSE
            msg.typeCode = 0;
            msg.type = "ok";

            int numberPackets = msg.packetNumber;

            resCode = sendMessage(msg);

            myLogger -> info("[OPERATION_" + to_string(operation) + "]: SEND CONF RESP returned code: " + to_string(resCode));
            myLogger -> flush();

            if(resCode < 0)
                return -5;

            resCode = readInitialConfStream(numberPackets, initialConf);

            myLogger -> info("[OPERATION_" + to_string(operation) + "]: RCV CONF STREAM returned code: " + to_string(resCode));
            myLogger -> flush();
            
            if(resCode < 0)
                return -6;

            resCode = sendMessage(msg);

            myLogger -> info("[OPERATION_" + to_string(operation) + "]: SEND CONF STREAM RESP returned code: " + to_string(resCode));
            myLogger -> flush();

            if(resCode < 0)
                return -7;

            resCode = readMessageResponse(response);

            myLogger -> info("[OPERATION_" + to_string(operation) + "]: RCV END STREAM returned code: " + to_string(resCode));
            myLogger -> flush();

            if(resCode < 0)
                return -8;

    }

    return 0;

}

/*

Close connection channel with the server.

RETURN:

 0 -> no error
-1 -> error sending disconnection request
-2 -> error receiving disconnection request RESPONSE
-3 -> unexpected error

*/
int Client::serverDisconnection () {

    try {

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

        resCode = sendMessage(fcu2);
        
        if(resCode < 0) {

            shutdown(socketObj.GetSocketDescriptor(), 2);
            return -1;

        }

        resCode = readMessageResponse(response);

        if(resCode < 0) {

            shutdown(socketObj.GetSocketDescriptor(), 2);
            return -2;
            
        }

        // Disconnect from server
        socketObj.Close();

        myLogger -> info ("disconnected from server " + uc.serverIp + ":" + uc.serverPort);
        myLogger -> flush();

        return resCode;

    } catch (...) {

        myLogger -> error("Unexpected error happened during server-disconnection");
        return -3;

    }

}

/*

Check if the connection with the server IS CLOSED.

RETURN:

    true: connection IS CLOSED
    false: connection IS OPENED

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


///////////////////////////////////
//        PRIVATE METHODS        //
///////////////////////////////////


/*

RETURN:

 0 ---> no error
-1 ---> error parsing message json
-2 ---> error sending message 
-3 ---> unexpected error

*/
int Client::sendMessage(msg::message2 msg){

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

        if(sendCode <= 0){

            return -2;

        } 

    } catch(...){

        return -3;

    }

    return 0;

}

/*

RETURN:

 0 ---> no error
-1 ---> error opening filePath
-2 ---> socket was closed yet
-3 ---> error sending file stream
-4 ---> unexpected error

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

        if(sendFileReturnCode <= 0){

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
-1 ---> error receiving message RESPONSE
-2 ---> unexpected error

*/
int Client::readMessageResponse(string & response){

    uint8_t rcvBuf [PACKET_SIZE];    // Allocate a receive buffer
    int rcvCode = 0;
    fd_set set;
    int iResult = 0;

    try {
        
        memset(rcvBuf, 0, PACKET_SIZE);
        int rcvCode = this -> socketObj.Receive(PACKET_SIZE, rcvBuf);

        if(rcvCode <= 0){

            return -1;

        }
        
        response.clear();
        response = (char*) rcvBuf;

        cout << response << endl;
        
    } catch (...) {

        return -2;

    }

    return 0;

}

/*

RETURN:

 0 ---> no error
-1 ---> error receiving configuration Stream PACKET
-2 ---> unexpected error

*/
int Client::readInitialConfStream(int packetsNumber, string conf){

    // per ogni stream ricevuto dal client scrivo su un file temporaneo
    // se lo stream è andato a buon fine e ho ricevuto tutto faccio una copia dal file temporaneo a quello ufficiale
    // ed eliminio il file temporaneo

    int i = 0;
    int sockFd = this -> socketObj.GetSocketDescriptor();
    uint8 buffer [BUFSIZ];

    try {

        do {

            i++;

            // reset char array
            memset(buffer, 0, BUFSIZ);

            int rcvCode = this -> socketObj.Receive(PACKET_SIZE, buffer);

            if(rcvCode <= 0){

                return -1;

            }

            string tempBuf = (char*)buffer;
            conf += tempBuf;            

        } while(i < packetsNumber);

    } catch (...) {

        return -2;

    }

    return 0;

}

/*

RETURN:

 0  ---> no error
-1 ---> error parsing message received into JSON

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

        return -1;

    }
 
    return 0;

}

