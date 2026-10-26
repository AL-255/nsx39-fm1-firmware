#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Downloads only into .cache/ (or the supplied directory); never needs sudo.
set -eu
TASK_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TASK_CACHE=${1:-"$TASK_ROOT/.cache"}
mkdir -p "$TASK_CACHE"
TASK_CACHE=$(CDPATH= cd -- "$TASK_CACHE" && pwd)
if [ ! -x "$TASK_CACHE/toolchain/pi32v2/bin/clang" ]; then
 curl -fL --retry 3 https://pkgman.jieliapp.com/s/linux-toolchain -o "$TASK_CACHE/toolchain.tar.xz"
 printf '%s  %s\n' a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd "$TASK_CACHE/toolchain.tar.xz" | sha256sum -c -
 tar -xJf "$TASK_CACHE/toolchain.tar.xz" -C "$TASK_CACHE"
 ln -s jieli-linux-toolchains-20260730.1 "$TASK_CACHE/toolchain"
fi
if [ ! -d "$TASK_CACHE/ac79-sdk/.git" ]; then
 git clone --depth 1 --single-branch --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$TASK_CACHE/ac79-sdk"
fi
[ "$(git -C "$TASK_CACHE/ac79-sdk" rev-parse HEAD)" = d179b4484759423312073f5fbb232501aa491047 ] || { echo 'Unexpected SDK revision' >&2; exit 1; }
TASK_FONT="$TASK_CACHE/NotoSansCJK-Bold.ttc"
TASK_FONT_SHA=faa5f3656a78b2e2d450d27fe8382c778bc2b6bb5ea29c986664a6a435056ceb
if [ ! -f "$TASK_FONT" ]; then
 # The single upstream font is byte-identical to Ubuntu 24.04's font, without
 # downloading the entire 61 MB CJK family from a potentially stalled APT mirror.
 curl -fL --retry 3 --connect-timeout 15 --max-time 180 --speed-limit 10000 --speed-time 30 \
  https://raw.githubusercontent.com/notofonts/noto-cjk/Sans2.004/Sans/OTC/NotoSansCJK-Bold.ttc -o "$TASK_FONT.tmp"
 printf '%s  %s\n' "$TASK_FONT_SHA" "$TASK_FONT.tmp" | sha256sum -c -
 mv "$TASK_FONT.tmp" "$TASK_FONT"
fi
printf '%s  %s\n' "$TASK_FONT_SHA" "$TASK_FONT" | sha256sum -c -
printf 'JIELI_TOOLCHAIN=%s/toolchain\nAC79_SDK=%s/ac79-sdk\nNSX39_LABEL_FONT=%s\n' "$TASK_CACHE" "$TASK_CACHE" "$TASK_FONT"
