/* 
 * File:   Server.cpp
 * Author: stels
 *
 * Created on November 14, 2013, 9:56 PM
 */

#include <cstdlib>
#include <sys/types.h>

#include <unordered_map>
#include <iostream>
#include <sstream>
#include <deque>
#include <thread>


#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>


#include "../../Core/Message.hpp"

using boost::asio::ip::tcp;


typedef std::deque<Message> MessageQueue;

class Client {
public:

    Client(boost::asio::io_service& io_service, tcp::resolver::iterator endpoint_iterator,
            const std::string& username) :
    io_service_(io_service),
    socket_(io_service),
    readMsg(),
    writeMessages(),
    username(username),
    msgCount(-1),
    handlers({
        {Message::login_reply, &Client::onLogin},
        {Message::fetch_reply, &Client::onFetch},
        {Message::send_reply, &Client::onSend},
        {Message::logout_reply, &Client::onLogout}
    }) {
        doConnect(endpoint_iterator);
    }

    void close() {
        io_service_.post([this]() {
            socket_.close(); });
    }

    void postMessage(const Message m) {
        io_service_.post(
                [this, m]() {
                    writeMessages.push_front(m);
                });
    }

private:

    void doConnect(tcp::resolver::iterator endpoint_iterator) {
        boost::asio::async_connect(socket_, endpoint_iterator,
                [this](boost::system::error_code ec, tcp::resolver::iterator) {
                    if (!ec) {
                        doLogin();
                    }
                });
    }

    void doLogin() {
        Message msg(Message::login_request);
        msg.fillBody(username);
        doWrite(msg);
    }

    void doWrite(const Message m) {
        boost::asio::async_write(socket_,
                boost::asio::buffer(m.getData(), m.getDataLength()),
                [this](boost::system::error_code ec, std::size_t /*length*/) {
                    if (!ec) {
                        doReadHeader();
                    } else {
                        socket_.close();
                    }
                });
    }

    void doReadHeader() {
        boost::asio::async_read(socket_,
                boost::asio::buffer(readMsg.getData(), Message::HEADER_LENGTH),
                [this](boost::system::error_code ec, std::size_t /*length*/) {
                    if (!ec && readMsg.verifyHeader()) {
                        doReadBody();
                    } else {
                        socket_.close();
                    }
                });
    }

    void doReadBody() {
        boost::asio::async_read(socket_,
                boost::asio::buffer(readMsg.getBody(), readMsg.getBodyLength()),
                [this](boost::system::error_code ec, std::size_t /*length*/) {
                    if (!ec) {
                        handleReply();
                        doRequest();
                    } else {
                        socket_.close();
                    }
                });
    }

    void handleReply() {
        auto handler = handlers.find(readMsg.getMsgType());
        if (handler != handlers.end()) {
            (this->*(handler -> second))();
        } else {
            std::cout << "Wrong command " << readMsg.getMsgType() << std::endl;
            socket_.close();
        }
    }

    void onLogin() {
        std::istringstream ss(std::string(readMsg.getBody(), readMsg.getBodyLength()));
        ss >> msgCount;
    }

    void onFetch() {
        std::istringstream iss(std::string(readMsg.getBody(), readMsg.getBodyLength()));
        std::string msg;
        iss >> msg;
        if (!msg.empty()) {
            std::cout << msg << std::endl;
            ++msgCount;
        }
    }

    void onSend() {

    }

    void onLogout() {
        socket_.close();
    }

    void doRequest() {
        if (writeMessages.empty()) {
            doFetch();
        } else {
            doWrite(writeMessages.back());
            writeMessages.pop_back();
        }
    }

    void doFetch() {
        Message msg(Message::fetch_request);
        std::ostringstream oss;
        oss << msgCount;
        msg.fillBody(oss.str());
        doWrite(msg);
    }

    boost::asio::io_service& io_service_;
    tcp::socket socket_;
    Message readMsg;
    MessageQueue writeMessages;
    std::string username;
    int msgCount;

    typedef void(Client::*Handler)();
    typedef std::unordered_map<u_int32_t, Handler> TypeHandlerMap;

    TypeHandlerMap handlers;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 4) {
            std::cerr << "Usage: " << argv[0] << " <host> <port> <username>\n";
            return 1;
        }

        boost::asio::io_service io_service;
        tcp::resolver resolver(io_service);
        auto endpoint_iterator = resolver.resolve({argv[1], argv[2]});
        Client c(io_service, endpoint_iterator, std::string(argv[3]));

        std::thread t([&io_service]() {
            io_service.run(); });

        const static std::string EXIT("exit");
        while (true) {
            std::string msgStr;
            std::cin >> msgStr;
            if (msgStr == EXIT) {
                c.postMessage(Message::logoutRequest());
                break;
            }
            if (msgStr.size() < Message::MAX_LENGTH) {
                c.postMessage(Message::sendRequest(msgStr));
            }
        }
        c.close();
        t.join();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}



