# User feedback verification

On 2026-10-25, the owner marked feedback checks #2–#6 passed after testing the revised combined development build `f2403c720cde` and approved including them in the release. Quickslot 0.11.0 includes those five changes.

| Feedback check | Change | Issue | PR | Result |
| --- | --- | --- | --- | --- |
| 1 | Smart Potions with Apothecary/CACO | [#7](https://github.com/opteron-x86/quickslot-hotkeys/issues/7) | [#13](https://github.com/opteron-x86/quickslot-hotkeys/pull/13) | Awaiting gameplay confirmation; excluded from 0.11.0 |
| 2 | Configurable keyboard/controller actions | [#8](https://github.com/opteron-x86/quickslot-hotkeys/issues/8) | [#14](https://github.com/opteron-x86/quickslot-hotkeys/pull/14) | Pass, including the simplified None/Clear panel |
| 3 | Independent QuickLoot input options | [#9](https://github.com/opteron-x86/quickslot-hotkeys/issues/9) | [#15](https://github.com/opteron-x86/quickslot-hotkeys/pull/15) | Pass |
| 4 | Poison indicators and bow ammunition | [#10](https://github.com/opteron-x86/quickslot-hotkeys/issues/10) | [#16](https://github.com/opteron-x86/quickslot-hotkeys/pull/16) | Pass after the poison appearance and equipped-ammo corrections |
| 5 | Choose a hand for the current use | [#11](https://github.com/opteron-x86/quickslot-hotkeys/issues/11) | [#17](https://github.com/opteron-x86/quickslot-hotkeys/pull/17) | Pass |
| 6 | Groups linked to all matching Favorites | [#12](https://github.com/opteron-x86/quickslot-hotkeys/issues/12) | [#18](https://github.com/opteron-x86/quickslot-hotkeys/pull/18) | Pass |

The first HUD report showed a dark rectangle around the poison droplet and a bow showing zero despite ten equipped, fireable arrows. The follow-up build removed the droplet backing and corrected the engine call used to read equipped ammunition. The owner then passed check #4.

These are feature-level gameplay confirmations. The report did not enumerate every device, QuickLoot variant or edge case. Automated results remain separate: the [combined development build](https://github.com/opteron-x86/quickslot-hotkeys/actions/runs/40241401448) passed Linux and Windows CI, and the release build verifies the final release source and packages. See the [full gameplay checklist](TESTING.md) for future regression testing.

For Smart Potions, test Apothecary and CACO separately against the pending PR #13 build. Lower the relevant resource and check restoration and regeneration rules independently; for CACO, include the configured potion duration. Record a failing potion's FormID and the chosen Smart Potion rule. The existing Alchemy Plus setup was reported to work. The unconfirmed overhaul change remains separate from this release.
