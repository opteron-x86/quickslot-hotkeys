#include "Unicode.h"
#ifdef _WIN32
#include <windows.h>
#endif
namespace quickslot {
std::string gameTextUTF8(std::string_view text,unsigned codepage) {
    if(text.empty()||validUTF8(text))return std::string(text);
#ifdef _WIN32

    const int n=MultiByteToWideChar(codepage,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);
    if(n>0) {
        std::wstring wide(std::size_t(n),L'\0');MultiByteToWideChar(codepage,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),wide.data(),n);
        const int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),n,nullptr,0,nullptr,nullptr);
        if(bytes>0){std::string result(std::size_t(bytes),'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),n,result.data(),bytes,nullptr,nullptr);return result;}
    }
#else
    (void)codepage;
#endif
    return std::string(text);
}
}
