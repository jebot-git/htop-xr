#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
stage="${1:-$root/build/stage}"
cmake --install "$root/build/gui" --prefix "$stage/usr/local"
install -Dm755 "$root/build/tui/htop" "$stage/usr/local/libexec/htop/htop-tui"
install -Dm755 "$root/de-xr/htop" "$stage/usr/local/bin/htop"
install -Dm644 "$root/de-xr/org.domed.Htop.desktop" "$stage/usr/local/share/applications/org.domed.Htop.desktop"
install -Dm644 "$root/htop.svg" "$stage/usr/local/share/icons/hicolor/scalable/apps/htop.svg"
install -Dm644 "$root/build/tui/htop.1" "$stage/usr/local/share/man/man1/htop.1"
for name in COPYING de-xr/README.md de-xr/UPSTREAM.json fltk/LICENSE.shared-ui; do
    install -Dm644 "$root/$name" "$stage/usr/local/share/doc/htop-de-xr/$(basename "$name")"
done
desktop-file-validate "$stage/usr/local/share/applications/org.domed.Htop.desktop"
(cd "$stage"; find usr -type f -print0 | sort -z | xargs -0 sha256sum > manifest.sha256)
printf 'Staged %s\n' "$stage"
