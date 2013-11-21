/* 
 * File:   ClientHeader.hpp
 * Author: stels
 *
 * Created on November 14, 2013, 2:03 PM
 */

#ifndef MESSAGEHEADER_HPP
#define	MESSAGEHEADER_HPP

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
class MessageHeader {
public:

    enum MessageTypes {
        login_request = 1, send_request = 3, fetch_request = 5, logout_request = 7,
        login_reply = 2, send_reply = 4, fetch_reply = 6, logout_reply = 8
    };

    MessageHeader() {
        std::fill(rep_, rep_ + sizeof (rep_), 0);
    }

    u_int32_t length() const {
        return decode(12);
    }

    u_int32_t vP() const {
        return decode(0);
    }

    u_int32_t msgID() const {
        return decode(4);
    }

    u_int32_t flags() const {
        return decode(8);
    }

    void vP(u_int32_t n) {
        encode(0, n);
    }
    
    
    void msgID(u_int32_t n) {
        encode(4, n);
    }
   

    void flags(u_int32_t n) {
        encode(8, n);
    }
    
   
    void length(u_int32_t n) {
        encode(12, n);
    }
    
    
    friend std::istream& operator>>(std::istream& is, MessageHeader& header) {
        return is.read(reinterpret_cast<char*> (header.rep_), 16);
    }

    friend std::ostream& operator<<(std::ostream& os, const MessageHeader& header) {
        return os.write(reinterpret_cast<const char*> (header.rep_), 16);
    }

private:

    u_int32_t decode(int a) const {
        u_int32_t res = 0;
        for(int i = 0; i < 4; ++i) {
            res += rep_[a + i] << (8 * (3 - i));
        }
        return res;
    }

    void encode(int a, u_int32_t n) {
        rep_[a] = static_cast<unsigned char> (n >> 24);
        rep_[a + 1] = static_cast<unsigned char> ((n >> 16) & (0xFF));
        rep_[a + 2] = static_cast<unsigned char> ((n >> 8) & (0xFF));
        rep_[a + 3] = static_cast<unsigned char> (n & 0xFF);
    }

    unsigned char rep_[16];
};


#endif	/* MESSAGEHEADER_HPP */

