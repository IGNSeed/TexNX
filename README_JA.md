[English](README.md) | [日本語](README_JA.md)

<p align="center">
  <img src="assets/App/App.png" width="220" alt="TexNX アプリアイコン">
</p>

<h1 align="center">TexNX</h1>

<p align="center">
  Minecraft: Nintendo Switch Edition 向けのシンプルなテクスチャパック切り替えアプリ。<br>
  <strong>最新の安定版: v1.0.0</strong>
</p>

## 概要

TexNXは、**Minecraft: Nintendo Switch Edition** 用のテクスチャパックを一覧表示・切り替えする非公式Nintendo Switch Homebrewアプリです。Atmosphère LayeredFS環境を対象とし、Title ID `01006BD001E06000`に対応します。

操作は、パックを選んで安全に適用するか、ゲーム内蔵のテクスチャへ戻すことに絞っています。パックのダウンロード機能はなく、ゲームデータも同梱しません。

## 主な機能

- `Textures`、`Settings`、`About`を切り替えるSidebar UI
- Borealisによるcontrollerとtouchの両対応
- `System`を初期値とする英語／日本語表示
- パック名、任意の説明文、任意のパックアイコンを表示
- 欠損・破損・非対応アイコン用の内蔵fallback icon
- 適用前の確認Dialogと、UI描画を止めない進捗表示
- パック全体をRAMへ読み込まないstreaming copyとSHA-256検証
- 実際のLayeredFS `Common`を基準にしたCurrent Texture判定
- ゲーム内蔵の`Default`テクスチャへの安全な復帰
- 通常時は`PLUS`で終了

## 必要環境

| 項目 | 必要条件 |
| --- | --- |
| 本体 | Homebrewを実行できるNintendo Switch |
| CFW | LayeredFSが利用可能なAtmosphère |
| ゲーム | Minecraft: Nintendo Switch Edition |
| Title ID | `01006BD001E06000` |
| 対応言語 | English / 日本語 |

Minecraft Bedrock Editionや他のTitle IDは対象外です。

## インストール

