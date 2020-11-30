#include <filesystem>
#include <boost/filesystem.hpp>
#include <fstream>
#include <atomic>
#include "client.hpp"
#include <iostream>
#include "FileWatcher.hpp"

// read client configuration file
Client client;

std::string getFilePath(const std::string& s) {

    char sep = '/';

    size_t i = s.rfind(sep, s.length());
    if (i != std::string::npos) {
        return(s.substr(0, i));
    }

    return("");
};

std::string getFileName(const std::string& s) {

    char sep = '/';

    size_t i = s.rfind(sep, s.length());
    if (i != std::string::npos) {
        return(s.substr(i + 1, s.length()));
    }

    return("");
};

//bool fun(info_backup_file i , std::atomic<bool>& b) {
//    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
//    std::this_thread::sleep_for(std::chrono::milliseconds(rand()%5000));
//    return true;
//}

bool fun(info_backup_file i, std::atomic<bool>& b) {

    std::string path = getFilePath(i.file_path);
    std::string name = getFileName(i.file_path);

    int operation;

    switch (i.status)
    {

    case FileStatus::created:
        operation = 1;
        break;

    case FileStatus::renamed:
        operation = 2;
        break;

    case FileStatus::erased:
        operation = 4;
        break;

    }

    int resCode;

    // SEND MESSAGE TO SERVER

    // connection
    client.serverConnection();
    long value_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::time_point_cast<std::chrono::milliseconds>(i.file_last_write).time_since_epoch()).count();
    if (i.status == FileStatus::renamed)
    {
        std::string rename = getFileName(i.adding_info);
        resCode = client.send(operation, path, name, rename, i.file_size, i.file_hash, value_ms, b);
    }

    else {

        resCode = client.send(operation, path, name, "", i.file_size, i.file_hash, value_ms, b);

    }

    // disconnection
    client.serverDisconnection();

    if (resCode < 0) {

        return false;
    }

    return true;

}

// Keep a record of files from the base directory and their last modification time

static int runs = 0;
FileWatcher::FileWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> delay) : path_to_watch{ path_to_watch }, delay{ delay } {
    /*for (auto& file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
        if (std::filesystem::is_regular_file(std::filesystem::path(file.path().string()))) {
            paths_[file.path().string()].file_last_write = std::filesystem::last_write_time(file);
        }

    }*/

    // read client configuration
    client.readConfiguration();

    // initialize client logger
    client.initLogger();

}

