#pragma once
#include <atomic>
#include <mutex>

namespace quickslot {

class MenuAPIState {
    mutable std::mutex mutex_;
    bool ready_=false,open_=false;
public:
    enum class Command {none,open,close};
private:
    Command command_=Command::none;
    std::atomic_bool hotkey_{true};
public:
    bool requestOpen() {
        std::scoped_lock lock(mutex_);
        if(!ready_)return false;
        if(open_&&command_!=Command::close)return true;
        command_=Command::open;return true;
    }
    void requestClose(){std::scoped_lock lock(mutex_);command_=Command::close;}
    bool isOpen()const{std::scoped_lock lock(mutex_);return open_;}
    bool pending()const{std::scoped_lock lock(mutex_);return command_==Command::open;}
    Command take(){std::scoped_lock lock(mutex_);const auto c=command_;command_=Command::none;return c;}
    void publish(bool ready,bool open){std::scoped_lock lock(mutex_);ready_=ready;open_=open;if(!ready)command_=Command::none;}
    bool hotkeyEnabled()const{return hotkey_.load();}
    void setHotkeyEnabled(bool enabled){hotkey_=enabled;}
};
inline MenuAPIState& menuAPI(){static MenuAPIState value;return value;}
}
