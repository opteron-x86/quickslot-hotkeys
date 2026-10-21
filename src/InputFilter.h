#pragma once
#include <utility>
#include <vector>

namespace quickslot {

inline bool captureGameplayStick(bool& owned,bool neutral,bool browsing,bool menuOpen) {
    if(menuOpen){owned=false;return false;}
    const bool capture=browsing||owned;
    owned=capture&&!neutral;
    return capture;
}

template<class Node,class Predicate,class Forward>
void filterEvents(Node* head,Predicate consume,Forward forward) {
    struct Restore {
        std::vector<std::pair<Node*,Node*>> links;
        ~Restore(){for(const auto& [node,next]:links)node->next=next;}
    } restore;
    Node* filtered=nullptr;Node** tail=&filtered;
    for(auto* p=head;p;) {
        auto* next=p->next;restore.links.emplace_back(p,next);
        if(!consume(p)){*tail=p;tail=&p->next;}
        p=next;
    }
    *tail=nullptr;forward(&filtered);
}
}
