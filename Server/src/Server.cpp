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
#include <functional>
#include <array>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "MessageHeader.hpp"

using namespace boost::asio;
using namespace boost::posix_time;


// Forwars Definitions
class Connection;

// Typedefs & MACROSES
typedef boost::shared_ptr<Connection> Ptr;
typedef std::vector<Ptr> UserList;

#define MEM_FN(x) boost::bind(&SelfType::x, shared_from_this())
#define MEM_FN1(x,y) boost::bind(&SelfType::x, shared_from_this(),y)
#define MEM_FN2(x,y,z) boost::bind(&SelfType::x, shared_from_this(),y,z)
#define HEADER_SIZE 16
// GLOBALS
io_service service;
ip::tcp::acceptor acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), 33333));
UserList users;
std::vector<boost::shared_ptr<std::string> > messages;
const static std::string HELLO_MSG("Hello, ");
const static std::string BYE_MSG("Bye, ");
//////////////////////////////////////////////////////////////////////////////////

class Connection : public boost::enable_shared_from_this<Connection>, boost::noncopyable {
public:
    typedef boost::system::error_code ErrorCode;
    typedef boost::shared_ptr<Connection> Ptr;

    void start() {
        isStarted = true;
        users.push_back(shared_from_this());
        lastPing = boost::posix_time::microsec_clock::local_time();
        doRead();
    }

    static Ptr createNewUser() {
        Ptr newUser(new Connection);
        return newUser;
    }

    void stop() {
        if (!isStarted) return;
        isStarted = false;
        mySocket.close();
        addMessage(BYE_MSG + username + "!");
        Ptr self = shared_from_this();
        auto it = std::find(users.begin(), users.end(), self);
        users.erase(it);
    }

    bool started() const {
        return isStarted;
    }

    ip::tcp::socket& sock() {
        return mySocket;
    }

    std::string getUsername() const {
        return username;
    }


private:
    typedef Connection SelfType;

    Connection() : mySocket(service),
    isStarted(false),
    myTimer(service),
    canRead(sizeof (MessageHeader)),
    isHeaderRead(false),
    bytesToWrite(0),
    handlers({
        {1, &Connection::onLogin},
        {3, &Connection::onFetch},
        {5, &Connection::onSend},
        {7, &Connection::onLogout}
    }) {
    }

    void addMessage(const std::string& msg) {
        messages.push_back(boost::make_shared<std::string>(std::string(msg)));
    }

    void onRead(const ErrorCode& err, size_t bytes) {
        if (err) {
            stop();
        }
        if (!started()) {
            return;
        }

        std::istream is(&requestBuffer);
        MessageHeader header;
        is >> header;

        auto handler = handlers.find(header.msgID());
        if (handler != handlers.end()) {
            (this->*(handler -> second))();
        } else {
            std::cout << "Wrong command " << header << std::endl;
        }
    }

    void onWrite(const ErrorCode& err, size_t bytes) {
        doRead();
    }
    // Handlers
    ////////////////////////////////////////////////////////////////////////////////

    void onLogin() {
        std::istream is(&requestBuffer);

        MessageHeader header;

        is >> header;
        is >> username;
        addMessage(HELLO_MSG + username + "!");
        replyLogin();
    }

    void replyLogin() {
        MessageHeader mh;

        mh.vP(1);
        mh.msgID(MessageHeader::login_reply);
        mh.length(HEADER_SIZE);

        replyBuffer.consume(replyBuffer.size());
        std::ostream os(&replyBuffer);

        os << mh << messages.size();
        doWrite(HEADER_SIZE + mh.length());
    }

    void onFetch() {
        std::istream is(&requestBuffer);

        MessageHeader header;
        u_int32_t state;

        is >> header;
        is >> state;

        replyFetch(state);
    }

    void replyFetch(u_int32_t state) {
        MessageHeader mh;

        mh.vP(1);
        mh.msgID(MessageHeader::fetch_reply);

        replyBuffer.consume(replyBuffer.size());
        std::ostream os(&replyBuffer);

        if (state < messages.size()) {
            mh.length(messages[state] -> size());
            os << mh << *(messages[state]);
        } else {
            os << mh;
        }
        doWrite(HEADER_SIZE + mh.length());
    }

    void onSend() {
        std::istream is(&requestBuffer);

        MessageHeader header;
        std::string msg;

        is >> header >> msg;
        addMessage(msg);

        replySend();
    }

    void replySend() {
        MessageHeader mh;

        mh.vP(1);
        mh.msgID(MessageHeader::send_reply);

        replyBuffer.consume(replyBuffer.size());
        std::ostream os(&replyBuffer);

        os << mh;
        doWrite(HEADER_SIZE);
    }

    void onLogout() {
        MessageHeader mh;

        mh.vP(1);
        mh.msgID(MessageHeader::logout_reply);
        replyBuffer.consume(replyBuffer.size());
        std::ostream os(&replyBuffer);

        os << mh;
        doWrite(HEADER_SIZE);
    }

    ///////////////////////////////////////////////////////////////////////////////////////

    void doRead() {
        requestBuffer.consume(requestBuffer.size());
        async_read(mySocket, requestBuffer.prepare(32346),
                MEM_FN2(readComplete, _1, _2), MEM_FN2(onRead, _1, _2));
        postCheckPing();
    }

    void doWrite(size_t bytes) {
        bytesToWrite = bytes;
        async_write(mySocket, replyBuffer,
                MEM_FN2(writeComplete, _1, _2), MEM_FN2(onWrite, _1, _2));
    }

    void onCheckPing() {
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        if ((now - lastPing).total_milliseconds() > 7000) {
            std::cout << "stopping " << username << " - no ping in time" << std::endl;
            stop();
        }
        lastPing = boost::posix_time::microsec_clock::local_time();
    }

    void postCheckPing() {
        myTimer.expires_from_now(boost::posix_time::millisec(3000));
        myTimer.async_wait(MEM_FN(onCheckPing));
    }

    size_t writeComplete(const boost::system::error_code& err, size_t bytes) {
        bytesToWrite -= bytes;
        return bytesToWrite;
    }

    size_t readComplete(const boost::system::error_code& err, size_t bytes) {
        if (err) {
            return 0;
        }
        canRead -= bytes;
        if (canRead == 0 && isHeaderRead) {
            isHeaderRead = false;
            canRead = sizeof (MessageHeader);
            return 0;
        }
        if (canRead == 0) {
            std::istream is(&requestBuffer);
            MessageHeader header;
            is >> header;
            canRead = header.length();
            isHeaderRead = true;
        }
        return canRead;
    }

    ip::tcp::socket mySocket;

    bool isStarted;
    deadline_timer myTimer;

    size_t canRead;
    bool isHeaderRead;
    size_t bytesToWrite;

    typedef void(Connection::*Handler)();
    typedef std::unordered_map<u_int32_t, Handler> TypeHandlerMap;

    TypeHandlerMap handlers;

    std::string username;
    boost::posix_time::ptime lastPing;
    boost::asio::streambuf replyBuffer;
    boost::asio::streambuf requestBuffer;
};

void handleAccept(Connection::Ptr user, const boost::system::error_code& err) {
    user->start();
    Connection::Ptr newUser = Connection::createNewUser();
    acceptor.async_accept(newUser->sock(), boost::bind(handleAccept, newUser, _1));
}

int main(int argc, char** argv) {
    Connection::Ptr user = Connection::createNewUser();
    acceptor.async_accept(user -> sock(), boost::bind(handleAccept, user, _1));
    service.run();
}

