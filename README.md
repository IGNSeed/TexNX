# TexNX

TexNX は、Atmosphere CFW を導入した Nintendo Switch 上で動作する、Minecraft: Nintendo Switch Edition 向けの非公式 Homebrew アプリケーションです。対象 Title ID は `01006BD001E06000` です。

現在のバージョンは `0.2.0` です。Borealis による controller／touch 対応 GUI、英語／日本語表示、言語設定の保存、将来のテクスチャ一覧用画面を備えています。この版はテクスチャファイルを走査・コピー・削除・置換・バックアップ・復元・展開・ダウンロードしません。

## v0.2.0 の機能

- `Textures`、`Settings`、`About` を独立した画面として持つ Home 画面
- Handheld／Docked の表示サイズに追従する Borealis flex layout
- controller と touch による標準的な UI 操作
- `A` で決定、サブ画面では `B` または画面内の戻る Button で Home へ戻る
- どの画面からでも `PLUS` で安全に終了
- 固定された black／dark gray／gray／white の配色
- `System`（既定）、`English`、`日本語` の言語選択
- `sdmc:/switch/TexNX/config.json` への言語設定だけの保存
- config が存在しない、空、壊れている、未知の値を含む場合は `System` へ安全に fallback
- 日本語表示には Borealis の libnx backend が読み込む Switch shared system font を使用
- `sdmc:/atmosphere/contents/01006BD001E06000/romfs/Common` を読み取り専用で確認
- Common が存在しない場合または確認 error の場合だけ Dialog を表示し、GUI 自体は利用可能なまま維持

`Textures` は今後の一覧を受け入れるための placeholder です。v0.2.0 にテクスチャ適用処理や backup／restore 機能はありません。

## 開発環境

必要なもの:

- devkitPro
- devkitA64
- libnx 4.10.0 以上
- deko3d と uam
- switch-glm
- switch-cmake
- CMake 3.20 以上

GUI dependency の [XITRIX/borealis](https://github.com/XITRIX/borealis) は `external/borealis` の Git submodule として revision を固定しています。Borealis が必要とするため TexNX は C++20 を使用します。Switch backend は deko3d を利用し、未使用の GLFW／SDL submodule は初期化しません。

clone 後は submodule を初期化してください。

```sh
git submodule update --init external/borealis
```

Switch 向け build には必ず devkitA64 toolchain を使用してください。MSVC、通常の Windows MinGW、x86/x64 compiler は対象外です。

Windows では devkitPro MSYS2 shell から次のように build できます。

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITA64="$DEVKITPRO/devkitA64"

cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake"
cmake --build build --parallel
```

生成物は `build/TexNX.nro` です。必要な shader、font、i18n resource は NRO 内の RomFS に収録されるため、実機への配布物はこの NRO 1 ファイルだけです。NACP の app name は `TexNX`、version は CMake project version と同じ `0.2.0`、author は `IGNSeed` です。専用の権利クリアな artwork がないため custom icon は同梱していません。

## 配置と操作

生成した `TexNX.nro` は次の推奨場所へ配置します。Homebrew Menu が走査する `sdmc:/switch/` 直下へ配置しても起動できます。

```text
sdmc:/switch/TexNX/TexNX.nro
```

Homebrew Menu から起動し、controller または touch で操作します。初回の言語は Switch 本体設定を参照し、日本語と英語以外の本体言語は英語表示になります。言語は Settings から起動中に即時変更でき、次回起動用として `config.json` に保存されます。

Common が見つかった場合は通常 UI に status を追加しません。見つからない場合や読み取り error の場合は Dialog で通知しますが、TexNX は終了せず各画面を利用できます。

## Runtime のファイル操作

v0.2.0 が書き込む可能性があるのは、言語を選択したときの次の場所だけです。

```text
sdmc:/switch/TexNX/
sdmc:/switch/TexNX/config.json
```

安全な置換のため同じ directory に一時的な `config.json.tmp` を作成し、完了後に置き換えます。Atmosphere、LayeredFS、Minecraft の `Common` 以下には一切書き込みません。

## Dependency とライセンス

TexNX のソースコードは [MIT License](LICENSE) で提供されます。Borealis は Apache License 2.0 で提供され、revision は Git submodule pointer により固定されます。Borealis の Material Icons font とその license は GUI の実行に必要な resource として NRO の RomFS に収録されます。Nintendo／Minecraft から抽出した font や asset は使用・収録しません。

TexNX は Nintendo、Mojang Studios、Microsoft の非公式プロジェクトであり、これらの企業による承認・提携・支援を受けていません。リポジトリにはゲームから抽出したデータ、Minecraft/Nintendo の assets、ROM、keys、Atmosphere の配布バイナリを含めません。
