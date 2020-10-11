#include <string>
#include <vector>

namespace utilities {

    // bisogna capire se il client manda un messaggio per ogni file 
    // oppure il client per ogni giro del while raccoglie info in queste struct 

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