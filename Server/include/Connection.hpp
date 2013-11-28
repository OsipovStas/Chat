/* 
 * File:   Server.hpp
 * Author: stels
 *
 * Created on November 27, 2013, 7:43 PM
 */

#ifndef CONNECTION_HPP
#define	CONNECTION_HPP
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
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/thread.hpp>

#include "../../Core/Message.hpp"

using namespace boost::asio;
using namespace boost::posix_time;

class Connection : public boost::enable_shared_from_this<Connection>, boost::noncopyable {
public:
    typedef boost::system::error_code ErrorCode;
    typedef boost::shared_ptr<Connection> Ptr;

    void start();

    static Ptr createNewUser();

    void stop();

    bool started() const;

    ip::tcp::socket& sock();

    std::string getUsername() const;

    long long getAllTime() const;

    long long getRequestCounter() const;


private:
    typedef Connection SelfType;

    Connection();

    void handleRequest(Message);


    // Handlers
    ////////////////////////////////////////////////////////////////////////////////

    void onLogin(Message);

    void replyLogin();

    void onFetch(Message);

    void replyFetch(u_int32_t state);

    void onSend(Message);

    void replySend();

    void onLogout(Message);
    ///////////////////////////////////////////////////////////////////////////////////////

    void doReadHeader();

    void doReadBody(Message m);

    void doWrite(const Message m);
    ip::tcp::socket socket_;

    bool isStarted;

    std::string username;
    
    typedef void(Connection::*Handler)(Message);
    typedef std::unordered_map<u_int32_t, Handler> TypeHandlerMap;

    TypeHandlerMap handlers;

    //////////////////////////////
    // Timers
    long long allTime;
    long long requestCounter;
    boost::posix_time::ptime current;

    void startRequest();

    void completeRequest();

    /////////////////////////////////////////
    //Synchronization
    mutable boost::recursive_mutex userMutex;

};

#endif	/* CONNECTION_HPP */

