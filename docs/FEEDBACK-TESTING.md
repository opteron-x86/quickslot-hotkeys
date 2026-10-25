# User feedback integration build

This development branch combines the six feedback PRs for gameplay testing. The individual PRs remain separate so confirmed fixes can ship before larger features. It does not change the latest stable release, v0.10.3.

| Change | Issue | PR |
| --- | --- | --- |
| Smart Potions with Apothecary/CACO | [#7](https://github.com/opteron-x86/quickslot-hotkeys/issues/7) | [#13](https://github.com/opteron-x86/quickslot-hotkeys/pull/13) |
| Configurable keyboard/controller actions | [#8](https://github.com/opteron-x86/quickslot-hotkeys/issues/8) | [#14](https://github.com/opteron-x86/quickslot-hotkeys/pull/14) |
| Independent QuickLoot input options | [#9](https://github.com/opteron-x86/quickslot-hotkeys/issues/9) | [#15](https://github.com/opteron-x86/quickslot-hotkeys/pull/15) |
| Poison indicators and bow ammunition | [#10](https://github.com/opteron-x86/quickslot-hotkeys/issues/10) | [#16](https://github.com/opteron-x86/quickslot-hotkeys/pull/16) |
| Choose a hand for the current use | [#11](https://github.com/opteron-x86/quickslot-hotkeys/issues/11) | [#17](https://github.com/opteron-x86/quickslot-hotkeys/pull/17) |
| Groups linked to all matching Favorites | [#12](https://github.com/opteron-x86/quickslot-hotkeys/issues/12) | [#18](https://github.com/opteron-x86/quickslot-hotkeys/pull/18) |

Install the runtime `Quickslot-0.10.3-dev.<commit>.zip` through the mod manager, using one Quickslot package at a time. The source, symbols and API archives are separate developer downloads. Keep existing settings and presets. Use a separate test profile/save when comparing this build with v0.10.3, particularly for new Favorites settings that older versions do not understand.

## First checks

1. Test Smart Potions with Apothecary, CACO, and the known-working Alchemy Plus setup separately. Lower the relevant resource, add a restoration shortcut, and confirm it finds and drinks a matching potion. Check regeneration separately. For CACO, test the selected duration setting. Include a potion FormID and the chosen Smart Potion rule with any failure.
2. In **Controls → Keyboard → Remap actions**, move or disable the remove modifier and enable **Allow group selection with unused modifiers**. Hold Alt-to-walk while selecting a group. On controller, use X as a group modifier and remap unequip under **Browsing actions**.
3. In **Controls → General**, enable keyboard/mouse input with QuickLoot while leaving controller input disabled. Confirm the keyboard works and D-pad navigation stays with QuickLoot. Repeat with the opposite policy and interrupt a held or queued action by opening another menu.
4. Poison one of two identical weapons. Only that copy should show the droplet. Fire arrows and bolts, switch ammunition, and check the count on bows/crossbows. Test the indicators together with Smart Weapons and Favorites groups.
5. Enable the mouse or bumper hand preset. Hold a group, highlight a compatible item, then choose a hand. It should equip once, retain the saved default hand, and ignore the later group-key release. Repeat with a generated Favorites item beyond position 16 and while QuickLoot is open under each device policy.
6. Change a group's source from **Manual** to a Favorites category. Favorite/unfavorite items in Skyrim, test more than 16 matches, exclude/reset an entry, then return to Manual. The original contents should return. Check save/load, stock depletion/restocking, enchanted copies and Undo. An indistinguishable weapon stack may ask you to equip one copy first.

See [the complete gameplay checklist](TESTING.md) for interruption, identity, controller and VR checks. Automated results and actual in-game observations should be reported separately; record the package commit and Skyrim/SKSE versions with any report.

## Publishing order

After gameplay confirmation, ship the Smart Potion fix as a patch release. Follow with an input/HUD beta containing #14–#17; #17 depends on #14 and must be retargeted after it merges. Keep Favorites as a later feature release so it does not delay the other improvements. Use passing checks on each final merge result before tagging a release.
