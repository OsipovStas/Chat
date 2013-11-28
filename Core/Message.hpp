/* 
 * File:   ClientHeader.hpp
 * Author: stels
 *
 * Created on November 14, 2013, 2:03 PM
 */

#ifndef MESSAGEHEADER_HPP
#define	MESSAGEHEADER_HPP

#include <cstring>
#include <memory>
#include <iostream>
#include <boost/shared_array.hpp>


// 
// 0                               32                             63
// +-------------------------------+------------------------------+      ---
// |                               |                              |       ^
// |              vP               |             msgID            |       |
// |                               |                              |       |
// +-------------------------------+------------------------------+    16 bytes
// |                               |                              |       |
// |             flags             |            length            |       |
// |                               |                              |       v
// +-------------------------------+------------------------------+      ---

class Message {
public:

    enum MessageType {
        login_request = 1, send_request = 3, fetch_request = 5, logout_request = 7,
        login_reply = 2, send_reply = 4, fetch_reply = 6, logout_reply = 8
    };

    enum {
        HEADER_LENGTH = 16
    };

    enum {
        MAX_LENGTH = 4096
    };

    enum {
        VERSION = 1
    };

    Message() : data(new char[HEADER_LENGTH + MAX_LENGTH]) {
        std::fill(data.get(), data.get() + HEADER_LENGTH + MAX_LENGTH, 0);
    }

    explicit Message(u_int32_t type, u_int32_t vProtocol = VERSION, u_int32_t flags = 0) : data(new char[HEADER_LENGTH + MAX_LENGTH]) {
        std::fill(data.get(), data.get() + HEADER_LENGTH + MAX_LENGTH, 0);
        setvP(vProtocol);
        setMsgType(type);
        setFlags(flags);
    }

    Message(const Message&) = default;

    Message& operator=(const Message&) = default;

    void fillBody(const std::string& msg) {
        if (msg.size() < MAX_LENGTH) {
            std::memcpy(getBody(), msg.c_str(), msg.size());
            setBodyLength(msg.size());
        }
    }

    friend std::istream& operator>>(std::istream& is, Message& header) {
        return is.read(reinterpret_cast<char*> (header.data.get()), HEADER_LENGTH);
    }

    friend std::ostream& operator<<(std::ostream& os, const Message& header) {
        return os.write(reinterpret_cast<const char*> (header.data.get()), HEADER_LENGTH);
    }

    u_int32_t getBodyLength() const {
        return decode(12);
    }

    u_int32_t getvP() const {
        return decode(0);
    }

    u_int32_t getMsgType() const {
        return decode(4);
    }

    u_int32_t getFlags() const {
        return decode(8);
    }

    void setvP(u_int32_t n) {
        encode(0, n);
    }

    void setMsgType(u_int32_t n) {
        encode(4, n);
    }

    void setFlags(u_int32_t n) {
        encode(8, n);
    }

    void setBodyLength(u_int32_t n) {
        encode(12, n);
    }

    bool verifyHeader() {
        if (getBodyLength() > MAX_LENGTH) {
            setBodyLength(0);
            return false;
        }
        return true;
    }

    char* getData() {
        return data.get();
    }

    const char* getData() const {
        return data.get();
    }

    size_t getDataLength() const {
        return getBodyLength() + HEADER_LENGTH;
    }

    char* getBody() {
        return data.get() + HEADER_LENGTH;
    }

    const char* getBody() const {
        return data.get() + HEADER_LENGTH;
    }

    static Message logoutRequest() {
        return Message(logout_request);
    }

    static Message sendRequest(const std::string& str) {
        Message msg(send_request);
        msg.fillBody(str);
        return msg;
    }
private:

    u_int32_t decode(int a) const {
        u_int32_t res = 0;
        for (int i = 0; i < 4; ++i) {
            res += data[a + i] << (8 * (3 - i));
        }
        return res;
    }

    void encode(int a, u_int32_t n) {
        data[a] = static_cast<unsigned char> (n >> 24);
        data[a + 1] = static_cast<unsigned char> ((n >> 16) & (0xFF));
        data[a + 2] = static_cast<unsigned char> ((n >> 8) & (0xFF));
        data[a + 3] = static_cast<unsigned char> (n & 0xFF);
    }

    boost::shared_array<char> data;

};


#endif	/* MESSAGEHEADER_HPP */

