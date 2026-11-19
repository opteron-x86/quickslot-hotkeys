#pragma once
#include "CastModel.h"
#include <RE/Skyrim.h>
#include <optional>
#include <string>

namespace quickslot {
// Casts spells and scrolls without putting them in a hand. The cast plays in the shout behavior: "ShoutStart" raises
// the hands, and Open Animation Replacer swaps the shout clips for the game's casting clips while the
// Quickslot_Casting condition matches the running cast. The casting animation pack installs those clips under
// meshes\actors\character\OpenAnimationReplacer\Quickslot. Main thread only; OAR reads the condition on its own threads.
class SpellCasting {
public:
    static SpellCasting& get();
    // Registers the condition with Open Animation Replacer. Call on kPostLoad.
    void registerCondition();
    // Looks for the casting animation pack. Call on kDataLoaded, after registerCondition.
    void findAnimations();
    // Open Animation Replacer took the condition and the casting animation pack is installed.
    bool installed()const{return registered_&&animations_;}
    bool conditionRegistered()const{return registered_;}
    bool animationsFound()const{return animations_;}

    enum class Result {started,waiting,refused};
    struct Outcome {Result result=Result::refused;std::string notice;};
    // Starts a cast, or keeps it until the previous cast's release has finished. held: the key that keeps a
    // concentration spell going is down (Cast ready spell), or a quick-cast tap turned it on.
    Outcome cast(RE::MagicItem* item,RE::ScrollItem* scroll,bool leftHand,bool held);
    // The Cast key was let go, or a quick-cast tap turned a concentration spell off.
    void letGo();
    // A cast is charging or a concentration spell is running.
    bool busy()const{return sequence_.active();}
    bool channeling()const{return sequence_.channeling();}
    // Runs every frame with the game's frame time. Returns a notice when a running cast had to end.
    std::string update(float seconds);
    // Ends everything at once, as on loading, death or a menu that takes over.
    void reset();
    struct Status {std::uint32_t form=0;float progress=0;bool channeling=false;};
    std::optional<Status> status()const;
private:
    struct Pending {RE::MagicItem* item=nullptr;RE::ScrollItem* scroll=nullptr;bool left=false,held=false;float age=0;};
    bool registered_=false,animations_=false;
    CastSequence sequence_;
    CastLinger linger_;
    CastAnimation animation_=CastAnimation::none;
    CastSide side_=CastSide::right;
    RE::MagicItem* item_=nullptr;
    RE::ScrollItem* scroll_=nullptr;
    float cost_=0,recovery_=0,sinceDraw_=10;
    // Seconds since the release clip started, below 0 while none plays.
    float sinceRelease_=-1;
    std::optional<Pending> pending_;
    std::optional<std::int32_t> savedLocomotion_;
    RE::BSSoundHandle chargeSound_,loopSound_;
    std::optional<Outcome> begin(RE::MagicItem* item,RE::ScrollItem* scroll,bool leftHand,bool held);
    bool fire(std::string& notice);
    void stopAnimation();
    void finish(bool stopped);
    void stopSounds();
};
}
