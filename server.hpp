#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <thread>
#include "json.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "configuration.hpp"
#include "clientConn.hpp"
#include <mutex>
#include <shared_mutex>

using namespace std;
using namespace nlohmann;
using pClient = std::shared_ptr<ClientConn>;

#define IDLE_TIMEOUT 60

class Server {

public:

    // Questo file viene creato e/o modificato all'avvio delle connessioni
    // dei client verso il server: contiene un array di STRUCT USER (si ricicla la
    // struct user ) che lato server indicano per ogni user in quale folder spostare
    // i file creati. Questo file deve essere creato dal SERVER. Non è di configurazione
    
    // questo file servirà al server per capire a quale path è associato l'utente

    std::vector <conf::user> usersPath;
    shared_ptr <spdlog::logger> log;
    nlohmann::json jServerConf;

    // the server configuration containing ip and port informations
    conf::server sc;

    std::mutex m;
    int sock = -1;
    bool running;
    std::map<int, pClient> clients; // mapping fd -> pClient

    string ip;
    int port;
    string logFile = "server_log.txt";

    Server (){}

    // init the Server Logger
    // return -1 in case of error
    int initLogger();

    int readConfiguration (string file);

    // metodo usato per inizializzare il vettore usersPath. Esso conterrà,
    // per ogni utente, il corrispettivo path iniziale in cui SALVARE le modifiche
    // che i vari client manderanno. Nel caso in cui tale file non dovesse esistere,
    // verrà creato e popolato per OGNI CONNESSIONE RICEVUTA.

    int readUsersPath();

    // a while loop used to listen to all the connection request

    int startListening();

    ~Server();

    void unregisterClient(int csock);

};