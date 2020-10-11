
#include "client.hpp"

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
-2 ---> server address not supported
-3 ---> socket won't be accepted

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

    socketObj.SetSendTimeout(uc.secondTimeout);
    socketObj.SetReceiveTimeout(uc.secondTimeout);

    // convert port into INT VALUE
    int port = stoi(this -> uc.serverPort);

    while(! this -> socketObj.Open(this -> uc.serverIp.c_str() , port)){

        myLogger -> info ("Can't connect to server " + uc.serverIp + " port: " + uc.serverPort);
        myLogger -> info ("Try again to connect in 5 seconds");
        myLogger -> flush();

        sleep(5);

    }

    while(readMessageResponse(response) < 0){

        myLogger -> info ("Can't connect to server " + uc.serverIp + " port: " + uc.serverPort);
        myLogger -> info ("Try again to connect in 5 seconds");
        myLogger -> flush();

        sleep(5);

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
    myLogger -> info ("try to disconnect from server - IP: " + uc.serverIp + " PORT: " + uc.serverPort);
    myLogger -> flush();

    string response;
    msg::message fcu2 {
        "disconnect",
        6,
        "test",
        "test",
        "test disconnect",
        "gaetano"

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

}

/*

RETURN:

 0 ---> no error
-1 ---> error parsing message json
-2 ---> error sending message LENGTH
-3 ---> error sending message DATA
-4 ---> socket CLOSED

*/

int Client::sendMessage(msg::message msg){

    json jMsg;
    int sendCode = 0;
    
    try {

        jMsg = json{{"userName", msg.userName},{"type", msg.type}, {"typeCode", msg.typeCode}, {"fileName", msg.fileName}, {"folderPath", msg.folderPath}, {"fileContent", msg.fileContent}};
    
    } catch (...) {

        myLogger -> error ("an error parsing message json");
        return -1;

    }

    string jMsgString = jMsg.dump();
    uint64_t sizeNumber = jMsgString.length();
    string size = to_string(sizeNumber);

    myLogger -> info("Sending SIZE msg for file to server: " + size + " bytes");
    myLogger -> flush(); 

    if(this -> socketObj.Send((const uint8 *) size.c_str(), sizeof(uint64_t)) <= 0){
        return -2;
    }

    myLogger -> info("Sending DATA msg for file to server: " + jMsgString + " length: " + to_string(jMsgString.length()) + " bytes");
    myLogger -> flush(); 

    if(this -> socketObj.Send((const uint8 *) jMsgString.c_str(), sizeNumber) <= 0){
        return -3;
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

int Client::readMessageResponse(string & response){

    uint64_t rcvDataLength;
    std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
    int rcvCode = 0;
    fd_set set;
    struct timeval timeout;
    int iResult = 0;

    try {

        myLogger -> info("wait For response");
        myLogger -> flush();

        // Receive the message length

        if(this -> socketObj.Receive(sizeof(uint64_t)) <= 0){

            myLogger -> error("an error occured receiving response LENGTH");
            return -1;

        }
        
        string length = (char *) socketObj.GetData();
        rcvDataLength = atoi(length.c_str());

        //memcpy(&rcvDataLength, socketObj.GetData(), sizeof(uint64_t));

        myLogger -> info ("response size received: " + to_string(rcvDataLength));
        myLogger -> flush();

        // Receive the string data
        if(this -> socketObj.Receive(rcvDataLength) <= 0){

            myLogger -> error("an error occured receiving response DATA");
            return -2;

        }
        
        string respString = (char *) socketObj.GetData();
        
        response = respString;

        //response.assign(rcvBuf.begin(), rcvBuf.end());
        myLogger -> info ("response received: " + response);
        myLogger -> flush();
        
    } catch (...) {

        myLogger -> error ("unexpected error happened reading message response");
        return -3;

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

