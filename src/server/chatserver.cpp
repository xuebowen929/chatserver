#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <functional>
using namespace std;

#include "json.hpp"
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册连接回调
    _server.setConnectionCallback(bind(&ChatServer::onConnection, this, _1));

    // 注册读写事件回调
    _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置服务器线程数量
    _server.setThreadNum(4);
}

void ChatServer::start()
{
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开连接
    if (!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

void ChatServer::onMessage(const TcpConnectionPtr & conn,
                           Buffer *buffer,
                           Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    // 数据反序列化
    json js = json::parse(buf);
    
    // 通过js[msgid]获取对应的业务处理器(Handler)
    // 目的: 完全解耦网络模块和业务模块的代码
    auto msgHandler = ChatService::instance()->getHandlerId(js["msgid"].get<int>());
    // 回调消息绑定好的业务处理器，来执行相应的业务处理
    msgHandler(conn, js, time);
}