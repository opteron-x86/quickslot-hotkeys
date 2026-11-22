#pragma once
#include "Model.h"

namespace quickslot {
// An outfit's parts are its worn pieces, in any order, followed by nothing, by one two-handed item or by a
// right-hand and a left-hand entry. Hand entries are whatever a combination allows, plus a two-handed item.
inline constexpr std::size_t outfitPieceLimit=24;
inline bool outfitPiece(const Entry& e){return e.kind==Kind::armor;}
inline std::vector<Entry> outfitPieces(const Entry& outfit) {
    std::vector<Entry> pieces;for(const auto& part:outfit.parts)if(outfitPiece(part))pieces.push_back(part);return pieces;
}
inline std::vector<Entry> outfitHands(const Entry& outfit) {
    std::vector<Entry> hands;for(const auto& part:outfit.parts)if(!outfitPiece(part))hands.push_back(part);return hands;
}
inline bool samePiece(const Entry& a,const Entry& b){return a.form==b.form&&a.owner==b.owner&&a.unique==b.unique;}
inline bool holdsPiece(const std::vector<Entry>& pieces,const Entry& piece) {
    return std::any_of(pieces.begin(),pieces.end(),[&](const Entry& held){return samePiece(held,piece);});
}
inline bool validArmorEntry(const Entry& e) {
    return e.kind==Kind::armor&&e.hand==Hand::right&&!e.spell&&!e.twoHanded&&!e.recipe&&!e.shortcut&&e.parts.empty()&&e.exclusions.empty()&&
        unsigned(armorPart(e))<unsigned(ArmorPart::count)&&unsigned(armorWeight(e))<unsigned(ArmorWeight::count);
}
// What both hands hold: nothing recorded, one two-handed item, or a right and a left entry.
inline bool validHeldEntries(const std::vector<Entry>& hands) {
    if(hands.empty())return true;
    const auto held=[](const Entry& e){return !isHandAction(e)&&!isSmartEquipment(e)&&!isConsumable(e)&&!isVoice(e)&&!wornGear(e)&&e.kind!=Kind::combo&&e.kind!=Kind::ammo&&!e.shortcut;};
    if(hands.size()==1)return held(hands[0])&&hands[0].twoHanded&&hands[0].hand==Hand::right;
    if(hands.size()!=2)return false;
    for(unsigned hand=0;hand<2;++hand) {
        const auto& e=hands[hand];
        if(e.hand!=Hand(hand)||e.shortcut||(e.kind==Kind::emptyHand?!validHandAction(e):!held(e)||e.twoHanded))return false;
    }
    if(hands[0].kind==Kind::shield||hands[0].kind==Kind::torch)return false;
    return hands[0].unresolved||hands[1].unresolved||validPair(hands[0],hands[1]);
}
inline bool validOutfit(const Entry& e) {
    if(e.kind!=Kind::outfit||e.form||e.owner||e.unique||e.spell||e.twoHanded||e.hand!=Hand::right||e.recipe||e.unresolved||!e.exclusions.empty())return false;
    const auto pieces=outfitPieces(e),hands=outfitHands(e);
    if(pieces.size()+hands.size()==0||pieces.size()>outfitPieceLimit||!validHeldEntries(hands))return false;
    // Pieces come first, and no piece appears twice.
    for(std::size_t i=0;i<e.parts.size();++i)if(outfitPiece(e.parts[i])!=(i<pieces.size()))return false;
    for(std::size_t i=0;i<pieces.size();++i) {
        if(!validArmorEntry(pieces[i]))return false;
        if(!pieces[i].unresolved)for(std::size_t j=0;j<i;++j)if(!pieces[j].unresolved&&samePiece(pieces[i],pieces[j]))return false;
    }
    return true;
}
// The hands of an outfit as the combination that equips them. Empty for a two-handed item or no hands.
inline std::optional<Entry> handCombination(const std::vector<Entry>& hands,const std::string& name) {
    if(hands.size()!=2)return {};
    Entry combo;combo.kind=Kind::combo;combo.name=name;combo.parts=hands;return combo;
}
// The piece an outfit shows as its icon: its body piece, or its first piece or hand.
inline const Entry* outfitFace(const Entry& outfit) {
    for(const auto& part:outfit.parts)if(outfitPiece(part)&&armorPart(part)==ArmorPart::body)return &part;
    for(const auto& part:outfit.parts)if(!isHandAction(part))return &part;
    return nullptr;
}
inline std::string outfitSummary(const Entry& outfit) {
    const auto pieces=outfitPieces(outfit).size();const bool hands=!outfitHands(outfit).empty();
    auto text=std::to_string(pieces)+(pieces==1?tr(" piece"):tr(" pieces"));
    if(hands)text+=tr(" + hands");
    return text;
}

// Which worn pieces come off and which go on. `worn` lists the pieces worn now.
struct GearChange {std::vector<Entry> remove,add;};
// Putting an outfit on: its pieces go on, and an exclusive outfit takes off every other worn piece.
inline GearChange wearChange(const Entry& outfit,const std::vector<Entry>& worn) {
    GearChange change;const auto pieces=outfitPieces(outfit);
    if(outfit.exclusive)for(const auto& piece:worn)if(!holdsPiece(pieces,piece))change.remove.push_back(piece);
    for(const auto& piece:pieces)if(!holdsPiece(worn,piece))change.add.push_back(piece);
    return change;
}
// Going back: the outfit's pieces that were not worn before come off, and the earlier pieces go back on.
inline GearChange restoreChange(const Entry& outfit,const WornBefore& before,const std::vector<Entry>& worn) {
    GearChange change;
    for(const auto& piece:outfitPieces(outfit))if(holdsPiece(worn,piece)&&!holdsPiece(before.pieces,piece))change.remove.push_back(piece);
    for(const auto& piece:before.pieces)if(!holdsPiece(worn,piece))change.add.push_back(piece);
    return change;
}
// Taking an outfit off: its worn pieces come off.
inline GearChange removeChange(const Entry& outfit,const std::vector<Entry>& worn) {
    GearChange change;
    for(const auto& piece:outfitPieces(outfit))if(holdsPiece(worn,piece))change.remove.push_back(piece);
    return change;
}
// A tap on a worn outfit puts the earlier gear back when that gear was recorded for this outfit.
inline bool returnsToEarlierGear(const std::optional<WornBefore>& before,const Entry& outfit) {
    return before&&outfit.shortcut&&before->outfit==outfit.shortcut;
}
// Records the gear worn before an outfit goes on. A second try at an outfit that did not go on completely keeps
// the record of the first.
inline void rememberWornBefore(std::optional<WornBefore>& before,const Entry& outfit,std::vector<Entry> pieces,std::vector<Entry> hands) {
    if(before&&before->outfit==outfit.shortcut&&!before->applied)return;
    before=WornBefore{outfit.shortcut,std::move(pieces),std::move(hands),false};
}
}
