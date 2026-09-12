#pragma once

namespace texnx::paths {

inline constexpr char MinecraftTitleId[] = "01006BD001E06000";
inline constexpr char SdCardRoot[] = "sdmc:/";
inline constexpr char AtmosphereDirectory[] = "sdmc:/atmosphere";
inline constexpr char AtmosphereContentsDirectory[] =
    "sdmc:/atmosphere/contents";
inline constexpr char MinecraftContentsDirectory[] =
    "sdmc:/atmosphere/contents/01006BD001E06000";
inline constexpr char MinecraftLayeredFsRoot[] =
    "sdmc:/atmosphere/contents/01006BD001E06000/romfs";
inline constexpr char MinecraftCommon[] =
    "sdmc:/atmosphere/contents/01006BD001E06000/romfs/Common";
inline constexpr char TexNxDirectory[] = "sdmc:/switch/TexNX";
inline constexpr char TextureDirectory[] = "sdmc:/switch/TexNX/Textures";
inline constexpr char ConfigFile[] = "sdmc:/switch/TexNX/config.json";
inline constexpr char ConfigTemporaryFile[] = "sdmc:/switch/TexNX/config.json.tmp";

inline constexpr char EnglishTranslations[] = "romfs:/i18n/en-US/texnx.json";
inline constexpr char JapaneseTranslations[] = "romfs:/i18n/ja-JP/texnx.json";
inline constexpr char DefaultTextureIcon[] =
    "romfs:/assets/default/_icon.png";

} // namespace texnx::paths
