#!/usr/bin/env bash
# Empacota a build de PC (Linux x86-64) do Nébula 360 para uma pré-release.
#
#   tools/release/package_linux.sh VERSAO PASTA_SAIDA
#
# Compila em Release, roda todos os testes e gera:
#   PASTA_SAIDA/nebula360-VERSAO-linux-x86_64.tar.gz
#   PASTA_SAIDA/SHA256SUMS.txt
# Não inclui nenhum arquivo de terceiros além do que está em
# THIRD_PARTY_NOTICES.md (stb e fonte Inter).
set -euo pipefail

VERSION="${1:?informe a versão, ex.: v0.3.0-alpha.1}"
OUT="${2:?informe a pasta de saída}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/build-release"
NAME="nebula360-${VERSION#v}-linux-x86_64"

cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD"
"$BUILD/nebula_tests"

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/$NAME/tools"
cp "$BUILD/nebula360" "$BUILD/nebula_scan" "$STAGE/$NAME/"
strip "$STAGE/$NAME/nebula360" "$STAGE/$NAME/nebula_scan"
cp -r "$ROOT/assets" "$STAGE/$NAME/assets"
cp "$ROOT/tools/demo/make_demo_data.py" "$STAGE/$NAME/tools/"
cp "$ROOT/README.md" "$ROOT/THIRD_PARTY_NOTICES.md" "$STAGE/$NAME/"
cp "$ROOT/third_party/stb/LICENSE.txt" "$STAGE/$NAME/LICENSE-stb.txt"
if [ -f "$ROOT/docs/releases/$VERSION.md" ]; then
  cp "$ROOT/docs/releases/$VERSION.md" "$STAGE/$NAME/LEIA-ME.md"
fi

# Garantia: nenhum executável de console ou pacote de terceiros no pacote.
if find "$STAGE" -iname '*.xex' -o -iname '*.rar' -o -iname '*.zip' | grep -q .; then
  echo "erro: arquivo proibido no pacote" >&2
  exit 1
fi

tar -C "$STAGE" -czf "$OUT/$NAME.tar.gz" "$NAME"
(cd "$OUT" && sha256sum "$NAME.tar.gz" > SHA256SUMS.txt)
echo "pacote: $OUT/$NAME.tar.gz"
cat "$OUT/SHA256SUMS.txt"
