#include "friendmodel.hpp"
#include <db.h>
using namespace std;

void FriendModel::insert(int userid, int friendid)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values(%d, %d)", userid, friendid);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }

    char sql2[1024] = {0};
    sprintf(sql2, "insert into friend values(%d, %d)", friendid, userid);

    MySQL mysql2;
    if (mysql2.connect())
    {
        mysql2.update(sql2);
    }
}

vector<User> FriendModel::quary(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select a.id, a.name, a.state from user a left join friend b on b.friendid=a.id where b.userid=%d", userid);

    vector<User> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把userid用户所有离线消息放入vec中返回
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(stoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.emplace_back(user);
            }
            mysql_free_result(res);
        }
    }
    return vec;
}