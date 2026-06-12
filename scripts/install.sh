#!/usr/bin/env bash
set -euo pipefail

REPO_URL="${MINISQL_REPO_URL:-}"
PREFIX="${PREFIX:-/usr/local}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
WORKDIR="${TMPDIR:-/tmp}/minisql-install-$$"

usage() {
  printf 'Usage: install.sh --repo <git-url> [--prefix <path>]\n'
  printf 'Environment: MINISQL_REPO_URL, PREFIX, BUILD_TYPE\n'
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --repo)
      REPO_URL="${2:-}"
      shift 2
      ;;
    --prefix)
      PREFIX="${2:-}"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      printf 'Unknown option: %s\n' "$1" >&2
      usage
      exit 1
      ;;
  esac
done

if [ -z "$REPO_URL" ]; then
  printf 'Missing repo URL. Pass --repo or set MINISQL_REPO_URL.\n' >&2
  exit 1
fi

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'Missing required command: %s\n' "$1" >&2
    exit 1
  fi
}

need_cmd git
need_cmd cmake
need_cmd c++

jobs() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
  elif command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN
  elif command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu
  else
    printf 2
  fi
}

mkdir -p "$WORKDIR"
trap 'rm -rf "$WORKDIR"' EXIT

git clone --depth 1 "$REPO_URL" "$WORKDIR/MiniSQL"
cmake -S "$WORKDIR/MiniSQL" -B "$WORKDIR/MiniSQL/build" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$WORKDIR/MiniSQL/build" -j"$(jobs)"
cmake --install "$WORKDIR/MiniSQL/build"

printf 'MiniSQL installed to %s/bin\n' "$PREFIX"
printf 'Start server: minisqld --data ./data\n'
printf 'Connect: minisql\n'
