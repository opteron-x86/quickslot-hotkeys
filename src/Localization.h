#pragma once
#include "Unicode.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <vector>

namespace quickslot {
inline bool languageID(std::string_view id) {
    return !id.empty()&&id.size()<=48&&std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';});
}
inline std::string languageCode(std::string language) {
    for(auto& c:language)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
    static const std::map<std::string,std::string> aliases{{"english","en"},{"french","fr"},{"german","de"},{"italian","it"},{"spanish","es"},{"russian","ru"},{"polish","pl"},{"japanese","ja"},{"korean","ko"},{"chinese","zh-cn"},{"schinese","zh-cn"},{"tchinese","zh-tw"},{"portuguese","pt"},{"brazilian","pt-br"},{"turkish","tr"}};
    if(const auto it=aliases.find(language);it!=aliases.end())return it->second;
    return languageID(language)?language:"en";
}
inline unsigned languageCodepage(std::string_view language) {
    if(language=="ru"||language=="uk"||language=="bg")return 1251;
    if(language=="pl"||language=="cs"||language=="hu")return 1250;
    if(language=="ja")return 932;if(language=="ko")return 949;
    if(language=="zh-cn")return 936;if(language=="zh-tw")return 950;
    if(language=="tr")return 1254;if(language=="el")return 1253;return 1252;
}

inline std::string formatSignature(std::string_view text) {
    std::string result;
    for(std::size_t i=0;i<text.size();++i)if(text[i]=='%') {
        if(++i==text.size())return "!";
        if(text[i]=='%')continue;
        while(i<text.size()&&std::string_view("-+ #0.123456789").find(text[i])!=std::string_view::npos)++i;
        if(i<text.size()&&text[i]=='*'){result+="*";++i;}
        const auto start=i;while(i<text.size()&&std::string_view("hljztL").find(text[i])!=std::string_view::npos)++i;
        if(i==text.size()||std::string_view("diuoxXfFeEgGaAcsp").find(text[i])==std::string_view::npos)return "!";
        result+=std::string(text.substr(start,i-start+1))+";";
    }
    return result;
}
struct LanguagePack {std::string id,name;std::map<std::string,std::string,std::less<>> strings;};
class Localization {
    std::vector<std::unique_ptr<LanguagePack>> packs_;
    std::vector<std::string> warnings_;
    std::atomic<const LanguagePack*> active_{nullptr};
    std::string gameLanguage_="en";
public:

    void load(const std::filesystem::path& directory,std::string gameLanguage="en") {
        gameLanguage_=languageCode(std::move(gameLanguage));
        std::error_code ec;
        if(!std::filesystem::is_directory(directory,ec))return;
        std::vector<std::filesystem::path> files;
        for(const auto& file:std::filesystem::directory_iterator(directory,ec))if(file.is_regular_file()&&file.path().extension()==".json")files.push_back(file.path());
        std::sort(files.begin(),files.end());
        for(const auto& path:files)try {
            if(std::filesystem::file_size(path)>2097152)throw std::runtime_error("File too large");
            std::ifstream stream(path,std::ios::binary);const auto j=nlohmann::json::parse(stream);
            auto pack=std::make_unique<LanguagePack>();pack->id=j.at("language").get<std::string>();pack->name=j.at("name").get<std::string>();
            if(!languageID(pack->id)||pack->id=="auto"||pack->name.empty()||pack->name.size()>128||!validUTF8(pack->name))throw std::runtime_error("Invalid language name");
            if(std::any_of(packs_.begin(),packs_.end(),[&](const auto& p){return p->id==pack->id;}))throw std::runtime_error("Duplicate language");
            const auto& strings=j.at("strings");if(!strings.is_object()||strings.size()>4096)throw std::runtime_error("Invalid strings");
            for(const auto& [key,value]:strings.items()) {
                if(!value.is_string())continue;const auto translated=value.get<std::string>();
                if(key.empty()||key.size()>4096||translated.empty()||translated.size()>4096||!validUTF8(key)||!validUTF8(translated)||translated.find("##")!=std::string::npos||formatSignature(key)=="!"||formatSignature(key)!=formatSignature(translated)) {
                    warnings_.push_back(path.filename().string()+": ignored invalid translation for "+key);continue;
                }
                pack->strings.emplace(key,translated);
            }
            packs_.push_back(std::move(pack));
        }catch(const std::exception& e){warnings_.push_back(path.filename().string()+": "+e.what());}
    }
    void select(std::string_view requested) {
        auto id=requested=="auto"?std::string_view(gameLanguage_):requested;
        const LanguagePack* found=nullptr;
        for(const auto& pack:packs_)if(pack->id==id){found=pack.get();break;}
        if(!found)if(const auto dash=id.find('-');dash!=std::string_view::npos)for(const auto& pack:packs_)if(pack->id==id.substr(0,dash)){found=pack.get();break;}
        active_.store(found);
    }
    const char* text(const char* english)const {
        if(english)if(const auto* pack=active_.load()){const auto it=pack->strings.find(english);if(it!=pack->strings.end())return it->second.c_str();}
        return english;
    }
    unsigned codepage()const{return languageCodepage(gameLanguage_);}
    const auto& packs()const{return packs_;}const auto& warnings()const{return warnings_;}
};
inline Localization& localization(){static Localization value;return value;}
inline const char* tr(const char* english){return localization().text(english);}
inline std::string translatedLabel(const char* english) {
    const std::string original=english;const auto end=original.find("##");const auto visible=original.substr(0,end);
    if(visible.empty())return original;
    const std::string translated=tr(visible.c_str());

    return translated==visible?original:translated+"###"+original;
}
inline std::string translatedChoices(const char* choices) {
    std::string result;
    for(const char* item=choices;*item;item+=std::char_traits<char>::length(item)+1){result+=tr(item);result+='\0';}
    result+='\0';return result;
}
}
