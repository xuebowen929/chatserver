#include "json.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <functional>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>

#include "user.hpp"
#include "group.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User _currentUser;
// 记录当前登录用户的好友列表信息
vector<User> _currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> _currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 用于读写线程之间的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};

// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int);
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 聊天客户端的实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    if (argc < 3) // 判断命令的个数
    {
        cerr << "command invalid! example: ./ChatClient 192.168.31.128 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息(ip+port)
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // 初始化读写线程的信号量
    sem_init(&rwsem, 0, 0);

    // 与服务器连接成功, 启动接收子线程
    thread readTask(readTaskHandler, clientfd); // pthread_create
    readTask.detach(); // pthread_detach

    // main线程用于接受用户输入，负责发送数据
    for (;;)
    {
        // 显示首页菜单,登录,注册,退出
        cout << "===================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "===================" << endl;
        cout << "choice: ";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车,读完整数要接读残留回车

        switch (choice)
        {
        case 1: // login
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get();
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            g_isLoginSuccess = false;

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if(len == -1)
            {
                cerr << "send login msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待信号量, 子线程处理完登录响应后通知
            if(g_isLoginSuccess)
            {
                // 进入聊天主菜单页面
                isMainMenuRunning = true;
                mainMenu(clientfd);
            }
        }
        break;
        case 2: // register
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username: ";
            cin.getline(name, 50); // 使用cin或scanf时，碰到空格..会直接按enter处理
            cout << "userpassword: ";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd,request.c_str(), strlen(request.c_str())+1, 0);
            if(len == -1)
            {
                cerr << "send reg msg error:" << request << endl;
            }
            sem_wait(&rwsem);
        }
        break;
        case 3: // quit
        {
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        }
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }
    return 0;
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "======================login user======================" << endl;
    cout << "current login user => id:" << _currentUser.getId() << " name:" << _currentUser.getName() << endl;
    cout << "----------------------friend list---------------------" << endl;
    if (!_currentUserFriendList.empty())
    {
        for (User &user : _currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------group list----------------------" << endl;
    if (!_currentUserGroupList.empty())
    {
        for (Group &group : _currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}

// 处理登录响应
void doLoginResponse(json& responsejs)
{
    if(0 != responsejs["errno"].get<int>()) // 登陆失败
    {
        cerr << responsejs["errmsg"] << endl;
    }
    else // 登陆成功
    {
        // 记录当前用户的id和name
        _currentUser.setId(responsejs["id"].get<int>());
        _currentUser.setName(responsejs["name"]);

        // 记录当前用户的好友列表信息
        if(responsejs.contains("friends"))
        {
            // 先初始化
            _currentUserFriendList.clear();

            vector<string> vec = responsejs["friends"];
            for(string &str : vec)
            {
                json js = json::parse(str);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                _currentUserFriendList.emplace_back(user);
            }
        }

        // 记录当前用户的群组列表信息
        if(responsejs.contains("groups"))
        {
            // 初始化
            _currentUserGroupList.clear();

            vector<string> vec1 = responsejs["groups"];
            for(string &groupstr : vec1)
            {
                Group group;
                json grpjs = json::parse(groupstr);
                group.setId(grpjs["id"].get<int>());
                group.setName(grpjs["groupname"]);
                group.setDesc(grpjs["groupdesc"]);

                vector<string> vec2 = grpjs["users"];
                for(string &userstr : vec2)
                {
                    GroupUser user;
                    json js = json::parse(userstr);
                    user.setId(js["id"].get<int>());
                    user.setName(js["name"]);
                    user.setState(js["state"]);
                    user.setRole(js["role"]);
                    group.getUsers().emplace_back(user);
                }
                _currentUserGroupList.emplace_back(group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线信息，个人聊天或者群组聊天
        if(responsejs.contains("offlinemsg"))
        {
            vector<string> vec = responsejs["offlinemsg"];
            for(string &str : vec)
            {
                json js = json::parse(str);
                // time + [id] + name + "said" + xxx
                if(ONE_CHAT_MSG == js["msgid"].get<int>())
                {
                    cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                        << " said: " << js["msg"].get<string>() << endl;
                }
                else
                {
                    cout << "群消息[" << js["groupid"] << "]: " << js["time"].get<string>() << " [" << js["userid"] << "]" << js["username"].get<string>()
                        << " said: " << js["message"].get<string>() << endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
}

// 处理注册响应 
void doRegResponse(json& responsejs)
{
    if(0 != responsejs["errno"].get<int>()) // 注册失败
    {
        cerr << "name is already exist, register error!" << endl;
    }
    else // 注册成功
    {
        cout << "name register success, userid is " << responsejs["userId"]
                << ", do not forget it!" << endl;
    }
}

// 接收线程
void readTaskHandler(int clientfd)
{
    for(;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if(-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if(ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue; 
        }
        else if(GROUP_CHAT_MSG == msgtype)
        {
            cout << "群消息[" << js["groupid"] << "]: " << js["time"].get<string>() << " [" << js["userid"] << "]" << js["username"].get<string>()
                 << " said: " << js["message"].get<string>() << endl;
            continue;
        }else if(LOGIN_MSG_ACK == msgtype)
        {
            doLoginResponse(js);
            sem_post(&rwsem); // 通知主线程,已经处理完登录响应
            continue;
        }else if(REG_MSG_ACK == msgtype)
        {
            doRegResponse(js);
            sem_post(&rwsem); // 处理完注册响应,通知主线程
            continue;
        }
    }
}

// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void logout(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,格式addgroup:groupid"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"logout", "注销,格式logout"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"logout", logout}};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command;
        int idx = commandbuf.find(":");
        if(-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0,idx);
        }
        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }

        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要改mainMenu代码
        it->second(clientfd, commandbuf.substr(idx+1, commandbuf.size()-idx));
    }
}

// "help" command handler
void help(int , string)
{
    cout << "show command list" << endl;
    for(auto &p : commandMap)
    {
        cout << p.first << ": " << p.second << endl;
    }
    cout << endl;
}

// "chat" command handler
void chat(int clientfd, string str)
{
    int idx = str.find(":");
    if(idx == -1)
    {
        cerr << "chat command error" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string msg = str.substr(idx+1, str.size()-idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = _currentUser.getId();
    js["name"] = _currentUser.getName();
    js["to"] = friendid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(len == -1)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

// "addfriend" command handler
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = _currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len)
    {
        cerr << "send addfriend msg error ->" << buffer << endl;
    }
}

// "creategroup" command handler
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if(idx == -1)
    {
        cerr << "create group command error" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx+1, str.size()-idx);
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = _currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(len == -1)
    {
        cerr << "send creategroup msg error -> " << buffer << endl;
    }
}

// "addgroup" command handler
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["userid"] = _currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(len == -1)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
}

// "groupchat" command handler
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if(idx == -1)
    {
        cerr << "groupchat command error" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string msg = str.substr(idx+1, str.size()-idx);
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["userid"] = _currentUser.getId();
    js["username"] = _currentUser.getName();
    js["groupid"] = groupid;
    js["message"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(len == -1)
    {
        cerr << "send groupchat msg error -> " << buffer << endl;
    }
}

// "logout" command handler
void logout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = _currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(len == -1)
    {
        cerr << "send logout msg error -> " << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}

// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}