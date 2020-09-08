#include <string>
#include <vector>

namespace conf {

    // User Configuration File -> contiene la configurazione del Client
    struct user {

        std::string serverIp;
        int serverPort;
        std::string name;
        std::string folderPath;

    };

    // Server Configuration File -> contiente info utili al client per connettersi
    struct server {

        std::string ip;
        std::string port;

    };

}