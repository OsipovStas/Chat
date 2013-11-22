/* 
 * File:   Server.cpp
 * Author: stels
 *
 * Created on November 14, 2013, 9:56 PM
 */

#include <cstdlib>
#include <sys/types.h>

#include <memory>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <functional>
#include <string>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "../../Core/Message.hpp"

using namespace boost::asio;
using namespace boost::posix_time;


// Forwars Definitions
class Connection;

// Typedefs & MACROSES
typedef boost::shared_ptr<Connection> Ptr;
typedef std::vector<Ptr> UserList;

// GLOBALS
io_service service;
ip::tcp::acceptor acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), 33333));
UserList users;
std::vector<std::string > messages;
const static std::string HELLO_MSG("Hello, ");
const static std::string BYE_MSG("Bye, ");


const static std::string USER_NAME_COLOR("\033[1;31;40m");
const static std::string SERVICE_COLOR("\033[1;34;40m");
const static std::string END_COLOR("\033[0m");

//////////////////////////////////////////////////////////////////////////////////

class Connection : public boost::enable_shared_from_this<Connection>, boost::noncopyable {
public:
    typedef boost::system::error_code ErrorCode;
    typedef boost::shared_ptr<Connection> Ptr;

    void start() {
        isStarted = true;
        users.push_back(shared_from_this());
        lastPing = boost::posix_time::microsec_clock::local_time();
        doReadHeader();
    }

    static Ptr createNewUser() {
        Ptr newUser(new Connection);
        return newUser;
    }

    void stop() {
        if (!isStarted) return;
        isStarted = false;
        socket_.close();
        addMessage(SERVICE_COLOR + BYE_MSG + username + "!" + END_COLOR);
        Ptr self = shared_from_this();
        auto it = std::find(users.begin(), users.end(), self);
        users.erase(it);
        std::cout << "stop " << username << std::endl;
    }

    bool started() const {
        return isStarted;
    }

    ip::tcp::socket& sock() {
        return socket_;
    }

    std::string getUsername() const {
        return username;
    }


private:
    typedef Connection SelfType;

    Connection() : socket_(service),
    isStarted(false),
    myTimer(service),
    username(),
    readMsg(),
    handlers({
        {Message::login_request, &Connection::onLogin},
        {Message::fetch_request, &Connection::onFetch},
        {Message::send_request, &Connection::onSend},
        {Message::logout_request, &Connection::onLogout}
    }) {
    }

    void addMessage(const std::string& msg) {
        messages.push_back(std::string(msg));
    }

    void handleRequest() {
        auto handler = handlers.find(readMsg.getMsgType());
        if (handler != handlers.end()) {
            (this->*(handler -> second))();
        } else {
            std::cout << "Wrong command " << readMsg.getMsgType() << std::endl;
            stop();
        }
    }


    // Handlers
    ////////////////////////////////////////////////////////////////////////////////

    void onLogin() {
        std::istringstream iss(std::string(readMsg.getBody(), readMsg.getBodyLength()));
        std::getline(iss, username);
        addMessage(SERVICE_COLOR + HELLO_MSG + username + "!" + END_COLOR);
        std::cout << "Login " << username << std::endl;
        replyLogin();
    }

    void replyLogin() {
        Message msg(Message::login_reply);
        std::ostringstream oss;
        oss << messages.size();
        msg.fillBody(oss.str());
        doWrite(msg);
    }

    void onFetch() {
        std::istringstream iss(std::string(readMsg.getBody(), readMsg.getBodyLength()));
        u_int32_t state;
        iss >> state;
        replyFetch(state);
    }

    void replyFetch(u_int32_t state) {
        Message msg(Message::fetch_reply);
        std::ostringstream oss;
        if (state < messages.size()) {
            oss << messages[state];
        }
        msg.fillBody(oss.str());
        doWrite(msg);
    }

    void onSend() {
        std::istringstream iss(std::string(readMsg.getBody(), readMsg.getBodyLength()));
        std::string msg;
        std::getline(iss, msg);
        addMessage(USER_NAME_COLOR + username + ": " + END_COLOR + msg);
        replySend();
    }

    void replySend() {
        Message msg(Message::send_reply);
        doWrite(msg);
    }

    void onLogout() {
        Message msg(Message::logout_reply);
        doWrite(msg);
    }

    ///////////////////////////////////////////////////////////////////////////////////////

    void doReadHeader() {
        boost::asio::async_read(socket_,
                boost::asio::buffer(readMsg.getData(), Message::HEADER_LENGTH),
                [this](boost::system::error_code ec, std::size_t /*length*/) {
                    if (!ec && readMsg.verifyHeader()) {
                        doReadBody();
                    } else {
                        stop();
                    }
                });
        //        postCheckPing();
    }

    void doReadBody() {
        boost::asio::async_read(socket_,
                boost::asio::buffer(readMsg.getBody(), readMsg.getBodyLength()),
                [this](boost::system::error_code ec, std::size_t /*length*/) {
                    if (!ec) {
                        lastPing = boost::posix_time::microsec_clock::local_time();
                        handleRequest();
                    } else {
                        stop();
                    }
                });
    }

    void doWrite(const Message m) {
        boost::asio::async_write(socket_,
                boost::asio::buffer(m.getData(), m.getDataLength()),
                [this](boost::system::error_code ec, std::size_t /*length*/) {
                    if (!ec) {
                        doReadHeader();
                    } else {
                        stop();
                    }
                });
    }

    void onCheckPing() {
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        if ((now - lastPing).total_milliseconds() > 5000) {
            std::cout << "stopping " << username << " - no ping in time" << std::endl;
            stop();
        }
        lastPing = boost::posix_time::microsec_clock::local_time();
    }

    void postCheckPing() {
        myTimer.expires_from_now(boost::posix_time::millisec(6000));
        myTimer.async_wait([this](boost::system::error_code ec) {
            if (!ec) {
                postCheckPing();
            } else {
                stop();
            }
        });
    }

    ip::tcp::socket socket_;

    bool isStarted;
    deadline_timer myTimer;

    std::string username;
    boost::posix_time::ptime lastPing;
    Message readMsg;

    typedef void(Connection::*Handler)();
    typedef std::unordered_map<u_int32_t, Handler> TypeHandlerMap;

    TypeHandlerMap handlers;
};

void handleAccept(Connection::Ptr user, const boost::system::error_code& err) {
    user->start();
    Connection::Ptr newUser = Connection::createNewUser();
    std::cout << "Accepted" << std::endl;
    acceptor.async_accept(newUser->sock(), boost::bind(handleAccept, newUser, _1));
}

int main(int argc, char** argv) {
    Connection::Ptr user = Connection::createNewUser();
    acceptor.async_accept(user -> sock(), boost::bind(handleAccept, user, _1));
    service.run();
}

