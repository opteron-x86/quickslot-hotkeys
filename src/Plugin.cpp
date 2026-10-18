#include "Game.h"
#include "Version.h"
#include "RuntimePolicy.h"
#include "VROverlay.h"

extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version=[]()noexcept {
    SKSE::PluginVersionData data{};
    data.PluginName("Quickslot");data.AuthorName("Quickslot");data.PluginVersion(REL::Version(quickslot::release::major,quickslot::release::minor,quickslot::release::patch,quickslot::release::revision));
    data.UsesAddressLibrary();data.UsesNoStructs();
    return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface* skse,SKSE::PluginInfo* info) {
    info->infoVersion=SKSE::PluginInfo::kVersion;info->name="Quickslot";info->version=SKSEPlugin_Version.pluginVersion;
    return !skse->IsEditor()&&quickslot::runtimeVersion(skse->RuntimeVersion()).supported();
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if(skse->IsEditor()||!quickslot::runtimeVersion(skse->RuntimeVersion()).supported())return false;
    SKSE::InitInfo init;init.logLevel=spdlog::level::info;init.trampoline=true;init.trampolineSize=128;
    SKSE::Init(skse,init);
    auto* serial=SKSE::GetSerializationInterface();if(!serial)return false;
    serial->SetUniqueID(0x51534c54);
    serial->SetSaveCallback([](auto* s){quickslot::Game::get().save(s);});
    serial->SetLoadCallback([](auto* s){quickslot::Game::get().load(s);});
    serial->SetRevertCallback([](auto*){quickslot::Game::get().reset(true);});
    const bool listening=SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if(!message)return;
        try {
            auto& game=quickslot::Game::get();
            switch(message->type) {
            case SKSE::MessagingInterface::kPostPostLoad:quickslot::initializeVROverlay();break;
            case SKSE::MessagingInterface::kDataLoaded:if(!game.initialize())SKSE::log::error("Quickslot initialization failed");break;
            case SKSE::MessagingInterface::kPreLoadGame:game.reset(false);break;
            case SKSE::MessagingInterface::kPostLoadGame:if(message->data)game.loaded();break;
            case SKSE::MessagingInterface::kNewGame:game.loaded();break;
            default:break;
            }
        }catch(const std::exception& e){SKSE::log::error("Quickslot lifecycle error: {}",e.what());}
    });
    SKSE::log::info("Quickslot {} loaded for Skyrim {}",quickslot::release::version,skse->RuntimeVersion().string());
    return listening;
}
