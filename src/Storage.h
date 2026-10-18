#pragma once
#include "Model.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <stdexcept>

namespace quickslot {
using Json=nlohmann::json;
inline constexpr unsigned groupSchema=5,arsenalSchema=7;
inline std::string cleanName(const std::string& raw) {
    auto name=Json::parse(Json(raw).dump(-1,' ',false,Json::error_handler_t::replace)).get<std::string>();
    if(name.size()>512) {
        std::size_t end=512;
        while(end>0&&(static_cast<unsigned char>(name[end])&0xc0)==0x80)--end;
        name.resize(end);
    }
    return name.empty()?"Unnamed item":name;
}
inline Json encodeEntry(const Entry& e) {
    Json j={{"form",e.form},{"owner",e.owner},{"unique",e.unique},{"hand",unsigned(e.hand)},
        {"kind",unsigned(e.kind)},{"twoHanded",e.twoHanded},{"spell",e.spell},{"unresolved",e.unresolved},{"name",e.name},{"recipe",e.recipe}};
    if(e.kind==Kind::smartPotion){j["resource"]=e.resource;j["effect"]=e.effect;j["policy"]=e.policy;j["exclusions"]=Json::array();for(const auto& x:e.exclusions)j["exclusions"].push_back({{"form",x.form},{"recipe",x.recipe}});}
    if(e.kind==Kind::combo){j["parts"]=Json::array();for(const auto& part:e.parts)j["parts"].push_back(encodeEntry(part));}
    return j;
}
inline std::string encode(const Group& g) {
    Json entries=Json::array();for(const auto& e:g.entries)entries.push_back(encodeEntry(e));
    return Json{{"schema",groupSchema},{"selected",g.selected},{"entries",entries}}.dump();
}
inline Entry decodeEntry(const Json& v,const std::function<bool(std::uint32_t,std::uint32_t&)>& resolve,unsigned depth=0) {
    Entry e;
    const auto number=[&](const char* k,std::uint64_t max)->std::uint32_t {
        if(!v.at(k).is_number_unsigned())throw std::runtime_error("Invalid item reference");
        const auto n=v.at(k).get<std::uint64_t>();if(n>max)throw std::runtime_error("Item reference outside range");return std::uint32_t(n);
    };
    e.form=number("form",UINT32_MAX);e.owner=number("owner",UINT32_MAX);e.unique=std::uint16_t(number("unique",UINT16_MAX));
    e.hand=Hand(number("hand",1));e.kind=Kind(number("kind",unsigned(Kind::unarmed)));
    e.name=v.at("name").get<std::string>();if(e.name.size()>512)throw std::runtime_error("Item name too long");
    e.twoHanded=v.at("twoHanded").get<bool>();e.spell=v.at("spell").get<bool>();e.unresolved=v.at("unresolved").get<bool>();
    if(!v.at("recipe").is_number_unsigned())throw std::runtime_error("Invalid recipe signature");e.recipe=v.at("recipe").get<std::uint64_t>();
    if(isHandAction(e)) {
        if(!validHandAction(e)||(depth&&e.kind==Kind::unarmed)||v.contains("parts")||v.contains("exclusions")||v.contains("resource")||v.contains("effect")||v.contains("policy"))throw std::runtime_error("Invalid hand action");
        e.name=handActionName(e);return e;
    }
    if(e.kind==Kind::smartPotion||e.kind==Kind::combo) {
        if(depth||e.form||e.owner||e.unique||e.spell||e.twoHanded||e.hand!=Hand::right||e.recipe||e.unresolved)throw std::runtime_error("Invalid compound shortcut");
        if(e.kind==Kind::smartPotion) {
            e.resource=number("resource",2);e.effect=number("effect",1);e.policy=number("policy",2);
            if(e.effect&&e.policy==2)throw std::runtime_error("Best fit requires restoration");
            const auto& excluded=v.at("exclusions");if(!excluded.is_array()||excluded.size()>128)throw std::runtime_error("Invalid exclusions");
            for(const auto& x:excluded) {
                if(!x.at("form").is_number_unsigned()||x.at("form").get<std::uint64_t>()>UINT32_MAX||!x.at("recipe").is_number_unsigned())throw std::runtime_error("Invalid exclusion reference");
                const auto recipe=x.at("recipe").get<std::uint64_t>();std::uint32_t mapped=0;
                if(resolve(x.at("form").get<std::uint32_t>(),mapped)&&mapped)e.exclusions.push_back({mapped,recipe});
                else if(recipe)e.exclusions.push_back({UINT32_MAX,recipe});
            }
        } else {
            const auto& parts=v.at("parts");if(!parts.is_array()||parts.size()!=2)throw std::runtime_error("Combination needs two hands");
            for(const auto& part:parts)e.parts.push_back(decodeEntry(part,resolve,depth+1));
            for(unsigned i=0;i<2;++i){const auto& p=e.parts[i];if(isConsumable(p)||isVoice(p)||p.kind==Kind::ammo||p.twoHanded||unsigned(p.hand)!=i||(i==0&&(p.kind==Kind::shield||p.kind==Kind::torch)))throw std::runtime_error("Invalid combination hand");}
            if(!e.parts[0].unresolved&&!e.parts[1].unresolved&&!validPair(e.parts[0],e.parts[1]))throw std::runtime_error("Same physical copy in both hands");
        }
        return e;
    }
    if(unsigned(e.kind)>unsigned(Kind::spell)) {
        const bool spell=e.kind==Kind::power||e.kind==Kind::lesserPower;
        if(e.owner||e.unique||e.spell!=spell||(!flexibleHand(e)&&e.hand!=normalizedHand(e,e.hand))||(e.kind!=Kind::scroll&&e.twoHanded))throw std::runtime_error("Invalid stack or voice reference");
    }
    if(!e.form||e.unresolved){e.form=0;e.owner=0;e.unresolved=true;}
    else {
        std::uint32_t mapped=0;
        if(resolve(e.form,mapped)&&mapped)e.form=mapped;else{e.form=0;e.unresolved=true;}
        if(e.owner){if(resolve(e.owner,mapped)&&mapped)e.owner=mapped;else{e.owner=0;e.unresolved=true;}}
    }
    return e;
}
inline Group decode(const std::string& bytes,const std::function<bool(std::uint32_t,std::uint32_t&)>& resolve) {
    if(bytes.size()>262144)throw std::runtime_error("Quickslot data exceeds limit");
    const auto j=Json::parse(bytes);
    if(j.at("schema")!=groupSchema||!j.at("entries").is_array()||j.at("entries").size()>Group::limit)throw std::runtime_error("Unsupported Quickslot data");
    Group g;
    for(const auto& v:j.at("entries"))g.entries.push_back(decodeEntry(v,resolve));
    if(!j.at("selected").is_number_unsigned())throw std::runtime_error("Invalid selection");
    const auto selected=j.at("selected").get<std::uint64_t>();
    g.selected=g.entries.empty()?0:std::min<std::uint64_t>(selected,g.entries.size()-1);
    return g;
}
inline std::string encodeArsenal(const Arsenal& arsenal) {
    Json groups=Json::array();for(const auto& group:arsenal.groups)groups.push_back(Json::parse(encode(group)));
    return Json{{"schema",arsenalSchema},{"presetIdentity",arsenal.presetIdentity},{"active",arsenal.active},{"groups",groups}}.dump();
}
inline Arsenal decodeArsenal(const std::string& bytes,const std::function<bool(std::uint32_t,std::uint32_t&)>& resolve) {
    if(bytes.size()>2097152)throw std::runtime_error("Quickslot data exceeds limit");
    const auto j=Json::parse(bytes);Arsenal arsenal;
    if(j.at("schema")!=arsenalSchema||!j.at("groups").is_array()||j.at("groups").size()!=Arsenal::count||
       !j.at("active").is_number_unsigned())throw std::runtime_error("Unsupported group data");
    for(std::size_t i=0;i<Arsenal::count;++i)arsenal.groups[i]=decode(j.at("groups")[i].dump(),resolve);
    const auto active=j.at("active").get<std::uint64_t>();
    if(active>=Arsenal::count)throw std::runtime_error("Invalid active group");
    arsenal.active=std::size_t(active);arsenal.presetIdentity=j.at("presetIdentity").get<std::string>();
    if(arsenal.presetIdentity.size()>64)throw std::runtime_error("Invalid preset identity");
    return arsenal;
}
}
