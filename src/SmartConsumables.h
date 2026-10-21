#pragma once
#include "Model.h"
#include <limits>
#include <cstdio>

namespace quickslot {
struct PotionCandidate {
    Entry entry;
    int count=0;
    std::array<float,3> restore{},regeneration{};
};
inline std::string smartName(const Entry& e) {
    const std::array<const char*,3> resource{"Health","Magicka","Stamina"};
    const std::array<const char*,3> policy{"lowest","highest","best fit"};
    return std::string(resource[std::min(e.resource,2u)])+(e.effect?" regeneration":" restoration")+" · "+policy[std::min(e.policy,2u)];
}
inline Icon smartIcon(unsigned resource){return resource==1?Icon::magicka:resource==2?Icon::stamina:Icon::health;}
inline bool exclusionMatches(const PotionExclusion& x,const Entry& potion){return x.recipe?x.recipe==potion.recipe:x.form==potion.form;}
inline const PotionCandidate* choosePotion(const Entry& rule,const std::vector<PotionCandidate>& bottles,float deficit) {
    const PotionCandidate* best=nullptr;float bestScore=0;
    if(rule.resource>2||rule.effect>1||rule.policy>2)return nullptr;
    const auto strength=[&](const PotionCandidate& c){return (rule.effect?c.regeneration:c.restore)[rule.resource];};

    for(const auto& c:bottles) {
        if(c.count<=0||c.entry.unresolved||c.entry.kind!=Kind::potion||!c.entry.form)continue;
        if(std::any_of(rule.exclusions.begin(),rule.exclusions.end(),[&](const auto& x){return exclusionMatches(x,c.entry);}))continue;
        const float score=strength(c);if(!std::isfinite(score)||score<=0)continue;
        bool prefer=!best;
        if(best) {
            if(rule.policy==1)prefer=score>bestScore;
            else if(rule.policy==0||rule.effect)prefer=score<bestScore;
            else {const bool covers=score>=deficit,previous=bestScore>=deficit;prefer=covers!=previous?covers:covers?score<bestScore:score>bestScore;}
            if(score==bestScore)prefer=c.entry.form<best->entry.form;
        }
        if(prefer){best=&c;bestScore=score;}
    }
    return best;
}
struct PotionFeedback {std::string reason,estimate;};
inline PotionFeedback potionFeedback(const Entry& rule,const PotionCandidate* bottle,float missing) {
    missing=std::isfinite(missing)?std::max(0.f,missing):0.f;
    const char* resources[]{tr("Health"), tr("Magicka"), tr("Stamina")};
    if(!rule.effect&&rule.policy==2&&missing<.01f)return {std::string(resources[std::min(rule.resource,2u)])+tr(" full"), tr("No potion needed")};
    if(!bottle)return {tr("No matching potion"), ""};
    const float strength=(rule.effect?bottle->regeneration:bottle->restore)[std::min(rule.resource,2u)];
    const auto reason=rule.policy==0?tr("Lowest strength"):rule.policy==1?tr("Highest strength"):strength>=missing?tr("Smallest sufficient"):tr("Strongest available");
    char text[180];
    if(rule.effect)std::snprintf(text,sizeof(text),tr("Estimated regeneration: +%.0f%%"),strength);
    else std::snprintf(text,sizeof(text),tr("Missing %.0f · Estimated %.0f · %.0f %s"),missing,strength,std::abs(strength-missing),strength>=missing?tr("excess"):tr("shortfall"));
    return {reason,text};
}
inline Entry smartEntry(unsigned resource,unsigned effect,unsigned policy) {
    Entry e;e.kind=Kind::smartPotion;e.resource=std::min(resource,2u);e.effect=std::min(effect,1u);e.policy=std::min(policy,2u);
    if(e.effect&&e.policy==2)e.policy=0;
    e.name=smartName(e);e.icon=smartIcon(e.resource);return e;
}
}
