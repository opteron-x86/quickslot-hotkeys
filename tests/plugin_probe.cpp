
#include "../src/Version.h"
#include "../src/API/QuickslotAPI.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
struct Query {std::uint32_t skseVersion,runtimeVersion,editorVersion,isEditor;};
struct Info {std::uint32_t infoVersion;const char* name;std::uint32_t version;};
using QueryFn=bool(*)(const Query*,Info*);
constexpr std::uint32_t version(unsigned a,unsigned b,unsigned c){return (a<<24)|(b<<16)|(c<<4);}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    auto dll=LoadLibraryA(argv[1]);
    if(!dll){std::fprintf(stderr,"LoadLibrary failed: %lu\n",GetLastError());return 3;}
    auto query=reinterpret_cast<QueryFn>(GetProcAddress(dll,"SKSEPlugin_Query"));
    const auto* metadata=reinterpret_cast<const std::uint32_t*>(GetProcAddress(dll,"SKSEPlugin_Version"));
    if(!query||!metadata||!GetProcAddress(dll,"SKSEPlugin_Load"))return 4;
    const auto apiVersion=reinterpret_cast<QuickslotAPI::GetAPIVersion>(GetProcAddress(dll,"Quickslot_GetAPIVersion"));
    const auto open=reinterpret_cast<QuickslotAPI::Open>(GetProcAddress(dll,"Quickslot_Open"));
    const auto close=reinterpret_cast<QuickslotAPI::Close>(GetProcAddress(dll,"Quickslot_Close"));
    const auto isOpen=reinterpret_cast<QuickslotAPI::IsMenuOpen>(GetProcAddress(dll,"Quickslot_IsMenuOpen"));
    const auto enable=reinterpret_cast<QuickslotAPI::SetHotkeyEnabled>(GetProcAddress(dll,"Quickslot_SetHotkeyEnabled"));
    if(!apiVersion||!open||!close||!isOpen||!enable||apiVersion()!=1)return 10;
    if(open()||isOpen())return 11;
    close();enable(false);if(open()||isOpen())return 12;enable(true);
    constexpr auto expectedVersion=(quickslot::release::major<<24)|(quickslot::release::minor<<16)|(quickslot::release::patch<<4)|quickslot::release::revision;
    if(metadata[1]!=expectedVersion){std::puts("FAIL: DLL version differs from release source");return 7;}
    for(auto runtime:{version(1,5,97),version(1,6,317),version(1,6,353),version(1,6,629),version(1,6,640),version(1,6,659),version(1,6,1130),version(1,6,1170),version(1,6,1179),version(1,7,99),version(1,7,104),version(1,4,15),version(1,5,80),version(1,4,14),version(1,9,32),version(2,0,0)}) {
        Query input{0,runtime,0,0};Info output{};
        const bool expected=runtime!=version(1,5,80)&&runtime!=version(1,4,14)&&runtime!=version(1,9,32)&&runtime!=version(2,0,0);
        if(query(&input,&output)!=expected||output.infoVersion!=1||std::strcmp(output.name,"Quickslot")||output.version!=metadata[1])return 5;
    }
    Query editor{0,version(1,6,1170),0,1};Info output{};if(query(&editor,&output))return 6;

    if((metadata[0x304/4]&3)!=3||(metadata[0x308/4]&1)!=1||(metadata[0x308/4]&4)!=0)return 8;
    for(unsigned i=0;i<16;++i)if(metadata[0x30c/4+i])return 9;
    FreeLibrary(dll);
    std::puts("PASS: real Quickslot.dll loads; SKSE and menu API exports/calls correct; pre-init API safe; SE/AE/VR policy and Address Library v5 metadata correct. No engine callbacks invoked.");
    return 0;
}
