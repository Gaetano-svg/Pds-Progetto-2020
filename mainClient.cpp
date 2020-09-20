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
#include "client.hpp"
#include "json.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

using namespace nlohmann;
using namespace std;
using json = nlohmann::json;

std::shared_ptr <spdlog::logger> myLogger;

      /// Reads n bytes from fd.
    bool readNBytes(int fd, void *buf, std::size_t n) {
        std::size_t offset = 0;
        char *cbuf = reinterpret_cast<char*>(buf);
        while (true) {
            ssize_t ret = recv(fd, cbuf + offset, n - offset, MSG_WAITALL);
            if (ret < 0) {
                if (errno != EINTR) {
                    // Error occurred
                    //throw IOException(strerror(errno));
                    return false;
                }
            } else if (ret == 0) {
                // No data available anymore
                if (offset == 0) return false;
                else             return false;//throw ProtocolException("Unexpected end of stream");
            } else if (offset + ret == n) {
                // All n bytes read
                return true;
            } else {
                offset += ret;
            }
        }
    }

    /// Reads message from fd
    string readResponse(int fd) {
        uint64_t size;
        std::string bufString;
            myLogger -> info("wait for length ");
        if (readNBytes(fd, &size, sizeof(size))) {

            myLogger -> info("size Risposta ricevuta " + to_string(size));
            char buf[size];
            memset(buf, 0, size);
            if (readNBytes(fd, buf, size)) {
                bufString = buf;
                myLogger -> info("Risposta ricevuta " + bufString);
            } else {
                //throw ProtocolException("Unexpected end of stream");
                bufString = buf;
            }
        }

        return bufString;
    }

// per ora stiamo considerando un solo utente e un solo folder

// in futuro dovremo gestire un array di utenti e per ciascuno un solo folder (o array di folder)
// in modo tale dal main, istanziare un thread per ogni utente il quale a sua volta si connetterà al server ...

int main()
{

    // Create Server Object
    Client client;
    client.readConfiguration();
    client.initLogger();
    client.serverConnection();
    client.sendConfiguration();

    cout << "send update message" << endl;
    msg::message fcu {
        "update",
        3,
        "test",
        "test",
        "test create"
    };
    client.sendMessage(fcu);
    client.readMessageResponse(client.sock);

    sleep(200);

    cout << "exit" << endl;

    //////////////////////////////////////////////////////////////
    //// si deve implementare la risposta da parte del server ////
    //////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////
    //// implementare logica invio pacchetti per lunghezza multipla di 1024 B ////
    //////////////////////////////////////////////////////////////////////////////

    // 5. Threads initialization

    return 0;
}
