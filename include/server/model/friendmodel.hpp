#ifndef FRIENDMODEL_HPP
#define FRIENDMODEL_HPP

#include "user.hpp"
#include <vector>
class FriendModel{
public:
    // 添加好友关系 
    void insert(int userid, int friendid);

    // 返回好友关系列表
    vector<User> quary(int userid);
};

#endif