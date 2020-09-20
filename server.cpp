#include "server.hpp"

int Server::initLogger(){

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

    Server::~Server(){
        if(sock!=-1)
            close(sock);
    }

    int Server::startListening(){

        running = true;
        int opt = 1;
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if ( sock == 0 )  
        { 
            perror("socket failed"); 
            exit(EXIT_FAILURE); 
        } 

        if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR , &opt, sizeof(opt))<0){
            log -> error("sockopt error");
            log -> error(strerror(errno));
            return -1;
        }

        sockaddr_in saddr;
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(atoi(sc.port.c_str()));
        saddr.sin_addr.s_addr = INADDR_ANY;

        if(bind(sock, (struct sockaddr *)&saddr, sizeof(saddr))<0){
            log -> error("bind error");
            log -> error(strerror(errno));
            return -1;
        }

        if(::listen(sock, 10)<0){
            log -> error("listen error");
            log -> error(strerror(errno));
            return -1;
        }

        // start idle clients loop -> for now it is not used
        // checkIdleClients();

        // we and the server with SIGINT
        while(running) {
            sockaddr_in caddr;
            socklen_t addrlen = sizeof(caddr);

            log -> info("waiting for client connection");
            log -> flush();
            int csock = accept(sock, (struct sockaddr*) &caddr, &addrlen);
            if(csock<0){
                log -> error("accept error");
                log -> error(strerror(errno));
            } else {
                char buff[8];
                std::lock_guard<mutex> lg(m);

                //get socket ip address
                struct sockaddr* ccaddr = (struct sockaddr*)&caddr;
                string clientIp = ccaddr -> sa_data;

                pClient client = pClient(new ClientConn(this -> logFile, csock, this -> sc));

                // this keeps the client alive until it's destroyed
                clients[csock] = client;

                // handle connection should return immediately
                client->handleConnection();

            }

        }
        std::cout << "exited" << std::endl;
        return 0;

    }

int Server::readUsersPath(){

    return 0;

}

int Server::readConfiguration (string file) {

    // Read SERVER configuration file in the local folder

    ifstream serverConfFile(file);
    json jServerConf;
     
    if(!serverConfFile)
    {
        string error = strerror(errno);
        cerr << "Server Configuration File: " << file << " could not be opened!";
        cerr << "Error code opening Server Configuration File: " << error;
        return -1;
    }
        

    if(!(serverConfFile >> jServerConf))
    {
        cerr << "The Server Configuration File couldn't be parsed";
        return -2; 
    }

    // save the server configuration inside a local struct
    this -> sc = {
        jServerConf["ip"].get<string>(),
        jServerConf["port"].get<string>(),
        jServerConf["backupFolder"].get<string>()
    };

    return 0;

}