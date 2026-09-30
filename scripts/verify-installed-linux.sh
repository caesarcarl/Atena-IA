#!/usr/bin/env bash
set -u
fail=0
check_file() {
  if [ -x "$1" ]; then printf '[OK] %s\n' "$1"; else printf '[ERRO] ausente/não executável: %s\n' "$1"; fail=1; fi
}
check_file /usr/bin/atena-ui
check_file /usr/bin/atena
check_file /usr/libexec/atena/atena-core
check_file /usr/libexec/atena/atena-ui-bin
if [ -r /usr/share/atena/identity/persona.json ]; then echo '[OK] identidade'; else echo '[ERRO] identidade ausente'; fail=1; fi
if [ -f /usr/share/atena/package-revision ]; then printf '[INFO] pacote: '; cat /usr/share/atena/package-revision; fi
if [ "$fail" -eq 0 ]; then
  echo '[INFO] Consultando o Core:'
  atena doctor || fail=1
else
  echo '[INFO] Se a instalação falhou, consulte /var/log/atena-install.log'
fi
exit "$fail"
