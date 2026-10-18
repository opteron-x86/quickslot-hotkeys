#pragma once
#include "EditorModel.h"
#include "ItemIdentity.h"
#include <deque>

namespace quickslot {
class EditHistory {
    struct State {Arsenal arsenal;Options options;};
    State baseline_;
    std::optional<State> pending_;
    std::deque<State> undo_;
    bool coalescing_=false,modified_=false;
    double last_=0;
public:
    void begin(const Arsenal& a,const Options& o){baseline_={a,o};undo_.clear();pending_.reset();coalescing_=modified_=false;last_=0;}
    void prepare(const Arsenal& a,const Options& o){pending_=State{a,o};}
    void finish(bool changed,bool continuous,double now) {
        if(changed&&pending_) {
            if(!(continuous&&coalescing_&&now-last_<.45)) {
                if(undo_.size()==32)undo_.pop_front();
                undo_.push_back(std::move(*pending_));
            }
            modified_=true;coalescing_=continuous;last_=now;
        }
        pending_.reset();
    }
    bool undo(Arsenal& a,Options& o){if(undo_.empty())return false;a=undo_.back().arsenal;o=undo_.back().options;undo_.pop_back();coalescing_=false;modified_=true;return true;}
    bool revert(Arsenal& a,Options& o){if(!modified_)return false;prepare(a,o);finish(true,false,0);a=baseline_.arsenal;o=baseline_.options;modified_=false;return true;}
    bool canUndo()const{return !undo_.empty();}
    bool canRevert()const{return modified_;}
    void identity(const IdentityChange& change){followIdentityChange(baseline_.arsenal,change);for(auto& s:undo_)followIdentityChange(s.arsenal,change);if(pending_)followIdentityChange(pending_->arsenal,change);}
};
class SettingsAutosave {
    bool pending_=false;
    double deadline_=0;
public:
    void change(double now){pending_=true;deadline_=now+.35;}
    bool due(double now)const{return pending_&&now>=deadline_;}
    bool pending()const{return pending_;}
    void saved(){pending_=false;}
    void failed(double now){deadline_=now+3;}
};
}
