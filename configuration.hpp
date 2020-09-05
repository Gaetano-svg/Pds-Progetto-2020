#include <string>
#include <vector>

namespace conf {

    struct user {

        std::string name;
        std::string loggerName;
        std::string folderName;

    };

    struct server {

        std::string ip;
        std::string port;

    };

    struct file {

        std::string name;
        std::string body; //file content
        double ts; //modify date

    };

    struct folder {

        int filesNumber;
        std::vector<std::string> files; // from the string array will be created the FILE array using the json library

    };

}