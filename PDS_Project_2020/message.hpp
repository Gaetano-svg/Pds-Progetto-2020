#include <string>
#ifndef Msg
#define Msg

struct message2 {

    std::string  type;
    int     typeCode;

    // std::filesystem::file_time_type    timestamp; DA MODIFICARE
    long timestamp;
    std::string  hash;

    int     packetNumber;
    std::string  folderPath;
    std::string  fileName; // will be the folder Path(for intial conf) or the file Path(for the other messages)
    std::string userName;

    std::string  body;

};

///////////////////////////////////
///*** INITIAL CONFIGURATION ***///
///////////////////////////////////

struct initialConf {

    std::string path;
    std::string hash;

};

#endif

