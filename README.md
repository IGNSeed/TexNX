# TexNX

TexNX は、Atmosphere CFW を導入した Nintendo Switch 上で動作する、Minecraft: Nintendo Switch Edition 向けの非公式 Homebrew アプリケーションです。対象 Title ID は `01006BD001E06000` です。

現在のバージョンは `0.1.0` で、今後のテクスチャパック管理機能に向けた初期開発基盤です。この版はファイルを変更せず、SDカード、Minecraft LayeredFS root、`Common` directory の存在を読み取り専用で確認します。テクスチャのコピー、削除、置換、バックアップ、展開、ダウンロードはまだ実装していません。

## 現在の機能

- libnx の text console に検出結果を表示
- `sdmc:/` へのアクセス確認
- `sdmc:/atmosphere/contents/01006BD001E06000/romfs` の存在確認
- `sdmc:/atmosphere/contents/01006BD001E06000/romfs/Common` の存在確認
- directory が存在しない場合と filesystem error を区別
- Handheld、Joy-Con、Joy-Con Grip、Pro Controller を含む標準 controller style に対応
- PLUS button で cleanup を行って Homebrew Menu へ戻る

## 開発環境

必要なもの:

- devkitPro
- devkitA64
- libnx
- switch-cmake
- CMake 3.20 以上

TexNX は C++17 と CMake を使用します。Switch 向け build には必ず devkitA64 toolchain を使用してください。MSVC、通常の Windows MinGW、x86/x64 compiler は対象外です。

Windows では devkitPro MSYS2 shell から次のように build できます。

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITA64="$DEVKITPRO/devkitA64"

cmake -S . -B build -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake"
cmake --build build --parallel
```

生成物は `build/TexNX.nro` です。NACP には app name `TexNX`、version `0.1.0`、author `IGNSeed` が設定されます。専用の権利クリアな artwork がまだないため、この版は custom icon を同梱しません。

## 使用方法

生成した `TexNX.nro` は、将来的に次の場所へ配置する想定です。

```text
sdmc:/switch/TexNX/TexNX.nro
```

Homebrew Menu から起動すると各 directory の状態を `Found`、`Not Found`、または `Error` として表示します。終了するには PLUS button を押します。

## 将来の予定

今後は Borealis GUI、テクスチャパック一覧、icon/description、preview、transactional copy、backup、進捗表示、error dialog などを段階的に追加する予定です。LayeredFS の変更機能では、既存データを守るため `copy -> verify -> replace` のような安全な方式を採用します。

## ライセンスと免責

TexNX のソースコードは [MIT License](LICENSE) で提供されます。

TexNX は Nintendo、Mojang Studios、Microsoft の非公式プロジェクトであり、これらの企業による承認・提携・支援を受けていません。リポジトリにはゲームから抽出したデータ、Minecraft/Nintendo の assets、ROM、keys、Atmosphere の配布バイナリを含めません。
