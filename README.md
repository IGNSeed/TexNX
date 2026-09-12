# TexNX

TexNX は、Atmosphere CFW を導入した Nintendo Switch 上で動作する、Minecraft: Nintendo Switch Edition 向けの非公式 Homebrew アプリケーションです。対象 Title ID は `01006BD001E06000` です。

現在のバージョンは `0.4.0` です。Borealis による controller／touch 対応 GUI、英語／日本語表示、言語設定の保存、SDカード上のテクスチャパック一覧表示とMinecraft LayeredFSへの適用を備えています。

## v0.4.0 の機能

- `Textures`、`Settings`、`About` を独立した画面として持つ Home 画面
- Handheld／Docked の表示サイズに追従する Borealis flex layout
- controller と touch による標準的な UI 操作
- `A` で決定、サブ画面では `B` または画面内の戻る Button で Home へ戻る
- 通常時はどの画面からでも `PLUS` で安全に終了し、適用処理中だけは不完全なCommonを残さないため終了入力を無効化
- 固定された black／dark gray／gray／white の配色
- `System`（既定）、`English`、`日本語` の言語選択
- `sdmc:/switch/TexNX/config.json` への言語設定だけの保存
- config が存在しない、空、壊れている、未知の値を含む場合は `System` へ安全に fallback
- 日本語表示には Borealis の libnx backend が読み込む Switch shared system font を使用
- `sdmc:/switch/TexNX/Textures/<Pack>/Common` 形式のテクスチャパックを画面を開くたびに一覧化
- directory名、`Common/res/description.txt` の先頭行、`Common/res/gui/pack_icon.png` を表示
- iconがない、壊れている、対応外の場合はNRO内蔵のdefault iconへfallback
- 常に先頭へ`Default` entryを表示し、packまたはDefaultの選択時に安全側の確認Dialogを表示
- 適用元Commonを全走査してから既存Minecraft Commonを完全削除し、128 KiB bufferで元packを変更せず再帰copy
- file byte数に基づく進捗Dialogと、copy後のSHA-256 fingerprint検証
- `Default`の選択ではLayeredFS Commonを完全削除し、ゲーム内蔵テクスチャへ戻す
- 実際のMinecraft Commonと各packを照合し、`Current`と選択中の`✓`を更新
- Known packと一致しないCommonは`External / Unknown`と表示
- TexNX外でCommonを手動配置した場合も、内容がKnown packと完全一致すればそのpackとして検出

fingerprintにはCommonをrootとした実際のrelative path、directory、file size、file contentを含め、bytewise sortで列挙順に依存しない結果を作ります。内容が同一のpackが複数ある場合は一覧sort順で最初のpackを表示します。configはCurrent判定に使用しません。

TexNXは現在のCommonの永続backup、backupからのrestore、rollback archiveを作成しません。`Default`操作は保存済みbackupの復元ではなく、LayeredFS Commonを削除するだけです。theme切り替え機能もありません。

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

生成物は `build/TexNX.nro` です。必要な shader、font、i18n resource、default texture iconはNRO内のRomFSに収録されるため、実機への配布物はこのNRO 1ファイルだけです。NACPのapp nameは`TexNX`、versionはCMake project versionと同じ`0.4.0`、authorは`IGNSeed`です。専用の権利クリアなapplication artworkがないためcustom application iconは同梱していません。

## 配置と操作

生成した `TexNX.nro` は次の推奨場所へ配置します。Homebrew Menu が走査する `sdmc:/switch/` 直下へ配置しても起動できます。

```text
sdmc:/switch/TexNX/TexNX.nro
```

Homebrew Menu から起動し、controller または touch で操作します。初回の言語は Switch 本体設定を参照し、日本語と英語以外の本体言語は英語表示になります。言語は Settings から起動中に即時変更でき、次回起動用として `config.json` に保存されます。

テクスチャパックは次の構造で配置します。`Common`がないdirectoryは一覧に表示されません。

```text
sdmc:/switch/TexNX/Textures/<Pack>/Common/
├── res/gui/pack_icon.png       # 任意
└── res/description.txt         # 任意、先頭行のみ使用
```

Common が見つかった場合は通常 UI に status を追加しません。見つからない場合や読み取り error の場合は Dialog で通知しますが、TexNX は終了せず各画面を利用できます。

Textures画面でpackを選択すると、確認後に次の順序で適用します。

1. 適用元packのCommonを完全に事前検証
2. 既存Minecraft Commonを完全削除
3. 新しいCommon directoryを作成
4. 選択packのCommonをstreaming copy
5. sourceとdestinationのfingerprint一致を検証
6. `Current`と`✓`を実際のCommonから再判定

事前検証に失敗した場合は既存Commonを変更しません。削除に失敗した場合は新しいcopyを開始しません。削除開始後のcopyや検証に失敗した場合は、不完全なdestination Commonをbest-effortで削除し、ゲーム内蔵Defaultへfallbackできる状態を目指します。適用開始後の途中cancelはありません。

## Runtime のファイル操作

v0.4.0 が書き込む可能性がある場所は次の範囲です。

```text
sdmc:/switch/TexNX/
sdmc:/switch/TexNX/config.json
sdmc:/switch/TexNX/Textures/
sdmc:/atmosphere/contents/01006BD001E06000/romfs/Common/
```

言語設定は安全な置換のため同じdirectoryに一時的な`config.json.tmp`を作成し、完了後に置き換えます。Texture適用時は上記Minecraft Commonだけを削除・再作成します。`sdmc:/switch/TexNX/Textures/<Pack>/Common`は常に読み取り専用で、move、rename、delete、modifyしません。Minecraftの起動状態確認や、それに伴うservice dependencyは実装していません。

## Dependency とライセンス

TexNX のソースコードは [MIT License](LICENSE) で提供されます。Borealis は Apache License 2.0 で提供され、revision は Git submodule pointer により固定されます。Borealis の Material Icons font とその license は GUI の実行に必要な resource として NRO の RomFS に収録されます。Nintendo／Minecraft から抽出した font や asset は使用・収録しません。

TexNX は Nintendo、Mojang Studios、Microsoft の非公式プロジェクトであり、これらの企業による承認・提携・支援を受けていません。リポジトリにはゲームから抽出したデータ、Minecraft/Nintendo の assets、ROM、keys、Atmosphere の配布バイナリを含めません。
