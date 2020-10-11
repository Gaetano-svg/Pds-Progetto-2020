#include <string>

namespace msg {

    // Messaggio generico
    // Il MESSAGGIO sarà sempre composto da tre campi:

    // 1. TYPE: descrizione alfanumerica del messaggio che si sta inviando
    // 2. CODE: codice numerico del tipo del messaggio (corrispondenza 1:1)
    // 3. BODY: il body del messaggio cambia in base al tipo: conoscendo il tipo 
    //          si potrà parsificare in modo diverso il body del messaggio per 
    //          estrarre informazioni differenti

    struct message {

        std::string type;
        int typeCode;
        std::string folderPath;
        std::string fileName;
        std::string fileContent;
        std::string userName;

    };

    ///////////////////////////////////
    ///*** INITIAL CONFIGURATION ***///
    ///////////////////////////////////

    struct initialConf {

        std::string path;
        std::string hash;

    };

    //////////////////////////////
    ///*** RESPONSE MESSAGE ***///
    //////////////////////////////

    // 0. OK -> la richiesta ricevuta è stata completata correttamente 
    // -1. PATH DOESN'T EXIST
    // -2. FILE DOESN'T EXIST

    ////////////////////////
    //*** FILE MESSAGE **///
    ////////////////////////

    // Strutture che il client inserirà nel body del messaggio quando 
    // si accorge dei cambiamenti:

    // 1. FileUpdate
    // 2. FileRename
    // 3. FileCreate
    // 4. FileDelete
    // 5. InitialConfigRequest

    // NB: dal momento che per i primi tre tipi di body le struct contengono
    // gli stessi campi stringa, bisogna vedere se è il caso di unirli in una
    // unica struct

    // il FolderPath che il client manda al server non sarà quello assoluto
    // che lui vede in locale, ma solo quello "a partire dalla folder di partenza"
    // il server poi (tramite un file da lui creato) saprà a quale folder è 
    // associato quell'utente 

    struct connection {

        std::string userName; // nome dell'utente che ha iniziato la connessione
        std::string folderPath; // path della cartella di cui allineare il backup 

    };

    struct fileUpdate {

        std::string folderPath;
        std::string name;
        std::string content;
        double ts; //modify date

    };

    struct fileRename {

        std::string folderPath;
        std::string oldName;
        std::string newName;
        double ts; //modify date

    };

    struct fileCreate {

        std::string folderPath;
        std::string name;
        std::string content;
        double ts; //modify date

    };

    struct fileDelete {

        std::string folderPath;
        std::string name;
        double ts; //modify date

    };

}