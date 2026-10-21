Quickslot translations

Copy en.json to a new UTF-8 JSON file. Set language to the locale code (for example ru, de, ja, zh-cn) and name to its native display name. Translate the values under strings; keep the English keys unchanged. Missing or empty values use the built-in English text. Names of items, spells and custom groups are not translated by these files.

Keep printf placeholders such as %s, %d and %.0f in their original order and type. Preserve spaces at the beginning or end of a fragment. Invalid placeholders are ignored and logged. Do not put ImGui ## identifiers in translations.

Install in Data/Quickslot/translations, then restart the game. Select the language in Appearance > Style > Language. Automatic follows Skyrim's sLanguage setting. Additional fonts can be installed in Data/Quickslot/fonts/fallback as .ttf or .otf files; restart after installing fonts. Theme fonts retain priority; fallbacks supply missing glyphs only.

The bundled CJK fallback covers Chinese, Japanese and Korean characters. The renderer does not provide bidirectional layout or contextual shaping for scripts that require those features. The controller's on-screen keyboard remains a Latin keyboard; hardware keyboard text uses Skyrim's character events.
