#!/usr/bin/env bash
# Stage as the regular user first. Only this file-copy phase needs root.
set -euo pipefail
[[ $EUID == 0 ]] || { echo 'Run sudo de-xr/install.sh after building and staging.' >&2; exit 1; }
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
stage="${1:-$root/build/stage}"
(cd "$stage"; sha256sum -c manifest.sha256)
backup="/var/backups/htop-de-xr/$(date +%s)"
while read -r hash relative; do
    [[ $relative == usr/local/* && $relative != *..* && -f $stage/$relative && ! -L $stage/$relative ]] || exit 1
    target="/$relative"
    if [[ -e $target || -L $target ]]; then
        mkdir -p "$backup/$(dirname "$relative")"
        cp -a -- "$target" "$backup/$relative"
    fi
    mode=0644
    [[ -x $stage/$relative ]] && mode=0755
    install -Dm "$mode" -o root -g root "$stage/$relative" "$target"
done < "$stage/manifest.sha256"
update-desktop-database /usr/local/share/applications
command -v restorecon >/dev/null && restorecon /usr/local/bin/htop /usr/local/libexec/htop/* || true
printf 'Installed htop de-xr; backup: %s\n' "$backup"
