/* 
 * File:   Server.hpp
 * Author: stels
 *
 * Created on November 27, 2013, 7:57 PM
 */

#ifndef SERVER_HPP
#define	SERVER_HPP

#include <cstdlib>
#include <sys/types.h>

#include <memory>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <utility>

#include <boost/range/numeric.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/thread.hpp>

#include "Connection.hpp"


class Server : boost::noncopyable {
public:
    typedef boost::shared_ptr<Connection> Ptr;
    typedef std::vector<Ptr> UserList;

    enum {
        THREADS_NUM = 5
    };
    
    static void listenThread();
    
    static void addMessage(const std::string& msg);

    static std::string getMessage(size_t index);

    static size_t getMessagesSize();

    static void startConnection(const Ptr& p);

    static void stopConnection(const Ptr& p);

    static void printStats(std::ostream& os);

    static void handleAccept(Ptr user, const boost::system::error_code& err);

    static void startWatcher(std::ostream& os);
    
    static void startServer();

    static void stopServer();
    
    static io_service& getService();

private:

    Server() {
    };

    static io_service service;
    static deadline_timer serverTimer;
    static ip::tcp::acceptor acceptor;
    static boost::thread_group threads;

    static UserList users;
    static boost::recursive_mutex usersMutex;

    static std::vector<std::string > messages;
    static boost::recursive_mutex messagesMutex;
};


#endif	/* SERVER_HPP */

