//先是自己的
#include "chat_message.hpp"
#include "Protocal.pb.h"

//然后是第三方的
#include <boost/asio.hpp>

//然后是c++库函数
#include <chrono>
#include <deque>
#include <iostream>
#include <thread>

//最后是c库函数
#include <cstdlib>
#include <unistd.h>

//done!!
using namespace chat::information;
using namespace messageDeal;

using boost::asio::ip::tcp;
//服务器和客户端的协议一般都是共用的
//这个统一的chat_message就相当于是协议
using chat_message_queue = std::deque<chat_message>;

//将时间戳转为日期
std::tm* gettm(int64_t timestamp)
{
    int64_t milli = timestamp+ (int64_t)8*60*60*1000;//此处转化为东八区北京>时间>，如果是其它时区需要按需求修改
    auto mTime = std::chrono::milliseconds(milli);
    auto tp=std::chrono::time_point<std::chrono::system_clock,std::chrono::milliseconds>(mTime);
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm* now = std::gmtime(&tt);
    return now;
}

//显示时间
void showTime(std::tm* theTime) {
    printf("%4d年%02d月%02d日 %02d:%02d:%02d  ",theTime->tm_year+1900,theTime->tm_mon+1,theTime->tm_mday,theTime->tm_hour,theTime->tm_min,theTime->tm_sec);
}

//input是传参，后面两个是输出
bool parseMessage(const std::string& input, int *type, std::string& outbuffer){
    //string返回的不是迭代器，和历史有关
    auto pos = input.find_first_of(" ");
    //这一部分负责解析，如果没有空格或者空格位置在第一个（没有头部），认为有错
    if(pos == std::string::npos || pos == 0)
        return false;
    //不同消息的消息实体不一样
    //比如"BindName ok"
    std::string command = input.substr(0,pos);
    if(command == "bindname") {
        std::string name = input.substr(pos+1);
        //如果type不是空指针,给type赋值
        if(*type == 0)
            *type = MT_BIND_NAME;
        //这里用protobuf
        PBindName bindInfo;
        bindInfo.set_name(name);
        //可以直接吧消息序列化成字符串的
        //这里outbuffer存的就是body
        bool ok = bindInfo.SerializeToString(&outbuffer);
        //如果想得到值，就用bindInfo.name()即可
        return ok;
    } else if(command == "chat") {
        std::string chat = input.substr(pos+1);
        if(*type == 0)
            *type = MT_CHAT_INFO;
        //这里用protobuf
        PChat info;
        info.set_information(chat);
        auto ok = info.SerializeToString(&outbuffer);
        return ok;
    }
    return false;
}

class chat_client{
    public:
        chat_client(boost::asio::io_context& io_context,
                const tcp::resolver::results_type& endpoints)
            : io_context_(io_context),
            socket_(io_context)
    { //这里在构造的时候就已经建立了网络连接
        //有优有劣，优就是接口比较简约；劣就是有时候不希望构造的时候就连接
        //灵活性会差一些
        do_connect(endpoints);
    }

        void write(const chat_message& msg)
        { //这里用的是post，为什么呢？
            //就相当于用post生成一个事件，这个事件在io_context的控制下去跑
            //因为io_context是可以在多线程下面跑的，在后面调用write是放到
            //另外一个线程里面跑的
            boost::asio::post(io_context_,
                    [this, msg]() //这里msg是值拷贝，而不是值引用
                    { //这里和chat message中的deliver处理是一样的
                        bool write_in_progress = !write_msgs_.empty();
                        write_msgs_.push_back(msg);
                        //只有write_msgs_是空的时候才进行do_write，防止调用两次do_write
                        if (!write_in_progress){
                            do_write();
                        }
                    });
        }

        void close()
        { //这里调用close的时候也调用post
            //就相当于用post生成一个事件，这个事件在io_context的控制下去跑
            //这里因为在不同线程下，可能会出现资源占用的情况，需要io控制
            boost::asio::post(io_context_, [this]() { socket_.close(); });
        }

