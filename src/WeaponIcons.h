#pragma once
#include "Icons.h"
#include <initializer_list>
#include <string_view>

namespace quickslot {

struct WeaponIconSelection {
    Icon icon=Icon::automatic;
    unsigned priority=0;

    void consider(std::string_view keyword,bool twoHanded=false) {
        bool recognized=false;
        for(const auto prefix:{std::string_view("OCF_WeapType"),std::string_view("ABC_I4WIO_"),std::string_view("WeapType")}) {
            if(keyword.starts_with(prefix)){keyword.remove_prefix(prefix.size());recognized=true;break;}
        }
        if(!recognized)return;
        if(keyword.ends_with("1H")){keyword.remove_suffix(2);twoHanded=false;}
        else if(keyword.ends_with("2H")){keyword.remove_suffix(2);twoHanded=true;}
        const auto matches=[&](std::string_view name) {
            if(keyword.size()!=name.size())return false;
            for(std::size_t i=0;i<name.size();++i) {
                const auto c=keyword[i];
                if((c>='A'&&c<='Z'?char(c-'A'+'a'):c)!=name[i])return false;
            }
            return true;
        };
        struct Rule {std::string_view name;Icon icon;unsigned priority;};
        static constexpr Rule rules[]{
            {"pickaxe",Icon::pickaxe,80},{"woodaxe",Icon::woodcutters_axe,70},
            {"woodcuttersaxe",Icon::woodcutters_axe,70},{"woodcutteraxe",Icon::woodcutters_axe,70},
            {"woodhatchet",Icon::hatchet,80},{"hatchet",Icon::hatchet,40},
            {"nodachi",Icon::nodachi,60},{"daikatana",Icon::nodachi,60},{"greatkatana",Icon::nodachi,60},
            {"tanto",Icon::tanto,60},{"stiletto",Icon::stiletto,60},
            {"flamberge",Icon::flamberge,60},{"falchion",Icon::falchion,60},
            {"shortsword",Icon::shortsword,40},{"rapier",Icon::rapier,40},
            {"saber",Icon::saber,40},{"sabre",Icon::saber,40},{"scimitar",Icon::scimitar,40},
            {"katana",Icon::katana,40},
            {"morningstar",Icon::morningstar,60},{"spikedclub",Icon::spiked_club,60},
            {"club",Icon::club,30},{"maul",Icon::maul,60},
            {"shortspear",Icon::shortspear,60},{"pike",Icon::pike,50},{"spear",Icon::spear,30},
            {"naginata",Icon::naginata,60},{"glaive",Icon::glaive,40},{"halberd",Icon::halberd,30},
            {"trident",Icon::trident,60},{"scythe",Icon::scythe,60},
            {"unarmed",Icon::unarmed,40},{"javelin",Icon::javelin,60},{"quarterstaff",Icon::quarterstaff,60},
            {"claw",Icon::claw,60},{"whip",Icon::whip,60},{"gun",Icon::gun,60}
        };
        for(const auto& rule:rules)if(matches(rule.name)) {
            const auto candidate=rule.icon==Icon::katana&&twoHanded?Icon::nodachi:rule.icon;

            if(rule.priority>priority||(rule.priority==priority&&unsigned(candidate)<unsigned(icon))) {
                icon=candidate;priority=rule.priority;
            }
            return;
        }
    }
};
}
