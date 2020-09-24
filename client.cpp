
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
            jUserConf["folderPath"].get<string>()
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

RETURN:

 0 ---> no error
-1 ---> error creating socket
-2 ---> server address not supported

*/

int Client::serverConnection () {

    // Create socket

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
        string error = strerror(errno);
		myLogger -> error("Can't create socket, Error: " + error);
        close(sock);
		return -1;
	}

    myLogger -> info("Socket " + to_string(sock) + " was created");

	// Fill in a hint structure
    sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(atoi(uc.serverPort.c_str()));
	
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if ((inet_pton(AF_INET, uc.serverIp.c_str(), &hint.sin_addr)) <= 0)
    {
        myLogger -> error("Invalid address: address " + uc.serverIp + " not supported");
        close(sock);
        return -2;
    }
    
    myLogger -> info ("try to connect to server - IP: " + uc.serverIp + " PORT: " + uc.serverPort);
    myLogger -> flush();

	// Connect to server
	while (connect(sock, (sockaddr*)&hint, sizeof(hint)) < 0)
	{
        string error = strerror(errno);
        myLogger -> error("Can't connect to server, Error: " + error);
        myLogger -> flush();
        sleep(5);
	}

    myLogger -> info ("connected to server " + uc.serverIp + ":" + uc.serverPort);
    myLogger -> flush();

    return 0;
}

int Client::serverDisconnection () {

    // Create socket

    myLogger -> info ("try to disconnect from server - IP: " + uc.serverIp + " PORT: " + uc.serverPort);
    myLogger -> flush();

	// Disconnect from server
    close(sock);

    myLogger -> info ("disconnected from server " + uc.serverIp + ":" + uc.serverPort);
    myLogger -> flush();

    return 0;

}
/*

RETURN:

 0 ---> no error
-1 ---> error parsing the user configuration json object
-2 ---> error sending user configuration LENGTH
-3 ---> error sending user configuration DATA
-4 ---> error receving user configuration RESPONSE from server

*/

int Client::sendConfiguration () {

    msg::connection connMess
    {
        uc.name,
        uc.folderPath
    };

    json jConnMess;

    try {

        jConnMess = json{{"userName", connMess.userName}, {"folderPath", connMess.folderPath}};

    } catch (...) {

        myLogger -> error ("error creating the user configuration json");
        return -1;

    }

    string connString = jConnMess.dump();

    uint64_t sizeNumber = connString.length();
    uint64_t dataLength = (connString.size()); 

    // Send the data length
    if(send(sock,&dataLength ,sizeof(uint64_t) ,MSG_CONFIRM) < 0){

        myLogger -> error ("error sending user configuration LENGTH");
        return -2;

    } 
    
    myLogger -> info("user configuration length sent correctly: " + to_string(dataLength));
    myLogger -> flush();

    // Send the data content 
    if(send(sock,connString.c_str(),connString.size(),MSG_CONFIRM) < 0){

        myLogger -> error ("error sending user configuration DATA");
        return -3;

    } 
    
    myLogger -> info("user configuration sent correctly: " + connString);
    myLogger -> flush();

    // wait until servers response
    string responseString;
    
    if(readConfigurationResponse(responseString) < 0){

        myLogger -> error ("error receiving response from server");
        return -4;

    } 

    myLogger -> info("message response received for connection-message: " + responseString);
    myLogger -> flush();

    return 0;

}

/*

RETURN:

 0 ---> no error
-1 ---> error parsing message json
-2 ---> error sending message LENGTH
-3 ---> error sending message DATA

*/

int Client::sendMessage(msg::message msg){

    json jMsg;
    
    try {

        jMsg = json{{"type", msg.type}, {"typeCode", msg.typeCode}, {"fileName", msg.fileName}, {"folderPath", msg.folderPath}, {"fileContent", msg.fileContent}};
    
    } catch (...) {

        myLogger -> error ("an error parsing message json");
        return -1;

    }

    string jMsgString = jMsg.dump();
    uint64_t sizeNumber = jMsgString.length();

    myLogger -> info("Sending create SIZE msg for file to server: " + to_string(jMsgString.length()) + " bytes");
    myLogger -> flush(); 
    
    if(send(sock, &sizeNumber, sizeof(sizeNumber), 0) < 0){

        myLogger -> error ("an error occured sending message LENGTH");
        return -2;

    }

    myLogger -> info("Sending create msg for file to server: " + jMsgString + " length: " + to_string(jMsgString.length()) + " bytes");
    myLogger -> flush(); 

    if (send(sock, jMsgString.c_str(), sizeNumber, 0) < 0){

        myLogger -> error ("an error occured sending message DATA");
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

*/

int Client::readMessageResponse(string & response){

    uint64_t rcvDataLength;
    std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
    
    try {

        myLogger -> info("wait For response");
        myLogger -> flush();

        // Receive the message length
        if(recv(sock,&rcvDataLength,sizeof(uint64_t),0) < 0){

            myLogger -> error("an error occured receiving response LENGTH");
            return -1;

        } 

        rcvBuf.resize(rcvDataLength,0x00); // with the necessary size

        myLogger -> info ("response size received: " + to_string(rcvDataLength));
        myLogger -> flush();

        // Receive the string data
        if(recv(sock,&(rcvBuf[0]),rcvDataLength,0) < 0){

            myLogger -> error("an error occured receiving response DATA");
            return -2;

        } 
        
        response.assign(rcvBuf.begin(), rcvBuf.end());
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

/*

RETURN:

 0 ---> no error
-1 ---> error receiving data length
-2 ---> error receiving data
-3 ---> unexpected error

*/

int Client::readConfigurationResponse(string & response) {

    uint64_t rcvDataLength;
    string bufString;
    std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer

    try {

        // Receive the message length
        if(recv(sock,&rcvDataLength,sizeof(uint64_t),0) < 0){

            myLogger -> error("an error occured waiting for CONF RESPONSE LENGTH");
            return -1;

        } 
        
        rcvBuf.resize(rcvDataLength,0x00); // with the necessary size

        // Receive the string data
        if(recv(sock,&(rcvBuf[0]),rcvDataLength,0) < 0){

            myLogger -> error("an error occured waiting for CONF RESPONSE DATA");
            return -2;

        }

        response.assign(rcvBuf.begin(), rcvBuf.end());

    } catch (...) {

        myLogger -> error ("an unexpected error happened reading configuration response");
        return -3;

    }
    
    return 0;

}
