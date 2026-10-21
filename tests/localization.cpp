#include "EditorModel.h"
#include "Presets.h"
#include <iostream>
#include <chrono>
#include <source_location>
using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* why,std::source_location at=std::source_location::current()){if(!value)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));++checks;}
int main(int argc,char** argv)try {
    for(const auto text:{"Espada élfica","Стальной меч","火球術","治癒の手","치유","Δράκος","🗡"}) {
        check(validUTF8(text)&&cleanName(text)==text,"Localized names preserve UTF-8");
        Group group;Entry e;e.name=text;e.form=1;group.add(e);
        check(decode(encode(group),[](auto id,auto& mapped){mapped=id;return true;}).entries[0].name==text,"Unicode survives save/load");
        check(gameTextUTF8(text,1251)==text,"Already valid UTF-8 is never code-page decoded");
    }
    for(const auto& [name,query]:std::vector<std::pair<std::string,std::string>>{{"СТАЛЬНОЙ МЕЧ","стальной"},{"ÉPÉE","épée"},{"Straße","STRASSE"},{"Épée","E\xcc\x81pée"},{"ΔΡΆΚΟΣ","δράκος"},{"火球術","火球"},{"치유의 손","치유"}})
        check(textContains(name,query),"Unicode search handles case folding and composed accents");
    check(!validUTF8(std::string("\xc0\xaf",2))&&!validUTF8(std::string("A\0B",3)),"Invalid UTF-8 and embedded NUL are rejected");
    check(!textContains("火球術","Ice"),"Unrelated text does not match");
    std::string edited="A治癒🗡";eraseLastCharacter(edited);check(edited=="A治癒","Backspace removes a full supplementary character");
    eraseLastCharacter(edited);check(edited=="A治","Backspace removes a full multibyte character");
    check(languageCode("RUSSIAN")=="ru"&&languageCode("tchinese")=="zh-tw"&&languageCode("japanese")=="ja","Skyrim language names map to packs");
    check(languageCodepage("ru")==1251&&languageCodepage("ja")==932&&languageCodepage("zh-cn")==936,"Legacy decoding follows game language");
#ifdef _WIN32
    check(gameTextUTF8(std::string("\xcc\xe5\xf7",3),1251)=="Меч","Legacy Russian game text converts to UTF-8");
    check(gameTextUTF8(std::string("\x8c\x95",2),932)=="剣","Legacy Japanese game text converts to UTF-8");
#endif
    const auto folder=std::filesystem::temp_directory_path()/("quickslot-languages-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(folder);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{folder};
    const auto pack=[&](const char* file,const Json& value){std::ofstream(folder/file,std::ios::binary)<<value.dump(2);};
    pack("ru.json",{{"language","ru"},{"name","Русский"},{"strings",{{"Groups","Группы"},{"Close","Закрыть"},{"%.0f damage","Урон: %.0f"},{"Unsafe %s","%n"},{"Missing",""},{"Injected","bad##control"}}}});
    pack("broken.json",{{"language","../bad"},{"name","Bad"},{"strings",Json::object()}});
    Localization language;language.load(folder,"RUSSIAN");language.select("auto");
    check(std::string(language.text("Groups"))=="Группы","Automatic uses game language");
    check(std::string(language.text("Missing"))=="Missing"&&std::string(language.text("Unknown"))=="Unknown","Incomplete packs fall back to English");
    check(std::string(language.text("Unsafe %s"))=="Unsafe %s"&&std::string(language.text("Injected"))=="Injected","Unsafe format strings and ImGui IDs cannot enter the UI");
    check(std::string(language.text("%.0f damage"))=="Урон: %.0f","Compatible number formatting can be translated");
    check(language.warnings().size()==4,"Invalid translations and files produce diagnostics");
    const char* retained=language.text("Close");language.select("en");check(std::string(retained)=="Закрыть"&&std::string(language.text("Close"))=="Close","Live language switches keep previously returned text alive");
    language.select("ru-ru");check(std::string(language.text("Groups"))=="Группы","Regional locale falls back to base language");
    language.select("unknown");check(std::string(language.text("Groups"))=="Groups","Unavailable locale falls back to English");
    Options o;o.language="ru";check(decodeOptions(encodeOptions(o)).language=="ru","Language setting survives settings serialization");
    Options imported;imported.language="ja";importAppearance(o,imported);check(o.language=="ru","Appearance imports keep the user's language");
    for(const auto name:{"Воин","魔法使い","치유","Équipement"})check(validPresetName(name),"Preset names accept Unicode");
    for(const auto name:{"../Воин","CON","NUL","Bad/name","Bad\\name"})check(!validPresetName(name),"Unicode preset names retain path and device-name validation");
    const auto path=presetPath(folder,"魔法使い");std::ofstream(path)<<"{}";check(listPresets(folder)==std::vector<std::string>{"broken","ru","魔法使い"},"Unicode preset filenames round trip");
    if(argc==2){Localization english;english.load(std::filesystem::path(argv[1])/"assets/translations");check(english.warnings().empty(),"Shipped translation template is valid");check(english.packs().size()==1&&english.packs()[0]->strings.size()>350,"English reference includes the full editor");}
    std::cout<<"PASS: "<<checks<<" language checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
