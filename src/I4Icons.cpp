#include "Game.h"
#include <RE/G/GFxLoader.h>
#include "I4Icons.h"
#include "I4Policy.h"
#include "DXContextState.h"
#include <deque>
#include <cstring>

namespace quickslot {
namespace {
template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
std::atomic<std::uint64_t> iconGeneration{0};
constexpr unsigned iconSize=256,maxTextures=128,maxForms=2048;
constexpr char processEntry[]="skse.plugins.InventoryInjector.ProcessEntry";
struct Spec {std::string source,label;std::optional<std::uint32_t> rgb;};

template<class T> RE::GPtr<T> own(T* raw) {
    RE::GPtr<T> pointer(raw);if(raw)raw->Release();return pointer;
}
double number(const RE::GFxValue& object,const char* name,double fallback=0) {
    RE::GFxValue value;return object.GetMember(name,&value)&&value.IsNumber()?value.GetNumber():fallback;
}
std::string string(const RE::GFxValue& object,const char* name) {
    RE::GFxValue value;return object.GetMember(name,&value)&&value.IsString()?value.GetString():"";
}
void keywords(RE::GFxValue& target,RE::BGSKeywordForm* form) {
    if(form)for(auto* keyword:form->GetKeywords())if(keyword&&!keyword->formEditorID.empty())
        target.SetMember(keyword->formEditorID.c_str(),true);
}
const char* defaultLabel(const Entry& entry) {
    switch(baseIcon(entry)) {
    case Icon::fire:return "magic_fire";
    case Icon::frost:return "magic_frost";
    case Icon::shock:return "magic_shock";
    case Icon::alteration:return "default_alteration";
    case Icon::conjuration:return "default_conjuration";
    case Icon::destruction:return "default_destruction";
    case Icon::illusion:return "default_illusion";
    case Icon::restoration:return "default_restoration";
    case Icon::health:return "potion_health";
    case Icon::magicka:return "potion_magic";
    case Icon::stamina:return "potion_stam";
    default:break;
    }
    switch(entry.kind) {
    case Kind::sword:return "weapon_sword";
    case Kind::greatsword:return "weapon_greatsword";
    case Kind::dagger:return "weapon_dagger";
    case Kind::axe:return entry.twoHanded?"weapon_battleaxe":"weapon_waraxe";
    case Kind::mace:return entry.twoHanded?"weapon_hammer":"weapon_mace";
    case Kind::bow:return entry.icon==Icon::crossbow?"weapon_crossbow":"weapon_bow";
    case Kind::staff:return "weapon_staff";
    case Kind::shield:return "armor_shield";
    case Kind::torch:return "misc_torch";
    case Kind::food:return "default_food";
    case Kind::poison:return "potion_poison";
    case Kind::potion:return "default_potion";
    case Kind::scroll:return "default_scroll";
    case Kind::ammo:return entry.icon==Icon::bolt?"weapon_bolt":"weapon_arrow";
    default:return "default_power";
    }
}
void skyUIData(RE::GFxValue& data,RE::GFxMovieView& movie,RE::TESForm& form,const Entry& entry) {
    data.SetMember("baseId",double(entry.form&0xffffff));
    data.SetMember("text",entry.name.c_str());data.SetMember("displayName",entry.name.c_str());
    data.SetMember("iconLabel",defaultLabel(entry));
    RE::GFxValue keywordObject;movie.CreateObject(&keywordObject);
    keywords(keywordObject,form.As<RE::BGSKeywordForm>());data.SetMember("keywords",keywordObject);
    RE::GFxValue resistance;
    if(data.GetMember("magicType",&resistance)&&resistance.IsNumber())data.SetMember("resistance",resistance);
    if(auto* magic=form.As<RE::MagicItem>()) {
        RE::GFxValue effects;movie.CreateObject(&effects);
        for(auto* effect:magic->effects)if(effect&&effect->baseEffect)keywords(effects,effect->baseEffect);
        data.SetMember("effectKeywords",effects);
    }
    if(auto* weapon=form.As<RE::TESObjectWEAP>()) {
        unsigned type=unsigned(weapon->GetWeaponType());
        if(type==6&&keywordObject.HasMember("WeapTypeWarhammer"))type=7;
        else if(type==7)type=8;
        else if(type==8)type=10;
        if(keywordObject.HasMember("WeapTypePickaxe"))type=11;
        if(keywordObject.HasMember("WeapTypeWoodAxe"))type=12;
        data.SetMember("subType",double(type));
    } else if(form.Is(RE::FormType::Ammo))data.SetMember("subType",entry.icon==Icon::bolt?1.0:0.0);
    else if(form.Is(RE::FormType::Armor)) {
        data.SetMember("subType",6.0); // SkyUI's shield category.
        data.SetMember("iconLabel",number(data,"weightClass")==0?"lightarmor_shield":"armor_shield");
        data.SetMember("iconColor",double(0xEDDA87));
    }
    else if(form.Is(RE::FormType::AlchemyItem)) {
        unsigned type=12;
        if(entry.kind==Kind::poison)type=15;
        else if(entry.kind==Kind::food) {
            const auto* potion=form.As<RE::AlchemyItem>();
            type=potion&&potion->data.consumptionSound&&potion->data.consumptionSound->GetFormID()==0x0b6435?13:14;
        }
        else switch(RE::ActorValue(int(number(data,"actorValue",-1)))) {
        case RE::ActorValue::kHealth:type=0;break;
        case RE::ActorValue::kHealRate:type=1;break;
        case RE::ActorValue::kHealRateMult:type=2;break;
        case RE::ActorValue::kMagicka:type=3;break;
        case RE::ActorValue::kMagickaRate:type=4;break;
        case RE::ActorValue::kMagickaRateMult:type=5;break;
        case RE::ActorValue::kStamina:type=6;break;
        case RE::ActorValue::kStaminaRate:type=7;break;
        case RE::ActorValue::kStaminaRateMult:type=8;break;
        case RE::ActorValue::kResistFire:type=9;break;
        case RE::ActorValue::kResistShock:type=10;break;
        case RE::ActorValue::kResistFrost:type=11;break;
        default:break;
        }
        data.SetMember("subType",double(type));
        // SkyUI's potion categories supply the defaults used by I4's matching rules.
        constexpr const char* labels[]{"potion_health","potion_magic","potion_stam","potion_fire","potion_shock","potion_frost"};
        constexpr unsigned colors[]{0xDB2E73,0x2E9FDB,0x51DB2E,0xC73636,0xEAAB00,0x1FFBFF};
        if(type<12){const unsigned index=type<9?type/3:type-6;data.SetMember("iconLabel",labels[index]);data.SetMember("iconColor",double(colors[index]));}
        else if(type==13)data.SetMember("iconLabel","food_wine");
        else if(type==15)data.SetMember("iconColor",double(0xAD00B3));
    } else if(form.Is(RE::FormType::Spell)||form.Is(RE::FormType::Scroll)) {
        switch(RE::ActorValue(int(number(data,"resistance",-1)))) {
        case RE::ActorValue::kResistFire:data.SetMember("iconColor",double(0xC73636));break;
        case RE::ActorValue::kResistShock:data.SetMember("iconColor",double(0xFFFF00));break;
        case RE::ActorValue::kResistFrost:data.SetMember("iconColor",double(0x1FFBFF));break;
        default:break;
        }
    }
}
std::optional<Spec> resolve(RE::GFxMovieView& movie,const Entry& entry) {
    auto* form=RE::TESForm::LookupByID(entry.form);if(!form)return {};
    RE::GFxValue data;movie.CreateObject(&data);
    RE::GFxValue args[]{double(entry.form),data,true,false};
    if(!movie.Invoke("skse.ExtendForm",nullptr,args,4)||!data.HasMember("formType"))return {};
    skyUIData(data,movie,*form,entry);
    if(!movie.Invoke(processEntry,nullptr,&data,1))return {};
    Spec result;const auto path=i4SourcePath(string(data,"iconSource"));if(!path)return {};
    result.source=*path;result.label=string(data,"iconLabel");
    if(result.label.empty()||result.label.size()>128||result.label.find('\0')!=std::string::npos)return {};
    RE::GFxValue rgb;
    if(data.GetMember("iconColor",&rgb)&&rgb.IsNumber()) {
        const auto color=rgb.GetNumber();if(std::isfinite(color)&&color>=0&&color<=0xffffff)result.rgb=std::uint32_t(color);
    }
    return result;
}
}
void invalidateI4Icons(){++iconGeneration;}

struct I4Icons::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    DXContextState isolation;
    struct Texture {ComPtr<ID3D11ShaderResourceView> original,mask;std::uint64_t used=0;};
    struct Form {std::optional<Spec> spec;std::uint64_t used=0;};
    std::map<std::string,Texture> textures;
    std::map<std::uint32_t,Form> forms;
    std::map<std::string,RE::GPtr<RE::GFxMovieDef>> movies;
    std::deque<Entry> pending;
    std::uint64_t generation=0,frame=0;
    bool available=false,reported=false;
    Impl(ID3D11Device* d,ID3D11DeviceContext* c):device(d),context(c),isolation(d,c){
        if(!isolation.available())SKSE::log::warn("I4 icons require D3D11 context-state support; using Quickslot icons");
    }

