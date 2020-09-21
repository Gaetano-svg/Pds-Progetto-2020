#include "server.hpp"

Server::~Server(){
        
    if(sock!=-1)
        close(sock);

}   

/* 

RETURN:

 0  ---> no error
-1  ---> creation socket error
-2  ---> set socket error
-3  ---> bind socket error
-4  ---> listen socket error

*/

int Server::startListening(){

    running = true;
    int opt = 1;

    // initialize socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if ( sock == 0 ) { 
        
        log -> error("socket creation failed"); 
        return -1; 

    } 

    // set socket
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR , &opt, sizeof(opt))<0){
        
        string error = strerror(errno);
        log -> error("setsockopt error: " + error);
        
        return -2;

    }

    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(sc.port.c_str()));
    saddr.sin_addr.s_addr = INADDR_ANY;

    // bind socket
    if(bind(sock, (struct sockaddr *)&saddr, sizeof(saddr))<0){

        string error = strerror(errno);
        log -> error("bind error: " + error);
        
        return -3;

    }

    // listen to N different clients
    if(::listen(sock, 10)<0){

        string error = strerror(errno);
        log -> error("listen error: " + error);
        
        return -4;

    }

    while(running) {

        try {

            sockaddr_in caddr;
            socklen_t addrlen = sizeof(caddr);

            log -> info("waiting for client connection");
            log -> flush();

            // wait until client connection
            int csock = accept(sock, (struct sockaddr*) &caddr, &addrlen);

            if(csock<0){

                string error = strerror(errno);
                log -> error("accept error: " + error);

            } else {

                char buff[8];
                std::lock_guard<mutex> lg(m);

                //get socket ip address
                struct sockaddr* ccaddr = (struct sockaddr*)&caddr;
                string clientIp = ccaddr -> sa_data;

                // for each client allocate a ClientConnection object
                pClient client = pClient(new ClientConn(this -> logFile, csock, this -> sc));

                // this keeps the client alive until it's destroyed
                clients[csock] = client;

                // handle connection should return immediately
                client->handleConnection();

            }

        } catch (const std::exception &exc) {

            string error = exc.what();
            log -> error ("an error occured: " + error);

        } catch (...) {

            log -> error ("an unexpected error occured ");

        }

        // interrupt 1 second before restart listening to socket connection
        try {
            
            log -> info("sleep 1 second before restart listening");
            sleep(1);

        } catch (...) {

            log -> error("an error occured during sleep");

        }

    }

    std::cout << "[SERVER]: exited from run" << std::endl;
    return 0;

}

/* 

RETURN:

 0   ---> no error
-1   ---> error opening/creating the file
-2   ---> error converting the JSON
-3   ---> error saving the JSON inside the struct

*/

int Server::readConfiguration (string file) {

    // Read SERVER configuration file in the local folder
    ifstream serverConfFile(file);
    json jServerConf;
    
    // check if file was opened/created correctly
    if(!serverConfFile)
    {
        string error = strerror(errno);
        cerr << "Server Configuration File: " << file << " could not be opened!";
        cerr << "Error code opening Server Configuration File: " << error;
        return -1;
    }

    // check if the JSON conversion is correct
    if(!(serverConfFile >> jServerConf))
    {
        cerr << "The Server Configuration File couldn't be parsed";
        return -2; 
    }

    // save the server configuration inside a local SERVER struct
    try {

        this -> sc = {
            jServerConf["ip"].get<string>(),
            jServerConf["port"].get<string>(),
            jServerConf["backupFolder"].get<string>()
        };

    } catch (...) {
        
        cerr << "Error during the saving of the configuration locally to SERVER";
        return -3;

    }
    
    return 0;

}

/* 

RETURN:

 0   ---> no error
-1   ---> error opening/creating the log file

*/

int Server::initLogger(){

    // opening/creating the log file
    try 
    {
        this -> log = spdlog::basic_logger_mt(this -> sc.ip, this -> logFile);
        this -> log -> info("Logger initialized correctly");
        this -> log -> flush();
    } 
    catch (const spdlog::spdlog_ex &ex) 
    {
        cerr << ex.what() << endl;
        return -1;
    }
        
    return 0;

}

void Server::unregisterClient(int csock){

    std::lock_guard <mutex> lg(m);
    clients.erase(csock);

}
