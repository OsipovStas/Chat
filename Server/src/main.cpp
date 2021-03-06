/* 
 * File:   main.cpp
 * Author: stels
 *
 * Created on November 27, 2013, 8:04 PM
 */

#include <iostream>

#include "../include/Connection.hpp"
#include "../include/Server.hpp"


int main(int argc, char** argv) {
    std::ofstream os("log.txt", std::ofstream::out);
    Server::startWatcher(os);
    Server::startServer();
    while(true) {
        std::string msg;
        std::cin >> msg;
        if(msg == "exit") {
            Server::stopServer();
            std::cout << "Server stopped" << std::endl;
            break;
        }
    }
}