    private:
        //这里是异步连接
        //有什么好处呢，比如说游戏，在后台连接的时候就会准备相关的
        //图形渲染，还有音效处理相关的东西，连接好了这些准备也准备好了 
        void do_connect(const tcp::resolver::results_type& endpoints){
            boost::asio::async_connect(socket_, endpoints,
                    [this](boost::system::error_code ec, tcp::endpoint)
                    { //回调函数
                        if (!ec){
                            do_read_header();
                        }
                    });
        }

        //这里和服务端一样，也是先读头部的信息
        void do_read_header(){
            //这里如果不给长度，会有异步触发的问题
            read_msg_.resize(chat_message::header_length);
            boost::asio::async_read(socket_, boost::asio::buffer(read_msg_.data(), chat_message::header_length),
                    [this](boost::system::error_code ec, std::size_t /*length*/){
                        if (!ec && read_msg_.decode_header()){
                            //通过头部检查body的合法性
                            do_read_body();
                        }
                        else
                        {   //这里错误处理是关闭连接，为什么是关闭连接呢？
                            //这是在同一个线程下面的处理，可以直接用close
                            //而不用io_context去控制
                            socket_.close();
                        }

                    });
        }

        //处理服务器发过来的东西，这里就是定义的RoomInformation
        void do_read_body(){
            read_msg_.resize(chat_message::header_length + read_msg_.body_length());
            boost::asio::async_read(socket_,
                    boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
                    [this](boost::system::error_code ec, std::size_t /*length*/){
                        if (!ec){
                            //如果是用protobuf处理:
                            PRoomInformation roomInfo;
                            //现在可以从string里面去解析了！
                            auto ok = roomInfo.ParseFromString(read_msg_.body());
                            //if(!ok) throw std::runtime_error("not valid message");
                            if(ok) {
                                showTime(gettm(roomInfo.time()));
                                std::cout << "client: '" << roomInfo.name() << "'";
                                std::cout << "  says : '" << roomInfo.information() << "'" << std::endl;
                            }else{
                                std::cout << "serialization error!" << std::endl;
                            }
                            do_read_header();
                        }
                        else{
                            socket_.close();
                        }
                    });
        }

        //往服务器里面写
        void do_write(){
            boost::asio::async_write(socket_, boost::asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
                    [this](boost::system::error_code ec, std::size_t /*length*/){
                        if (!ec){
                            write_msgs_.pop_front();
                            //没写完就继续写
                            if (!write_msgs_.empty()){
                                do_write();
                            }
                        }
                        else{
                            socket_.close();
                        }
                    });
        }

    private:
        //四个成员，前两个负责通信连接的，后两个负责收发消息
        boost::asio::io_context& io_context_;
        tcp::socket socket_;
        chat_message read_msg_;
        //std::deque<chat_message> == chat_message_queue
        chat_message_queue write_msgs_;
};

int main(int argc, char* argv[])
{
    try{
        //这个宏是为了判断是否兼容proto的前面的版本
        //因为是动态链接，可能分布到机器上会有问题
        GOOGLE_PROTOBUF_VERIFY_VERSION;
        if (argc != 3){
            //这里是服务器的ip和端口号
            std::cerr << "Usage: chat_client <host> <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);

        auto endpoints = resolver.resolve(argv[1], argv[2]);
        chat_client c(io_context, endpoints);

        std::thread t([&io_context](){ io_context.run(); });

        char line[chat_message::body_max_length+ 1];
        while (std::cin.getline(line, chat_message::body_max_length + 1)){
            chat_message msg;
            auto type = 0;
            //这里有点像迭代器，获得line的输入
            std::string input(line, line + std::strlen(line));
            std::string output;
            //都封装到这个parseMessage里面，整个框架就可以复用了
            if(parseMessage(input,&type,output)){
                //parse 把body解析到output里面去，setMessage搞成chat_message的格式
                msg.setMessage(type, output);
                c.write(msg);
                std::cout << "write message for server " << output.size() << std::endl;
            }
        }

        c.close();
        //这里如果不先close，t就永远不会结束，因为里面还有注册的事件
        t.join();
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << "\n";
    }
    //在系统释放内存前，提前释放内存，不然可能会有一些内存泄漏检测工具检测到
    //protobuf内存泄漏了
    google::protobuf::ShutdownProtobufLibrary(); 
    return 0;
}
