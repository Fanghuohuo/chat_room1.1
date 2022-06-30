#ifndef CHAT_MESSAGE_HPP
#define CHAT_MESSAGE_HPP
#include "Protocal.pb.h"

#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

//服务器开发关键主要是对客户端数据的处理

namespace messageDeal {

    //获取时间戳
    std::time_t getTimeStamp()
    {
        std::chrono::time_point<std::chrono::system_clock,std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        auto tmp=std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        std::time_t timestamp = tmp.count();
        //std::time_t timestamp = std::chrono::system_clock::to_time_t(tp);
        return timestamp;
    }

    //4字节对齐
    struct Header{
        int32_t bodySize;
        int32_t type;
    }__attribute__((aligned(4)));

    enum MessageType {
        MT_BIND_NAME = 1,
        MT_CHAT_INFO = 2,
        MT_ROOM_INFO = 3,
    };

    //这里相当于把聊天对话的信息封装了一下
    class chat_message
    {
        public:
            //首先定义了头部长度和最大body的长度
            //因为头部是定长的，所以好处理，所以一开始一般先处理头部
            enum { header_length = sizeof(Header) };
            enum { body_max_length = 1460 };

            chat_message() {m_data.resize(header_length);}

            const char* data() const{
                return m_data.data();
            }

            void resize(int size) {
                m_data.resize(size);
            }

            //有这个和const版本就够了，反正string的赋值运算符是深复制
            char* data(){
                return const_cast<char*>( m_data.data() );
            }

            const char* body() const{
                return const_cast<char*>( m_data.data() ) + header_length;
            }

            char* body() {
                return const_cast<char*>( m_data.data() ) + header_length;
            }

            std::size_t length() const{
                return header_length + m_header.bodySize;
            }

            //返回m_header的成员type即可
            int type() const {
                return m_header.type;
            }

            std::size_t body_length() const{
                return m_header.bodySize;
            }

            void setMessage(int messageType, const std::string& buffer){
                //assert(buffer.size() <= body_max_length);
                m_header.bodySize = buffer.size();
                m_header.type = messageType;
                resize(buffer.size() + header_length);
                std::memcpy(body(), buffer.data(), buffer.size());
                std::memcpy(data(), &m_header, header_length);
            } 

            //对header进行分析（其实header就存了body的长度）
            bool decode_header(){
                //先提取出header
                std::memcpy(&m_header, data(), header_length);
                //之后判断header的合法性
                if(m_header.bodySize > body_max_length){
                    std::cout << "body size " << m_header.bodySize << " is too long!!"
                        << "type is " << m_header.type << std::endl;
                    return false; 
                }
                return true;
            }

        private:
            Header m_header;
            std::string m_data;
    };
}
#endif // CHAT_MESSAGE_HPP
