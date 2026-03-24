#!/usr/bin/env bash
# install.sh — instala o compilador vit e a stdlib
#
# Uso (dentro do repo clonado):
#   bash install.sh
#
# O que faz:
#   1. Instala dependências do sistema (clang, llvm, libcurl, libsqlite3)
#   2. Compila e instala o binário `vit` via cargo
#   3. Copia a stdlib para ~/.vit/lib/

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[0;33m'
RESET='\033[0m'

info()    { echo -e "${CYAN}[vit]${RESET} $*"; }
success() { echo -e "${GREEN}[vit]${RESET} $*"; }
warn()    { echo -e "${YELLOW}[vit]${RESET} $*"; }
error()   { echo -e "${RED}[vit]${RESET} $*" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VIT_LIB="$HOME/.vit/lib"

# ── 1. Dependências ───────────────────────────────────────────────────────────

check_dep() {
    command -v "$1" &>/dev/null || error "Dependência não encontrada: $1. Instale com: sudo apt install $2"
}

check_dep cargo  "cargo (via rustup: https://rustup.rs)"
check_dep clang  "clang"
check_dep llc    "llvm (ex: llvm-18)"

# ── 1b. Dependências das libs (apt) ───────────────────────────────────────────
#
# lib/sqlite.vit   → libsqlite3-dev
# lib/http_client.vit → libcurl4-openssl-dev
#
# Instaladas automaticamente se apt estiver disponível.
# Em outros sistemas (brew, yum, pacman) exibe instruções manuais.

APT_PKGS=()

pkg_installed() {
    dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q "install ok installed"
}

if command -v apt-get &>/dev/null; then
    # LLVM 18 pode não estar no repo padrão em Ubuntu 20.04/22.04.
    # Se não estiver disponível, adiciona o repo oficial do LLVM antes de instalar.
    if ! pkg_installed llvm-18-dev; then
        if ! apt-cache show llvm-18-dev &>/dev/null 2>&1; then
            info "LLVM 18 não encontrado no repo padrão. Adicionando repositório oficial..."
            wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc >/dev/null
            . /etc/os-release
            echo "deb http://apt.llvm.org/${VERSION_CODENAME}/ llvm-toolchain-${VERSION_CODENAME}-18 main" \
                | sudo tee /etc/apt/sources.list.d/llvm-18.list
            sudo apt-get update -qq
        fi
        APT_PKGS+=(llvm-18-dev)
    fi
    pkg_installed libpolly-18-dev      || APT_PKGS+=(libpolly-18-dev)
    pkg_installed libsqlite3-dev       || APT_PKGS+=(libsqlite3-dev)
    pkg_installed libcurl4-openssl-dev || APT_PKGS+=(libcurl4-openssl-dev)
    pkg_installed libpq-dev            || APT_PKGS+=(libpq-dev)

    if [ ${#APT_PKGS[@]} -gt 0 ]; then
        info "Instalando dependências do sistema: ${APT_PKGS[*]}"
        sudo apt-get install -y "${APT_PKGS[@]}" \
            || error "Falha ao instalar dependências. Tente manualmente: sudo apt install ${APT_PKGS[*]}"
        success "Dependências instaladas."
    else
        success "Dependências do sistema já instaladas."
    fi
else
    # Verifica se as libs estão disponíveis de outra forma
    MISSING_LIBS=()
    pkg_check_header() {
        [ -f "$1" ] || MISSING_LIBS+=("$2")
    }
    pkg_check_header /usr/include/sqlite3.h     "libsqlite3-dev"
    pkg_check_header /usr/include/curl/curl.h   "libcurl4-openssl-dev"
    pkg_check_header /usr/include/postgresql    "libpq-dev"

    if [ ${#MISSING_LIBS[@]} -gt 0 ]; then
        warn "apt-get não encontrado. Instale manualmente as dependências:"
        warn "  sqlite3:  brew install sqlite  |  yum install sqlite-devel  |  pacman -S sqlite"
        warn "  libcurl:  brew install curl    |  yum install libcurl-devel |  pacman -S curl"
        warn "Continuando sem garantia de que lib/sqlite.vit e lib/http_client.vit funcionarão."
    else
        success "Dependências do sistema encontradas."
    fi
fi

# ── 2. Compilar e instalar o binário ──────────────────────────────────────────

info "Compilando o compilador vit..."
RUSTFLAGS="-Awarnings" cargo install --path "$SCRIPT_DIR" --quiet
success "Binário 'vit' instalado em $(which vit)"

# ── 3. Instalar a stdlib ──────────────────────────────────────────────────────

LIB_SRC="$SCRIPT_DIR/lib"

[ -d "$LIB_SRC" ] || error "Diretório 'lib/' não encontrado em $SCRIPT_DIR"

info "Instalando stdlib em $VIT_LIB ..."
mkdir -p "$VIT_LIB"

cp "$LIB_SRC/"*.vit "$VIT_LIB/"
cp "$LIB_SRC/"*.c   "$VIT_LIB/"

success "Stdlib instalada:"
for f in "$VIT_LIB"/*.vit; do
    echo "  $(basename "$f")"
done

# ── 3b. cJSON (bundled, zero apt deps) ────────────────────────────────────────
#
# json_parse.vit usa cJSON — single-file, MIT license.
# Baixado diretamente do GitHub para ~/.vit/lib/ se ainda não estiver lá.

CJSON_VER="v1.7.18"
CJSON_BASE="https://raw.githubusercontent.com/DaveGamble/cJSON/${CJSON_VER}"

download_file() {
    local dest="$1" url="$2"
    if [ -f "$dest" ]; then return 0; fi
    if command -v curl &>/dev/null; then
        curl -fsSL "$url" -o "$dest" && return 0
    fi
    if command -v wget &>/dev/null; then
        wget -qO "$dest" "$url" && return 0
    fi
    return 1
}

info "Baixando cJSON ${CJSON_VER}..."
if download_file "$VIT_LIB/cJSON.c" "$CJSON_BASE/cJSON.c" && \
   download_file "$VIT_LIB/cJSON.h" "$CJSON_BASE/cJSON.h"; then
    success "cJSON instalado em $VIT_LIB"
else
    warn "Não foi possível baixar cJSON (sem curl/wget ou sem internet)."
    warn "lib/json_parse.vit não funcionará até que cJSON.c e cJSON.h"
    warn "sejam copiados manualmente para $VIT_LIB/"
fi

# ── 4. Verificar instalação ───────────────────────────────────────────────────

echo
info "Verificando instalação..."
command -v vit &>/dev/null && success "vit está funcionando." || error "Binário não encontrado no PATH."

echo
success "Pronto! Experimente:"
echo "  vit run examples/hello.vit"
echo "  vit build meu_app.vit"
