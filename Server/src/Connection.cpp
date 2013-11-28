#include "../../Core/Message.hpp"
#include "../include/Connection.hpp"
#include "../include/Server.hpp"


// Constants
const static std::string HELLO_MSG("Hello, ");
const static std::string BYE_MSG("Bye, ");
const static std::string USER_NAME_COLOR("\033[1;31;40m");
const static std::string SERVICE_COLOR("\033[1;34;40m");
const static std::string END_COLOR("\033[0m");


typedef boost::system::error_code ErrorCode;

void Connection::start() {
    Server::startConnection(shared_from_this());
    boost::recursive_mutex::scoped_lock lock(userMutex);
    isStarted = true;
    doReadHeader();
}

Connection::Ptr Connection::createNewUser() {
    Ptr newUser(new Connection);
    return newUser;
}

void Connection::stop() {
    {
        boost::recursive_mutex::scoped_lock lock(userMutex);
        if (!isStarted) return;
        isStarted = false;
        socket_.close();
    }
    Server::addMessage(SERVICE_COLOR + BYE_MSG + username + "!" + END_COLOR);
    Ptr self = shared_from_this();
    Server::stopConnection(self);
}

bool Connection::started() const {
    return isStarted;
}

ip::tcp::socket& Connection::sock() {
    return socket_;
}

std::string Connection::getUsername() const {
    return username;
}

long long Connection::getAllTime() const {
    boost::recursive_mutex::scoped_lock lock(userMutex);
    return allTime;
}

long long Connection::getRequestCounter() const {
    boost::recursive_mutex::scoped_lock lock(userMutex);
    return requestCounter;
}

Connection::Connection() : socket_(Server::getService()),
isStarted(false),
username(),
handlers({
    {Message::login_request, &Connection::onLogin},
    {Message::fetch_request, &Connection::onFetch},
    {Message::send_request, &Connection::onSend},
    {Message::logout_request, &Connection::onLogout}
}),
allTime(0),
requestCounter(0) {
}

void Connection::handleRequest(Message readMsg) {
    auto handler = handlers.find(readMsg.getMsgType());
    if (handler != handlers.end()) {
        (this->*(handler -> second))(readMsg);
    } else {
        std::cout << "Wrong command " << readMsg.getMsgType() << std::endl;
        stop();
    }
}


// Handlers
////////////////////////////////////////////////////////////////////////////////

void Connection::onLogin(Message readMsg) {
    std::istringstream iss(std::string(readMsg.getBody(), readMsg.getBodyLength()));
    {
        boost::recursive_mutex::scoped_lock lock(userMutex);
        std::getline(iss, username);
    }
    Server::addMessage(SERVICE_COLOR + HELLO_MSG + username + "!" + END_COLOR);
    std::cout << "Login " << username << std::endl;
    replyLogin();
}

void Connection::replyLogin() {
    Message msg(Message::login_reply);
    std::ostringstream oss;
    u_int32_t serverState = Server::getMessagesSize();
    oss << serverState;
    msg.fillBody(oss.str());
    doWrite(msg);
}

void Connection::onFetch(Message readMsg) {
    std::istringstream iss(std::string(readMsg.getBody(), readMsg.getBodyLength()));
    u_int32_t state;
    iss >> state;
    replyFetch(state);
}

void Connection::replyFetch(u_int32_t state) {
    Message msg(Message::fetch_reply);
    std::ostringstream oss;
    if (state < Server::getMessagesSize()) {
        oss << Server::getMessage(state);
    }
    msg.fillBody(oss.str());
    doWrite(msg);
    //    std::cout << "reply " << requestCounter << std::endl;
}

void Connection::onSend(Message readMsg) {
    std::istringstream iss(std::string(readMsg.getBody(), readMsg.getBodyLength()));
    std::string msg;
    std::getline(iss, msg);
    Server::addMessage(USER_NAME_COLOR + username + ": " + END_COLOR + msg);
    replySend();
}

void Connection::replySend() {
    Message msg(Message::send_reply);
    doWrite(msg);
}

void Connection::onLogout(Message readMsg) {
    Message msg(Message::logout_reply);
    doWrite(msg);
}

///////////////////////////////////////////////////////////////////////////////////////

void Connection::doReadHeader() {
//    std::cout << "Read header " << i++ << " " << std::endl;
    Message readMsg;
    boost::asio::async_read(socket_,
            boost::asio::buffer(readMsg.getData(), Message::HEADER_LENGTH),
            [this, readMsg](boost::system::error_code ec, std::size_t /*length*/) mutable {
                if (!ec && readMsg.verifyHeader()) {
                    startRequest();
                    doReadBody(readMsg);
                } else {
                    std::cout << "Problem" << std::endl;
                    stop();
                }
            });
}

void Connection::doReadBody(Message readMsg) {
    //    std::cout << "Do read body" << boost::this_thread::get_id() << " " << std::endl;
    boost::asio::async_read(socket_,
            boost::asio::buffer(readMsg.getBody(), readMsg.getBodyLength()),
            [this, readMsg](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    handleRequest(readMsg);
                } else {
                    std::cout << "Problem" << std::endl;
                    stop();
                }
            });
}

void Connection::doWrite(const Message writeMsg) {
    //    std::cout << "Do write " << boost::this_thread::get_id() << " " << writeMsg.getvP() << " " << writeMsg.getMsgType() << std::endl;
    boost::asio::async_write(socket_,
            boost::asio::buffer(writeMsg.getData(), writeMsg.getDataLength()),
            [this](boost::system::error_code ec, std::size_t sz/*length*/) {
                if (!ec) {
                    completeRequest();
                    doReadHeader();
                } else {
                    std::cout << "Problem" << std::endl;
                    stop();
                }
            });
}

void Connection::startRequest() {
    boost::recursive_mutex::scoped_lock lock(userMutex);
    current = boost::posix_time::microsec_clock::local_time();
}

void Connection::completeRequest() {
    boost::recursive_mutex::scoped_lock lock(userMutex);
    allTime += (boost::posix_time::microsec_clock::local_time() - current).total_milliseconds();
    ++requestCounter;
}
