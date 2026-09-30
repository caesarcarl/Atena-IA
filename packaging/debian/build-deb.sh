#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VERSION=0.5.4
ARCH=amd64
OUT="${1:-$ROOT/atena_${VERSION}_${ARCH}.deb}"
PKG="$(mktemp -d)"
trap 'rm -rf "$PKG"' EXIT

mkdir -p "$PKG/DEBIAN" "$PKG/usr/bin" "$PKG/usr/lib/atena" "$PKG/usr/libexec/atena" \
  "$PKG/usr/share/applications" "$PKG/usr/share/atena/identity" "$PKG/usr/share/atena/assets" \
  "$PKG/usr/share/icons/hicolor" "$PKG/usr/share/pixmaps" "$PKG/usr/share/doc/atena" \
  "$PKG/usr/lib/systemd/user" "$PKG/usr/lib/atena/python/atena_worker"

cp -a "$ROOT" "$PKG/usr/lib/atena/source"
install -m 0755 "$ROOT/packaging/linux/atena-ui-launcher.sh" "$PKG/usr/bin/atena-ui"
install -m 0644 "$ROOT/packaging/linux/atena.desktop" "$PKG/usr/share/applications/atena.desktop"
cp -a "$ROOT/identity/." "$PKG/usr/share/atena/identity/"
cp -a "$ROOT/python/atena_worker/." "$PKG/usr/lib/atena/python/atena_worker/"
mkdir -p "$PKG/usr/share/atena/assets/branding" "$PKG/usr/share/atena/assets/icons" "$PKG/usr/share/atena/assets/illustrations"
cp -a "$ROOT/ui/qt/resources/branding/." "$PKG/usr/share/atena/assets/branding/"
cp -a "$ROOT/ui/qt/resources/icons/." "$PKG/usr/share/atena/assets/icons/"
cp -a "$ROOT/ui/qt/resources/illustrations/." "$PKG/usr/share/atena/assets/illustrations/"
for size in 16 24 32 48 64 96 128 256 512; do
  mkdir -p "$PKG/usr/share/icons/hicolor/${size}x${size}/apps"
  install -m 0644 "$ROOT/ui/qt/resources/branding/app-icon/atena-${size}x${size}.png" \
    "$PKG/usr/share/icons/hicolor/${size}x${size}/apps/atena.png"
done
install -m 0644 "$ROOT/ui/qt/resources/branding/app-icon/atena-256x256.png" "$PKG/usr/share/pixmaps/atena.png"

cat > "$PKG/usr/lib/systemd/user/atena-core.service" <<'UNIT'
[Unit]
Description=Atena Core 0.5.4
After=network.target
[Service]
Type=simple
ExecStart=/usr/libexec/atena/atena-core
Restart=on-failure
RestartSec=2
NoNewPrivileges=true
PrivateTmp=true
Environment=ATENA_IDENTITY_DIR=/usr/share/atena/identity
[Install]
WantedBy=default.target
UNIT

printf '%s\n' "$VERSION" > "$PKG/usr/share/atena/package-revision"
cp "$ROOT/README.md" "$PKG/usr/share/doc/atena/README"
cp "$ROOT/CHANGES-0.4.2.md" "$PKG/usr/share/doc/atena/changelog"
(
  cd "$PKG/usr/share/atena/assets"
  find . -type f ! -name ASSET-MANIFEST.sha256 -print0 | sort -z | xargs -0 sha256sum > ASSET-MANIFEST.sha256
)

install -m 0644 "$ROOT/packaging/debian/control" "$PKG/DEBIAN/control"
install -m 0755 "$ROOT/packaging/debian/postinst" "$PKG/DEBIAN/postinst"
install -m 0755 "$ROOT/packaging/debian/prerm" "$PKG/DEBIAN/prerm"
install -m 0755 "$ROOT/packaging/debian/postrm" "$PKG/DEBIAN/postrm"
chmod g-s "$PKG" "$PKG/DEBIAN" || true
chmod 0755 "$PKG" "$PKG/DEBIAN"

dpkg-deb --root-owner-group --build "$PKG" "$OUT"
echo "Gerado: $OUT"
