#pragma once
#include <utf8proc.h>
#include <cstdlib>
#include <string>
#include <string_view>

namespace quickslot {
inline bool validUTF8(std::string_view text) {
    for(std::size_t i=0;i<text.size();) {
        utf8proc_int32_t code=0;
        const auto n=utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(text.data()+i),utf8proc_ssize_t(text.size()-i),&code);
        if(n<=0||code==0)return false;i+=std::size_t(n);
    }
    return true;
}
inline std::string foldedText(std::string_view text) {
    utf8proc_uint8_t* output=nullptr;
    const auto size=utf8proc_map(reinterpret_cast<const utf8proc_uint8_t*>(text.data()),utf8proc_ssize_t(text.size()),&output,
        utf8proc_option_t(UTF8PROC_STABLE|UTF8PROC_COMPOSE|UTF8PROC_CASEFOLD));
    if(size<0)return std::string(text);
    std::string result(reinterpret_cast<char*>(output),std::size_t(size));std::free(output);return result;
}
inline bool textContains(std::string_view text,std::string_view query){return query.empty()||foldedText(text).find(foldedText(query))!=std::string::npos;}
std::string gameTextUTF8(std::string_view text,unsigned codepage);
inline void eraseLastCharacter(std::string& text) {
    if(text.empty())return;
    auto at=text.size()-1;while(at&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)--at;text.resize(at);
}
}
