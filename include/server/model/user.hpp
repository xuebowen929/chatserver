#ifndef USER_HPP
#define USER_HPP

#include <string>
using namespace std;

// User表的 ORM(映射)类
class User{
public:
    User(int id = -1, string name = "", string pwd = "", string state = "offline"){
        this->id = id;
        this->name = name;
        this->password = pwd;
        this->state = state;
    }

    void setId(int id) { this->id = id;}
    void setName(string name) { this->name = name;}
    void setPassword(string pwd) { this->password = pwd;}
    void setState(string state) { this->state = state;}

    int getId() { return this->id;}
    string getName() { return this->name;}
    string getPassword() { return this->password;}
    string getState() { return this->state;}

protected:
    int id;
    string name;
    string password;
    string state;
};

#endif