    static std::string key(const Spec& spec){return spec.source+'\n'+spec.label;}
    template<class Map> void trim(Map& values,std::size_t limit) {
        while(values.size()>=limit)values.erase(std::min_element(values.begin(),values.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;}));
    }
    ComPtr<ID3D11ShaderResourceView> upload(const std::vector<std::uint8_t>& pixels) {
        D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=iconSize;desc.ArraySize=desc.MipLevels=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{pixels.data(),iconSize*4,0};ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11ShaderResourceView> view;
        if(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)))device->CreateShaderResourceView(texture.Get(),nullptr,&view);
        return view;
    }
    Texture render(const Spec& spec) {
        Texture result;result.used=frame;
        auto* manager=RE::BSScaleformManager::GetSingleton();if(!manager||!manager->loader)return result;
        if(!movies.contains(spec.source)) {
            if(movies.size()>=32)movies.erase(movies.begin());
            const auto flags=RE::GFxLoader::LoadConstants(RE::GFxLoader::kLoadWaitCompletion|RE::GFxLoader::kLoadKeepBindData|RE::GFxLoader::kLoadQuietOpen);
            movies[spec.source]=own(manager->loader->CreateMovie(spec.source.c_str(),flags));
        }
        const auto& definition=movies.at(spec.source);if(!definition)return result;
        auto movie=own(definition->CreateInstance());if(!movie)return result;
        movie->SetBackgroundAlpha(0);movie->SetViewScaleMode(RE::GFxMovieView::ScaleModeType::kNoScale);
        movie->SetViewAlignment(RE::GFxMovieView::AlignType::kTopLeft);
        movie->SetViewport(iconSize,iconSize,0,0,iconSize,iconSize,RE::GViewport::kRenderTextureAlpha);
        if(!movie->GotoLabeledFrame(spec.label.c_str()))return result;
        movie->SetPlayState(RE::GFxMovie::PlayState::kStopped);
        RE::GFxValue root,bounds;
        if(!movie->GetVariable(&root,"_root")||!root.IsDisplayObject())return result;
        movie->Advance(0);
        root.SetMember("_x",0.0);root.SetMember("_y",0.0);root.SetMember("_xscale",100.0);root.SetMember("_yscale",100.0);
        if(!root.Invoke("getBounds",&bounds,&root,1)||!bounds.IsObject())return result;
        const auto fit=fitI4Icon(number(bounds,"xMin"),number(bounds,"yMin"),number(bounds,"xMax"),number(bounds,"yMax"),iconSize);
        if(!fit)return result;
        RE::GFxValue::DisplayInfo display;display.SetPosition(fit->x,fit->y);display.SetScale(fit->scale*100,fit->scale*100);display.SetAlpha(100);display.SetVisible(true);
        if(!root.SetDisplayInfo(display))return result;

        D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=iconSize;desc.ArraySize=desc.MipLevels=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target,staging;ComPtr<ID3D11RenderTargetView> view;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,&target))||FAILED(device->CreateRenderTargetView(target.Get(),nullptr,&view)))return result;
        desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging)))return result;
        {
            DXContextState::Frame saved(isolation);
            ID3D11RenderTargetView* targets[]{view.Get()};context->OMSetRenderTargets(1,targets,nullptr);
            const D3D11_VIEWPORT viewport{0,0,float(iconSize),float(iconSize),0,1};context->RSSetViewports(1,&viewport);
            const float clear[]{0,0,0,0};context->ClearRenderTargetView(view.Get(),clear);
            movie->Display();
            context->CopyResource(staging.Get(),target.Get());
        }
        std::vector<std::uint8_t> pixels(iconSize*iconSize*4);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return result;
        for(unsigned y=0;y<iconSize;++y)std::memcpy(pixels.data()+y*iconSize*4,static_cast<const std::uint8_t*>(mapped.pData)+y*mapped.RowPitch,iconSize*4);
        context->Unmap(staging.Get(),0);
        if(!straightenI4Pixels(pixels))return result;
        result.original=upload(pixels);
        for(std::size_t i=0;i<pixels.size();i+=4)pixels[i]=pixels[i+1]=pixels[i+2]=255;
        result.mask=upload(pixels);return result;
    }
    void beginFrame() {
        ++frame;
        if(generation!=iconGeneration.load()) {generation=iconGeneration.load();forms.clear();textures.clear();movies.clear();pending.clear();}
        available=false;if(!isolation.available())return;
        auto* ui=RE::UI::GetSingleton();if(!ui)return;
        const auto hud=ui->GetMenu<RE::HUDMenu>();if(!hud||!hud->uiMovie)return;
        auto& movie=*hud->uiMovie;
        available=movie.IsAvailable(processEntry)&&movie.IsAvailable("skse.ExtendForm");
        if(!reported){SKSE::log::info("I4 icon provider {}",available?"ready":"unavailable; using Quickslot icons");reported=true;}
        if(!available){pending.clear();return;}
        // Bound work per frame; only requested, visible icons enter the queue.
        unsigned rendered=0;
        for(unsigned work=0;work<8&&!pending.empty()&&rendered<1;++work) {
            auto entry=std::move(pending.front());pending.pop_front();
            auto found=forms.find(entry.form);
            if(found==forms.end()) {
                trim(forms,maxForms);found=forms.emplace(entry.form,Form{resolve(movie,entry),frame}).first;
            }
            if(!found->second.spec)continue;
            const auto& spec=*found->second.spec;const auto id=key(spec);
            if(textures.contains(id))continue;
            trim(textures,maxTextures);auto texture=render(spec);++rendered;
            if(!texture.original||!texture.mask)SKSE::log::warn("I4 icon unavailable: {} [{}]",spec.source,spec.label);
            textures.emplace(id,std::move(texture));
        }
    }
    std::optional<ItemIcon> find(const Entry& entry) {
        if(!available||entry.unresolved||!entry.form||isReusable(entry))return {};
        if(auto form=forms.find(entry.form);form!=forms.end()) {
            form->second.used=frame;
            if(!form->second.spec)return {};
            if(auto texture=textures.find(key(*form->second.spec));texture!=textures.end()) {
                texture->second.used=frame;
                if(!texture->second.original||!texture->second.mask)return {};
                return ItemIcon{reinterpret_cast<ImTextureID>(texture->second.original.Get()),reinterpret_cast<ImTextureID>(texture->second.mask.Get()),form->second.spec->rgb};
            }
        }
        if(pending.size()<128&&std::none_of(pending.begin(),pending.end(),[&](const auto& queued){return queued.form==entry.form;}))pending.push_back(entry);
        return {};
    }
};
I4Icons::I4Icons(ID3D11Device* d,ID3D11DeviceContext* c):impl_(std::make_unique<Impl>(d,c)){}
I4Icons::~I4Icons()=default;
void I4Icons::beginFrame(){impl_->beginFrame();}
std::optional<ItemIcon> I4Icons::find(const Entry& entry){return impl_->find(entry);}
}
