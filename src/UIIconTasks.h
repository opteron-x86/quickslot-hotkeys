#pragma once
#include <functional>
#include <mutex>
#include <vector>

namespace quickslot {
class UIIconTasks {
    std::mutex mutex_;
    std::vector<std::function<void()>> pending_;
public:
    void post(std::function<void()> task) {
        std::scoped_lock lock(mutex_);
        pending_.push_back(std::move(task));
    }
    void run() {
        std::vector<std::function<void()>> work;
        {std::scoped_lock lock(mutex_);work.swap(pending_);}
        // Provider calls can enter the engine. Keep the render-side handoff free
        // while they run, and leave newly posted work for the next UI update.
        for(auto& task:work)task();
    }
};
}
