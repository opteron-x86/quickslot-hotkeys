#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace quickslot {
struct ThemeFingerprint {
    std::uint64_t hash=0;
    std::size_t count=0;
    bool operator==(const ThemeFingerprint&)const=default;
};
inline bool watchedThemeExtension(const std::filesystem::path& path) {
    auto ext=path.extension().string();
    for(auto& c:ext)c=char(std::tolower(static_cast<unsigned char>(c)));
    return ext==".json"||ext==".png"||ext==".svg"||ext==".ttf"||ext==".otf";
}
// Computes a deterministic fingerprint of theme manifests, images and fonts up to depth 3 (capped at 512 files).
inline ThemeFingerprint scanThemeDirectory(const std::filesystem::path& dir) {
    ThemeFingerprint result;
    std::error_code ec;
    if(dir.empty()||!std::filesystem::is_directory(dir,ec))return result;
    struct Record {std::string rel;std::uintmax_t size;std::int64_t mtime;};
    std::vector<Record> records;
    std::vector<std::pair<std::filesystem::path,unsigned>> stack{{dir,0}};
    while(!stack.empty()&&records.size()<512) {
        const auto [current,depth]=stack.back();stack.pop_back();
        for(const auto& entry:std::filesystem::directory_iterator(current,ec)) {
            if(ec||records.size()>=512)break;
            if(entry.is_symlink(ec))continue;
            if(entry.is_directory(ec)) {
                if(depth<3)stack.emplace_back(entry.path(),depth+1);
                continue;
            }
            if(!entry.is_regular_file(ec)||!watchedThemeExtension(entry.path()))continue;
            const auto rel=entry.path().lexically_relative(dir).generic_string();
            const auto sz=entry.file_size(ec);if(ec)continue;
            const auto mt=entry.last_write_time(ec);if(ec)continue;
            records.push_back({rel,sz,std::int64_t(mt.time_since_epoch().count())});
        }
    }
    std::sort(records.begin(),records.end(),[](const Record& a,const Record& b){return a.rel<b.rel;});
    std::uint64_t h=14695981039346656037ull;
    const auto mix=[&](std::uint64_t v){h^=v;h*=1099511628211ull;};
    for(const auto& r:records) {
        for(unsigned char c:r.rel)mix(c);
        mix(0xff);mix(std::uint64_t(r.size));mix(std::uint64_t(r.mtime));
    }
    result.hash=h;result.count=records.size();
    return result;
}
class ThemeWatcher {
    double interval_=.5,lastCheck_=-1;
    bool primed_=false;
    ThemeFingerprint last_{};
public:
    explicit ThemeWatcher(double interval=.5):interval_(interval){}
    void reset(){primed_=false;lastCheck_=-1;last_={};}
    void sync(const std::filesystem::path& dir,double now) {
        last_=scanThemeDirectory(dir);primed_=true;lastCheck_=now;
    }
    bool poll(const std::filesystem::path& dir,double now) {
        if(!primed_){sync(dir,now);return false;}
        if(now-lastCheck_<interval_)return false;
        lastCheck_=now;
        const auto current=scanThemeDirectory(dir);
        if(current==last_)return false;
        last_=current;
        return true;
    }
};
}
