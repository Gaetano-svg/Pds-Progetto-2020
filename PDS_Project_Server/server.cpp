#include "server.hpp"


//////////////////////////////////
//        PUBLIC METHODS        //
//////////////////////////////////


/*

Read Server JSON configuration file.

RETURN:

 0   ---> no error
-1   ---> error opening/creating the file
-2   ---> error converting the JSON
-3   ---> error saving the JSON inside the struct

*/
int Server::readConfiguration(std::string file) {

    // Read SERVER configuration file in the local folder
    std::ifstream serverConfFile(file);
    json jServerConf;

    // check if file was opened/created correctly
    if (!serverConfFile)
    {
        std::string error = strerror(errno);
        std::cerr << "Server Configuration File: " << file << " could not be opened!";
        std::cerr << "Error code opening Server Configuration File: " << error;
        return -1;
    }

    // check if the JSON conversion is correct
    if (!(serverConfFile >> jServerConf))
    {
        std::cerr << "The Server Configuration File couldn't be parsed";
        return -2;
    }

    // save the server configuration inside a local SERVER struct
    try {

        this->sc = {
            jServerConf["ip"].get<std::string>(),
            jServerConf["port"].get<std::string>(),
            jServerConf["backupFolder"].get<std::string>(),
            jServerConf["userInactivityMS"].get<int>(),
            jServerConf["numberActiveClients"].get<int>(),
            jServerConf["secondTimeout"].get<int>()
        };

    }
    catch (...) {

        std::cerr << "Error during the saving of the configuration locally to SERVER";
        return -3;

    }

    return 0;

}

/*

Initialize logger for the server.

RETURN:

 0   ---> no error
-1   ---> error opening/creating the log file

*/
int Server::initLogger() {

    // opening/creating the log file
    try
    {
        // controllare se il logger esiste oppure creare e chiudere il log per ogni connessione del client
        this->log = spdlog::basic_logger_mt(this->sc.ip, this->logFile);
        this->log->info("Logger initialized correctly");
        this->log->flush();
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::cerr << ex.what() << std::endl;
        return -1;
    }

    return 0;

}

/*

Main function to let the server wait for client-connections.

RETURN:

 0  ---> no error
-1  ---> initialization error

*/
int Server::startListening() {

    CPassiveSocket socket;
    CActiveSocket* pClient = NULL;

    int serverPort;
    const char* serverIp;

    try {

        running.store(true);

        // thread to control users/sockets inactivity
        std::thread checkUserInactivityThread([this]() {

            this->checkUserInactivity();

            });

        checkUserInactivityThread.detach();

        this->activeConnections.store(0);

        pClient = NULL;

        serverPort = atoi(sc.port.c_str());
        serverIp = sc.ip.c_str();

    }
    catch (...) {

        return -1;

    }

    //--------------------------------------------------------------------------
    // Initialize our socket object 
    //--------------------------------------------------------------------------
    socket.Initialize();
    socket.SetBlocking();
    socket.Listen(serverIp, serverPort);

    while (running.load())
    {
        try {

            log->info("There are " + to_string(this->activeConnections) + " sockets alive");
            log->flush();

            if (this->activeConnections < this->sc.numberActiveClients) {

                auto client = std::shared_ptr<ClientConn>(new ClientConn(*this, this->logFile, this->sc));

                if ((pClient = socket.Accept()) != NULL)
                {
                    // get IP
                    client->ip = pClient->GetClientAddr();

                    // get SOCKET
                    client->sock = pClient->GetSocketDescriptor();

                    // get SOCKET OBJECT
                    client->sockObject = pClient;

                    //----------------------------------------------------------------------
                    // Receive request from the client.
                    //----------------------------------------------------------------------

                    // this keeps the client alive until it's destroyed
                    {
                        std::unique_lock<std::mutex> lg(m);
                        clients[client->sock] = client;
                    }

                    // handle connection should return immediately
                    client->handleConnection();

                }

            }
            else {

                try {

                    log->info("There are " + to_string(this->activeConnections) + " alive; go to sleep before accepting another one");
                    log->flush();

#ifdef _WIN32
                    Sleep(10000);
#else
                    sleep(10);
#endif

                }
                catch (...) {

                    log->error("an error occured while sleeping for 10 seconds");

                }

            }

        }
        catch (...) {

            log->error("an error occured while running");

#ifdef _WIN32
            Sleep(5000);
#else
            sleep(5);
#endif

        }

    }

    std::cout << "[SERVER]: exited from run" << std::endl;
    Sleep(25000);
    return 0;

}

/*
Server destructor. Closes and remove all structures created.
*/
Server::~Server() {

    if (sock != -1) {

#ifdef _WIN32
        _close(sock);
#else
        close(sock);
#endif
    }

}


///////////////////////////////////
//        PRIVATE METHODS        //
///////////////////////////////////


/*

Thread to check user inactivity state.

*/
void Server::checkUserInactivity() {

    while (running.load()) {

        log->info("[CHECK-INACTIVITY]: go to sleep for 60 seconds");
        log->flush();

#ifdef _WIN32
        Sleep(60000);
#else
        sleep(60);
#endif

        if (this->clients.size() > 0) {

            for (auto it = clients.begin(); it != clients.end(); ++it)
            {

                // client socket
                int sock = it->first;

                // client object
                auto client = it->second;

                try {

                    long msClient = client->activeMS.load();

                    milliseconds ms = duration_cast<milliseconds>(
                        system_clock::now().time_since_epoch()
                        );

                    long msNow = ms.count();

                    long msTotInactiveTime = msNow - msClient;

                    log->info("socket " + std::to_string(sock) + " was inactive for " + std::to_string(msTotInactiveTime));
                    log->flush();

                    if (msTotInactiveTime >= this->sc.userInactivityMS) {

                        log->info("[CHECK-INACTIVITY]: client - socket " + std::to_string(sock) + " will be closed for inactivity (" + std::to_string(sc.userInactivityMS) + " milliSeconds)");
                        log->flush();
                        client->running.store(false);

                        // shutdown both receive and send operations
                        client->sockObject->Close();

                    }

                }
                catch (...) {

                    log->error("an error occured checking inactivity for socket " + std::to_string(sock));
                }

            }

        }

    }

    log->info("[CHECK-INACTIVITY]: exited");
    log->flush();

}

/*

Close one client-socket and erase it from the local map.

*/
void Server::unregisterClient(int csock) {

    std::unique_lock <std::mutex> lg(m);

    try {

        clients.erase(csock);
        shutdown(csock, 2);
        log->info("Exited from waiting messages from socket: " + std::to_string(csock) + " \n");
        log->flush();

        this->activeConnections--;

    }
    catch (...) {
        log->error("an error occured unregistring client socket " + std::to_string(csock));
    }

}

/*

Check if the connection with the server IS CLOSED.

RETURN:

    true: connection IS CLOSED
    false: connection IS OPENED

*/
bool Server::isClosed(int sock) {

#ifdef _WIN32

    /*char buf;
    int err = recv(sock, &buf, 1, MSG_PEEK);
    return err == -1 ? false : true;*/

    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);
    timeval tv = { 0 };
    select(sock + 1, &rfd, 0, 0, &tv);

    if (!FD_ISSET(sock, &rfd))
        return false;

    u_long n = 0;
    ioctlsocket(sock, FIONREAD, &n);

    return n == 0;

#else

    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);
    timeval tv = { 0 };
    select(sock + 1, &rfd, 0, 0, &tv);

    if (!FD_ISSET(sock, &rfd))
        return false;

    int n = 0;
    ioctl(sock, FIONREAD, &n);

    return n == 0;

#endif

}
