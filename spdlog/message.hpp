#include <string>

namespace msg {

    // GENERIC MESSAGE

    struct message {

        std::string type;
        int typeCode;
        std::string body;

    };

    // SPECIFIC MESSAGES DERIVED FROM THE MESSAGE BODY
    // each message is recognized by the typeCode of the message itself

    struct connection {

    };

}