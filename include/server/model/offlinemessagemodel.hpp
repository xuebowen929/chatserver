#ifndef OFFLINEMESSAGEMODEL_HPP
#define OFFLINEMESSAGEMODEL_HPP

#include <string>
#include <vector>
using namespace std;

class OfflineMsgModel{
public:
    // 存储用户离线信息
    void insert(int userid, string msg);

    // 查询用户离线信息
    vector<string> quary(int userid);

    // 删除用户离线信息
    void remove(int userid);
};

#endif