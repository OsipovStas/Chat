/* 
 * File:   Server.cpp
 * Author: stels
 *
 * Created on November 14, 2013, 9:56 PM
 */

#include "../../Core/Message.hpp"
#include "../include/Connection.hpp"
#include "../include/Server.hpp"

io_service& Server::getService() {
    return service;
}

void Server::stopServer() {
    service.stop();
    UserList copy;
    {
        boost::recursive_mutex::scoped_lock lock(usersMutex);
        copy = users;
    }
    boost::for_each(copy, [](const Ptr & p) {
        p -> stop();
    });
}

void Server::addMessage(const std::string& msg) {
    boost::recursive_mutex::scoped_lock lock(messagesMutex);
    messages.push_back(std::string(msg));
    std::cout << msg << std::endl;
}

std::string Server::getMessage(size_t index) {
    std::string msg;
    boost::recursive_mutex::scoped_lock lock(messagesMutex);
    msg = messages[index];
    return std::move(msg);
}

size_t Server::getMessagesSize() {
    size_t size;
    boost::recursive_mutex::scoped_lock lock(messagesMutex);
    size = messages.size();
    return size;
}

void Server::startConnection(const Ptr& p) {
    boost::recursive_mutex::scoped_lock lock(usersMutex);
    users.push_back(p);
}

void Server::stopConnection(const Ptr& p) {
    boost::recursive_mutex::scoped_lock lock(usersMutex);
    auto it = std::find(users.begin(), users.end(), p);
    users.erase(it);
    //            std::cout << "stop " << username << std::endl;
}

void Server::printStats(std::ostream& os) {
    UserList copy;
    {
        boost::recursive_mutex::scoped_lock lock(usersMutex);
        copy = users;
    }
    auto ptr2Time = [](const Ptr & p) {
        return p -> getAllTime();
    };
    double time = boost::accumulate(copy
            | boost::adaptors::transformed(boost::bind<long long>(ptr2Time, _1)), 0);
    auto ptr2ReqNum = [](const Ptr & p) {
        return p -> getRequestCounter();
    };
    double num = boost::accumulate(copy
            | boost::adaptors::transformed(boost::bind<long long>(ptr2ReqNum, _1)), 0);
    os << time << " ; " << num << " ; " << time / num << " ; " << copy.size() << std::endl;
}

void Server::handleAccept(Ptr user, const boost::system::error_code& err) {
    user->start();
    Connection::Ptr newUser = Connection::createNewUser();
    //std::cout << "Accepted" << std::endl;
    acceptor.async_accept(newUser->sock(), boost::bind(handleAccept, newUser, _1));
}

void Server::startWatcher(std::ostream& os) {
    serverTimer.expires_from_now(boost::posix_time::millisec(10000));
    serverTimer.async_wait([&](const boost::system::error_code & ec) {
        if (!ec) {
            printStats(os);
            startWatcher(os);
        }
    });
}

void Server::startServer() {
    Connection::Ptr user = Connection::createNewUser();
    acceptor.async_accept(user -> sock(), boost::bind(handleAccept, user, _1));
    for(int i = 0; i < THREADS_NUM; ++i) {
        threads.create_thread(listenThread);
    }
}

void Server::listenThread() {
    service.run();
}

io_service Server::service;
deadline_timer Server::serverTimer(Server::service);
ip::tcp::acceptor Server::acceptor(Server::service, ip::tcp::endpoint(ip::tcp::v4(), 33333));
boost::thread_group Server::threads;

Server::UserList Server::users;
boost::recursive_mutex Server::usersMutex;

std::vector<std::string > Server::messages;
boost::recursive_mutex Server::messagesMutex;

//////////////////////////////////////////////////////////////////////////////////


