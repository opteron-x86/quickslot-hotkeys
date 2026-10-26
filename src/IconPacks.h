#pragma once
#include "Icons.h"
#include "Unicode.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>
#include <set>

namespace quickslot {
struct IconPack {std::string id,name;std::filesystem::path atlas;};
inline bool validIconPackID(const std::string& id) {
    if(id.empty()||id.size()>64)return false;
    return std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';});
}
inline std::vector<IconPack> loadIconPacks(const std::filesystem::path& directory) {
    std::vector<IconPack> packs{{"quickslot","Quickslot",directory/"items.png"}};
    const auto folder=directory/"packs";
    std::error_code error;
    if(!std::filesystem::is_directory(folder,error))return packs;
    std::vector<std::filesystem::path> files;
    for(auto it=std::filesystem::directory_iterator(folder,error);!error&&it!=std::filesystem::directory_iterator();it.increment(error)) {
        std::error_code entryError;
        if(it->is_directory(entryError)&&!it->is_symlink(entryError)&&!entryError&&files.size()<64)files.push_back(it->path()/"pack.json");
    }
    std::sort(files.begin(),files.end());std::set<std::string> seen{"quickslot","i4"};
    for(const auto& file:files)try {
        if(!std::filesystem::is_regular_file(file)||std::filesystem::is_symlink(file)||std::filesystem::file_size(file)>16384)continue;
        std::ifstream stream(file);const auto j=nlohmann::json::parse(stream);
        const auto id=j.at("id").get<std::string>(),name=j.at("name").get<std::string>(),atlas=j.at("atlas").get<std::string>();
        if(j.at("schema")!=1||!validIconPackID(id)||seen.contains(id)||!validUTF8(name)||name.empty()||name.size()>80||name.find("##")!=std::string::npos)continue;
        if(atlas.empty()||atlas.find_first_of("/\\:")!=std::string::npos||atlas.find('\0')!=std::string::npos||std::filesystem::path(atlas).extension()!=".png")continue;
        if(j.at("columns")!=iconColumns||j.at("rows")!=iconRows||j.at("cellSize")!=iconCell||j.at("icons")!=iconCount)continue;
        const auto path=file.parent_path()/atlas;if(!std::filesystem::is_regular_file(path)||std::filesystem::is_symlink(path))continue;
        packs.push_back({id,name,path});seen.insert(id);
    }catch(const std::exception&){}
    return packs;
}
}
