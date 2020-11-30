#include <string>
#include <vector>

#ifndef Configuration
#define Configuration

namespace conf {

    // User Configuration File -> contiene la configurazione del Client
    struct user {

        std::string serverIp;
        std::string serverPort;
        std::string name;
        std::string folderPath;
        int secondTimeout;

    };

    // Server Configuration File -> contiente info utili al client per connettersi
    struct server {

        std::string ip;
        std::string port;
        std::string backupFolder;
        int userInactivityMS;
        int numberActiveClients;
        int secondTimeout;

    };

}

#endif