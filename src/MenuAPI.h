#pragma once
#include <atomic>
#include <mutex>
#include <vector>

namespace quickslot {

class MenuAPIState {
    mutable std::mutex mutex_;
    bool ready_=false,open_=false;
public:
    enum class Command {none,open,close};
    // Gameplay requests from other mods, run on the game thread in request order.
    struct Action {enum class Type {swapPrevious,tapGroup} type=Type::swapPrevious;unsigned group=0;};
private:
    std::vector<Action> actions_;
    unsigned groups_=0;
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
    void publish(bool ready,bool open){std::scoped_lock lock(mutex_);ready_=ready;open_=open;if(!ready){command_=Command::none;actions_.clear();groups_=0;}}
    bool requestAction(Action action) {
        std::scoped_lock lock(mutex_);
        if(!ready_||open_||actions_.size()>=16||(action.type==Action::Type::tapGroup&&action.group>=groups_))return false;
        actions_.push_back(action);return true;
    }
    std::vector<Action> takeActions(){std::scoped_lock lock(mutex_);std::vector<Action> taken;taken.swap(actions_);return taken;}
    void publishGroups(unsigned count){std::scoped_lock lock(mutex_);groups_=ready_?count:0;}
    unsigned groups()const{std::scoped_lock lock(mutex_);return groups_;}
    bool hotkeyEnabled()const{return hotkey_.load();}
    void setHotkeyEnabled(bool enabled){hotkey_=enabled;}
};
inline MenuAPIState& menuAPI(){static MenuAPIState value;return value;}
}