[GitHub Releasesの最新版](https://github.com/IGNSeed/TexNX/releases/latest)から`TexNX.zip`をダウンロードし、SDカードのルートへ展開してください。ZIPは次の構成になっています。

```text
switch/
├── TexNX.nro
└── TexNX/
    └── Textures/
```

展開後は次の配置になります。

```text
sdmc:/switch/TexNX.nro
sdmc:/switch/TexNX/Textures/
```

`TexNX.nro`だけをダウンロードする場合は、`sdmc:/switch/TexNX.nro`へ配置し、パック用に`sdmc:/switch/TexNX/Textures/`を作成してください。

## テクスチャパックの構成

各パックは`Textures`直下のdirectoryにし、必ず`Common`を入れてください。

```text
sdmc:/switch/TexNX/Textures/<Pack>/Common/
├── res/
│   ├── gui/
│   │   └── pack_icon.png    # 任意
│   └── description.txt          # 任意、UTF-8の先頭1行
└── ...                               # パックのファイル
```

`<Pack>`のdirectory名が表示名になります。`Common`がないパックは一覧に表示されません。`pack_icon.png`がない、壊れている、または非対応の場合は、TexNX内蔵のdefault texture iconを表示します。

## 使い方

1. LayeredFSファイルを変更する前にMinecraftを終了してください。TexNXはゲームの起動状態を確認しません。
2. Homebrew MenuからTexNXを起動します。
3. Sidebarの`Textures`を`UP`／`DOWN`で選びます。
4. `A`または`RIGHT`を押してテクスチャ一覧へ移動します。
5. パックを選んで`A`を押し、確認後に適用します。touchでも選択できます。
6. 進捗Dialogが完了するまで待ちます。適用開始後の途中cancelはありません。
7. ゲーム内蔵のテクスチャへ戻す場合は`Default`を選びます。LayeredFSの`Common`が削除されます。

Content内で`LEFT`または`B`を押すとSidebarへ戻ります。通常時は`PLUS`で終了できます。適用中は不完全なファイルを残さないため、通常操作と終了操作を無効化します。

## Current Textureの表示

Current Texture cardは、保存した選択情報ではなく、実際のMinecraft LayeredFSを調べて判定します。

| 表示 | 意味 |
| --- | --- |
| `Default` | LayeredFSの`Common`が存在せず、ゲーム内蔵データを使用する状態。 |
| Known pack | path、directory、size、file contentを含め、一覧内のパックと`Common`が完全一致する状態。 |
| `External / Unknown` | `Common`は存在するが、完全なSHA-256 fingerprintが一致するパックが一覧にない状態。 |
| Error | 必要なfilesystem状態を調べられなかった状態。 |

TexNX以外で手動配置した`Common`も、一覧内のパックと完全一致すればKnown packとして検出されます。

## 適用処理と安全性

TexNXは次の順序でパックを適用します。

1. 適用元`Common`の全entryを列挙し、全fileが読み込めることをpreflightで検証
2. 既存のMinecraft LayeredFS `Common`を完全削除
3. 新しいdestination treeを作成
4. 128 KiBの再利用bufferで適用元からstreaming copy
5. preflight、copy-time source、destination read-backのfingerprintを照合
6. 検証成功後にCurrent Texture表示を更新

`sdmc:/switch/TexNX/Textures/<Pack>/Common`の適用元は、TexNXから見て常に読み取り専用です。copyのみを行い、move、rename、delete、modifyはしません。Preflightに失敗した場合は既存のdestinationを変更しません。削除開始後のcopyまたは検証に失敗した場合は、不完全なdestinationの削除を試みます。

TexNXが書き込む範囲は次のパスだけです。

```text
sdmc:/switch/TexNX/
sdmc:/switch/TexNX/config.json
sdmc:/switch/TexNX/config.json.tmp
sdmc:/switch/TexNX/Textures/
sdmc:/atmosphere/contents/01006BD001E06000/romfs/Common/
```

TexNXは永続バックアップ、ロールバック用アーカイブ、バックアップからの復元機能を実装していません。`Default`はLayeredFS `Common`を削除する操作であり、バックアップの復元ではありません。テーマ切り替え機能もありません。

## ソースからのビルド

ビルドにはCMake、devkitPro、devkitA64、libnx、deko3d、uam、switch-glm、固定revisionのBorealis submoduleを使用します。CMake 3.20以上とNinjaを推奨します。Borealisの要件によりC++20でビルドします。

```sh
git clone https://github.com/IGNSeed/TexNX.git
cd TexNX
git submodule update --init external/borealis

export DEVKITPRO=/opt/devkitpro
export DEVKITA64="$DEVKITPRO/devkitA64"

cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake"
cmake --build build --parallel
```

WindowsではdevkitPro MSYS2 shellを使用してください。MSVCや通常のdesktop向けMinGW compilerは対象外です。`build/TexNX.nro`が生成されます。

公式Releaseと同じSDカード構成のZIPを作る場合は次を実行します。

```sh
cmake --build build --target TexNXRelease
```

`build/release/TexNX.zip`が生成されます。アプリアイコン、shader、翻訳resource、Material Icons font、default texture fallback iconはNRO内に収録されます。build成果物はGitで管理しません。

## Releaseの内容

v1.0.0のGitHub Releaseには次の2ファイルだけが含まれます。

- `TexNX.nro` — 単体のHomebrewアプリ
- `TexNX.zip` — SDカード直下へ展開できる配布package

どちらにもMinecraftのファイル、Nintendoのファイル、テクスチャパック、Atmosphèreの配布バイナリ、keysは含まれません。

## ライセンス

TexNXのソースコードは[MIT License](LICENSE)で公開しています。固定した[XITRIX/borealis](https://github.com/XITRIX/borealis)はApache License 2.0で提供されます。実行に必要なMaterial Icons fontとlicenseもruntime resourceとしてNROへ収録しています。

## 免責事項

TexNXは非公式のコミュニティプロジェクトです。Nintendo、Mojang Studios、Microsoftとの提携、公認、支援関係はありません。このrepositoryにはゲームデータ、抽出したゲームasset、ROM、keys、Atmosphèreの公式配布バイナリを含めません。
