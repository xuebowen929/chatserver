#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "group.hpp"
#include <string>
#include <vector>
class GroupModel{
public:
    // 创建群组 
    bool createGroup(Group& group);

    // 加入群组
    void addGroup(int userid, int groupid, string role);

    // 查询用户所在群组信息
    vector<Group> quary(int userid);

    // 根据指定的groupid查询群组用户id列表(除userid自己)
    vector<int> quaryGroupUsers(int userid, int groupid);
};

#endif