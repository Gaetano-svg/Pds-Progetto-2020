
#include "client.hpp"

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
        return -1; 
    }

    // save the user configuration inside a local struct
    uc = conf::user 
    {
        jUserConf["serverIp"].get<string>(),
        jUserConf["serverPort"].get<string>(),
        jUserConf["name"].get<string>(),
        jUserConf["folderPath"].get<string>()
    };

    return 0;

}

int Client::initLogger () {

    // 2. Logger initialization

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
    if ((inet_pton(AF_INET, "127.0.0.1", &hint.sin_addr)) <= 0)
    {
        myLogger -> error("Invalid address: address " + uc.serverIp + " not supported");
        close(sock);
        return -1;
    }

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

int Client::sendConfiguration () {

    msg::connection connMess
    {
        uc.name,
        uc.folderPath
    };

    json jConnMess = json{{"userName", connMess.userName}, {"folderPath", connMess.folderPath}};

    string connString = jConnMess.dump();

    uint64_t sizeNumber = connString.length();
    uint64_t dataLength = (connString.size()); 
    
    send(sock,&dataLength ,sizeof(uint64_t) ,MSG_CONFIRM); // Send the data length
    
    myLogger -> info("user configuration length sent: " + to_string(dataLength));
    myLogger -> flush();

    send(sock,connString.c_str(),connString.size(),MSG_CONFIRM); // Send the string 
    
    myLogger -> info("user configuration sent: " + connString);
    myLogger -> flush();

    string responseString = readConfigurationResponse(sock);

    myLogger -> info("message response received for connection-message: " + responseString);
    myLogger -> flush();

    return 0;

}

/// Reads n bytes from fd.
bool Client::readNBytes(int fd, void *buf, std::size_t n) {
        
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

int Client::sendMessage(msg::message msg){

    json jMsg = json{{"type", msg.type}, {"typeCode", msg.typeCode}, {"fileName", msg.fileName}, {"folderPath", msg.folderPath}, {"fileContent", msg.fileContent}};
    string jMsgString = jMsg.dump();
    uint64_t sizeNumber = jMsgString.length();

    myLogger -> info("Sending create SIZE msg for file to server: " + to_string(jMsgString.length()) + " bytes");
    myLogger -> flush(); 
    
    send(sock, &sizeNumber, sizeof(sizeNumber), 0);

    myLogger -> info("Sending create msg for file to server: " + jMsgString + " length: " + to_string(jMsgString.length()) + " bytes");
    myLogger -> flush(); 

    send(sock, jMsgString.c_str(), sizeNumber, 0);

    return 0;

}

string Client::readMessageResponse(int fd){

        uint64_t rcvDataLength;
        string bufString;
        std::vector<uint8_t> rcvBuf;    // Allocate a receive buffer
        std::string receivedString;                        // assign buffered data to a 

    
    myLogger -> info("wait For response");
    myLogger -> flush();

        recv(fd,&rcvDataLength,sizeof(uint64_t),0); // Receive the message length
        rcvBuf.resize(rcvDataLength,0x00); // with the necessary size

        myLogger -> info ("message size received: " + rcvDataLength);

        recv(fd,&(rcvBuf[0]),rcvDataLength,0); // Receive the string data
        receivedString.assign(rcvBuf.begin(), rcvBuf.end());

        myLogger -> info ("message received: " + receivedString);

        bufString = receivedString.c_str();
        return bufString;

}

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

/// Reads message from fd
string Client::readConfigurationResponse(int fd) {

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