void FileWatcher::start(const std::function<void(std::string, int)>& action) {
    while (running_) {



        auto k = elements_to_backup3.begin();
        while (k != elements_to_backup3.end()) {
            switch (k->second.status) {
            case FileStatus::erased:
                std::cout << k->first << "  ------>  erased\n";
                break;
            case FileStatus::created:
                std::cout << k->first << "  ------>  created\n";
                break;
            case FileStatus::renamed:
                std::cout << k->first << "  ------>  renamed in " << k->second.adding_info << std::endl;
                break;
            }
            k++;
        }



        std::cout << "\n\n";


        /*

        for (auto v : elements_to_backup2) {
            std::cout << v.file_path << " --> ";
            switch (v.status) {
            case FileStatus::erased:
                std::cout << "erased\n";
                break;
            case FileStatus::modified:
                std::cout <<"modified\n";
                break;
            case FileStatus::created:
                std::cout << "created\n";
                break;
            case FileStatus::renamed:
                std::cout <<"renamed from "<< v.adding_info<<"\n";
                break;
            }
        }

        */

        // Wait for "delay" milliseconds

        std::this_thread::sleep_for(delay);

        //se � la prima volta che gira il programma (runs==0) tratta tutti i file del path come nuovi file da inviare in backup
        //DA MIGLIORARE
        if (runs == 0) {
            for (auto& file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
                if (std::filesystem::is_regular_file(file.path().string())) {
                    auto current_file_last_write_time = std::filesystem::last_write_time(file);
                    paths_[file.path().string()].file_last_write = current_file_last_write_time;
                    paths_[file.path().string()].file_size = file.file_size();
                    paths_[file.path().string()].file_hash = this->compute_hash(file.path().string());
                    action(file.path().string(), static_cast<int>(FileStatus::created));
                    info_backup_file k;
                    k.status = FileStatus::created;
                    k.file_path = file.path().string();
                    k.file_hash = this->compute_hash(file.path().string());
                    k.file_last_write = current_file_last_write_time;
                    k.file_size = file.file_size();
                    elements_to_backup3[file.path().string()] = k;
                }

            }

            //inizializzazione della tabella di thread e della tabella di booleani.
            //vengono creati tot thread a cui si passa la funzione da svolgere , il file e le sue info e il booleano che indica se fermarsi
            
            for (auto file = elements_to_backup3.begin(); file != elements_to_backup3.end(); ++file) {
                if (thread_table.size() < std::thread::hardware_concurrency()) {
                    thread_to_stop[file->first] = false;
                    thread_table.insert(std::make_pair(file->first, std::async(fun, elements_to_backup3[file->first], std::ref(thread_to_stop[file->first]))));
                }
                else {
                    break;
                }
            }

            runs++;
        }
        else {

            //AGGIORNAMENTO DELLA THREAD TABLE

            // (1) Se un task � terminato aggiorno  la tabella dei thread, la tabella booleani ed eventualmente elements_to_backup3,
            for (auto thr = thread_table.begin(), next_thr = thr; thr != thread_table.end(); thr = next_thr) {
                ++next_thr;
                if (thr->second.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                    if (thr->second.get() == true)
                        elements_to_backup3.erase(thr->first);
                    thread_to_stop.erase(thr->first); //possibile problema: se distruggo il booleano qua ma il thread che lo modifica deve ancora terminare?
                    thread_table.erase(thr);
                }
            }

            // (2) Faccio partire nuovi thread se possibile
            for (auto file = elements_to_backup3.begin(); file != elements_to_backup3.end(); ++file) {
                if (thread_table.size() < std::thread::hardware_concurrency()) {
                    if (thread_table.find(file->first) == thread_table.end()) {
                        thread_to_stop[file->first] = false;
                        thread_table.insert(std::make_pair(file->first, std::async(fun, elements_to_backup3[file->first], std::ref(thread_to_stop[file->first]))));
                    }
                }
                else {
                    break;
                }
            }






            // Check if a file was created or modified or renamed
            for (auto& file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
                if (std::filesystem::is_regular_file(file.path().string())) {
                    auto current_file_last_write_time = std::filesystem::last_write_time(file);
                    std::string file_name = file.path().string();
                    // File creation / rename
                    if (!contains(file_name)) {
                        //vedi se l'hash coincide con un file in path_
                        std::string hash_to_check = this->compute_hash(file.path().string());
                        auto j = paths_.begin();
                        int count = 0;
                        while (j != paths_.end()) {
                            if (hash_to_check == j->second.file_hash && file.file_size() == j->second.file_size && !std::filesystem::exists(j->first)) {

                                //RENAME---------------------------------------------------------------------------------------------------------------------------------


                                if (thread_table.find(j->first) != thread_table.end()) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla e poi BISOGNA NON AGGIORNARE IL FILEWATCHER PERCHE' AL GIRO SUCCESSIVO SI DEVE DI NUOVO ACCORGERE DI QUESTO EVENTO
                                    thread_to_stop[j->first] = true;
                                    break; // <- questo perch� il file watcher non deve accorgersi del nuovo file
                                }

                                if (elements_to_backup3.find(j->first) != elements_to_backup3.end()) {
                                    if (elements_to_backup3[j->first].status == FileStatus::created) { //CREATED
                                        elements_to_backup3.erase(j->first);
                                        info_backup_file k;
                                        k.status = FileStatus::created;
                                        k.file_path = file_name;
                                        k.file_hash = hash_to_check;
                                        k.file_last_write = current_file_last_write_time;
                                        k.file_size = file.file_size();
                                        elements_to_backup3[file_name] = k;
                                    }
                                }
                                else {
                                    //looking for a chain of rename   ----- BUG= NON BISOGNA PERMETTERE AD UN FILE DI RINOMINARSI IN SE STESSO (RISOLTO)
                                    bool flag_chain = false;
                                    bool flag_same = false;
                                    bool no_update = false;
                                    auto it = elements_to_backup3.begin();
                                    while (it != elements_to_backup3.end()) {
                                        if (it->second.status == FileStatus::renamed && it->second.adding_info == j->first && it->first != file_name) {

                                            if (thread_table.find(it->first) != thread_table.end()) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                                                thread_to_stop[it->first] = true;
                                                no_update = true;
                                                flag_chain = true;
                                                break;
                                            }
                                            else {
                                                flag_chain = true;
                                                info_backup_file k;
                                                k.status = FileStatus::renamed;
                                                k.file_path = it->first;
                                                k.file_hash = it->second.file_hash;
                                                k.file_last_write = it->second.file_last_write;
                                                k.file_size = it->second.file_size;
                                                k.adding_info = file.path().string();
                                                elements_to_backup3[it->first] = k;
                                                break;
                                            }


                                        }
                                        else if (it->second.status == FileStatus::renamed && it->second.adding_info == j->first && it->first == file_name) {

                                            if (thread_table.find(it->first) != thread_table.end()) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                                                thread_to_stop[it->first] = true;
                                                no_update = true;
                                                flag_same = true;
                                                break;
                                            }
                                            else {

                                                flag_same = true;
                                                elements_to_backup3.erase(it->first);
                                                break;
                                            }
                                        }
                                        it++;
                                    }

                                    if (no_update == true)
                                        break;




                                    if (!flag_chain && !flag_same) {
                                        //aggiungo rename 



                                        elements_to_backup3.erase(j->first);
                                        info_backup_file k;
                                        k.status = FileStatus::renamed;
                                        k.file_path = j->first;
                                        k.file_hash = j->second.file_hash;
                                        k.file_last_write = j->second.file_last_write;
                                        k.file_size = j->second.file_size;
                                        k.adding_info = file.path().string();
                                        elements_to_backup3[j->first] = k;
                                    }
                                }
                                paths_[file_name].file_last_write = current_file_last_write_time;
                                paths_[file_name].file_size = j->second.file_size;
                                paths_[file_name].file_hash = j->second.file_hash;
                                paths_.erase(j);
                                count--;
                                break;

                            }
                            j++;
                            count++;


                        }
                        if (count == paths_.size()) {
                            count = 0;

                            //CREATION-----------------------------------------------------------------------------------------

                            if (thread_table.find(file_name) != thread_table.end()) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                                thread_to_stop[file_name] = true;
                            }
                            else if (elements_to_backup3.find(file_name) != elements_to_backup3.end() && elements_to_backup3[file_name].status == FileStatus::renamed) {
                                //nulla perch� � necessario che prima venga processata la rename, la new viene ignorata.
                            }
                            else {

                                paths_[file.path().string()].file_last_write = current_file_last_write_time;
                                paths_[file.path().string()].file_size = file.file_size();
                                paths_[file.path().string()].file_hash = hash_to_check;
                                action(file.path().string(), static_cast<int>(FileStatus::created));

                                if (elements_to_backup3.find(file_name) != elements_to_backup3.end() && elements_to_backup3[file_name].status == FileStatus::erased) {
                                    info_backup_file k;
                                    k.status = FileStatus::created;
                                    k.file_path = file.path().string();
                                    k.file_hash = hash_to_check;
                                    k.file_last_write = current_file_last_write_time;
                                    k.file_size = file.file_size();
                                    elements_to_backup3[file_name] = k;
                                }
                                else {
                                    info_backup_file k;
                                    k.status = FileStatus::created;
                                    k.file_path = file.path().string();
                                    k.file_hash = hash_to_check;
                                    k.file_last_write = current_file_last_write_time;
                                    k.file_size = file.file_size();
                                    elements_to_backup3[file_name] = k;
                                }
                            }
                        }


                    }
                    // MODIFICATION-------------------------------------------------------------------------------------------
                    else {

                        if (thread_table.find(file_name) != thread_table.end()) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                            thread_to_stop[file_name] = true;
                        }
                        else {

                            if (paths_[file.path().string()].file_last_write != current_file_last_write_time) {
                                std::string  hash_to_add = this->compute_hash(file_name);
                                paths_[file.path().string()].file_last_write = current_file_last_write_time;
                                paths_[file.path().string()].file_hash = hash_to_add;
                                paths_[file.path().string()].file_size = file.file_size();
                                action(file.path().string(), static_cast<int>(FileStatus::created));

                                if (elements_to_backup3.find(file_name) != elements_to_backup3.end() && elements_to_backup3[file_name].status == FileStatus::created) {
                                    elements_to_backup3[file_name].file_hash = hash_to_add;
                                    elements_to_backup3[file_name].file_last_write = current_file_last_write_time;
                                    elements_to_backup3[file_name].file_size = file.file_size();
                                }
                                else if (elements_to_backup3.find(file_name) == elements_to_backup3.end()) {
                                    info_backup_file k;
                                    k.status = FileStatus::created;
                                    k.file_path = file_name;
                                    k.file_hash = hash_to_add;
                                    k.file_last_write = current_file_last_write_time;
                                    k.file_size = file.file_size();
                                    elements_to_backup3[file_name] = k;
                                }
                                else {
                                    std::cout << "\n\nMODIFICATION AFTER DELETE OR RENAME --> ERROR\n\n";
                                    exit(666);
                                }
                            }
                        }
                    }
                }

            }

            //ERASED ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
            auto it = paths_.begin();
            while (it != paths_.end()) {
                if (!std::filesystem::exists(it->first)) {
                    action(it->first, static_cast<int>(FileStatus::erased));

                    if (thread_table.find(it->first) != thread_table.end()) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                        thread_to_stop[it->first] = true;
                    }
                    else {

                        if (thread_table.find(it->first) != thread_table.end() && elements_to_backup3[it->first].status == FileStatus::created) {
                            elements_to_backup3.erase(it->first);
                            it = paths_.erase(it);
                        }
                        else if (thread_table.find(it->first) == thread_table.end()) {
                            bool b = false;
                            auto iter = elements_to_backup3.begin();
                            while (iter != elements_to_backup3.end()) {
                                if (iter->second.adding_info == it->first && iter->second.status == FileStatus::renamed) {
                                    if (thread_table.find(it->first) != thread_table.end()) { //se c'� un thread che sta lavorando quel file gli si dice di smetterla
                                        thread_to_stop[iter->first] = true;
                                        b = true;
                                        break;
                                    }
                                    info_backup_file k;
                                    k.status = FileStatus::erased;
                                    k.file_path = iter->first;
                                    elements_to_backup3[iter->first] = k;
                                    b = true;
                                    it = paths_.erase(it);
                                    break;
                                }
                                iter++;
                            }
                            if (!b) {
                                info_backup_file k;
                                k.status = FileStatus::erased;
                                k.file_path = it->first;
                                elements_to_backup3[it->first] = k;
                                it = paths_.erase(it);
                            }

                        }
                        else {
                            std::cout << "\n\DELETE AFTER DELETE OR RENAME --> ERROR\n\n";
                            exit(666);
                        }

                    }
                }
                else {
                    it++;
                }
            }
        }
    }
}

bool FileWatcher::contains(const std::string& key) {
    auto el = paths_.find(key);
    return el != paths_.end();
}

std::string FileWatcher::compute_hash(const std::string file_path)
{
    std::string result;

    CryptoPP::Weak::MD5 hash;
    CryptoPP::FileSource(file_path.c_str(), true, new
        CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(new
            CryptoPP::StringSink(result), false)));
    return result;
}
