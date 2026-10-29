Quickslot translations

Copy en.json to a new UTF-8 JSON file. Set language to the locale code (for example ru, de, ja, zh-cn) and name to its native display name. Translate the values under strings; keep the English keys unchanged. Missing or empty values use the built-in English text. Skyrim supplies item and spell names; players name custom groups.

Keep printf placeholders such as %s, %d and %.0f in their listed order and type. Preserve spaces at the beginning or end of a fragment. Check Quickslot.log for invalid placeholders. Translate the visible label before an ImGui ## identifier.

Install in Data/Quickslot/translations, then restart the game. Select the language in Appearance > Style > Language. Automatic follows Skyrim's sLanguage setting. Additional fonts can be installed in Data/Quickslot/fonts/fallback as .ttf or .otf files; restart after installing fonts. Theme fonts retain priority; fallbacks supply additional glyphs.

The bundled CJK fallback covers Chinese, Japanese and Korean characters. Text renders left to right using individual glyphs. The controller's on-screen keyboard uses Latin keys; hardware keyboard text uses Skyrim's character events.
