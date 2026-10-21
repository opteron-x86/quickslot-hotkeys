#pragma once

namespace quickslot {

class EditorLaunch {
    bool pending_=false;
    double deadline_=0;
public:
    enum class Result {idle,wait,open,cancel};
    bool pending()const{return pending_;}
    void request(double now){pending_=true;deadline_=now+3;}
    void cancel(){pending_=false;}
    Result poll(double now,bool safe,bool released) {
        if(!pending_)return Result::idle;
        if(!safe||now>=deadline_){cancel();return Result::cancel;}
        if(!released)return Result::wait;
        cancel();return Result::open;
    }
};
}
