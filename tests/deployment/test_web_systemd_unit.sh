#!/usr/bin/env bash

set -eu

project_root=${1:?project root is required}
source_unit="$project_root/packaging/systemd/igh-ethercat-diagnostics-web.service"

if [ ! -f "$source_unit" ]; then
    echo "missing Web systemd unit: $source_unit" >&2
    exit 1
fi

temporary_directory=$(mktemp -d /tmp/igh-ethercat-web-unit.XXXXXX)
trap 'rm -rf -- "$temporary_directory"' EXIT

sed 's#^ExecStart=.*#ExecStart=/bin/true#' "$source_unit" \
    > "$temporary_directory/igh-ethercat-diagnostics-web.service"

printf '%s\n' \
    '[Unit]' \
    'Description=Test dependency' \
    '[Service]' \
    'Type=oneshot' \
    'ExecStart=/bin/true' \
    > "$temporary_directory/igh-ethercat-diagnostics.service"

system_unit_path=$(systemd-analyze unit-paths | paste -sd:)
SYSTEMD_UNIT_PATH="$temporary_directory:$system_unit_path" \
    systemd-analyze verify \
    "$temporary_directory/igh-ethercat-diagnostics-web.service"

for required in \
    'ExecStart=/usr/local/bin/igh-ethercat-diagnostics-web --bind 0.0.0.0 --port 8080 --data-dir /var/lib/igh-ethercat-diagnostics/logs --assets-dir /usr/local/share/igh-ethercat-diagnostics/web' \
    'User=nobody' \
    'Group=nogroup' \
    'NoNewPrivileges=true' \
    'ProtectSystem=strict' \
    'ProtectHome=true' \
    'PrivateTmp=true'; do
    if ! grep -Fxq "$required" "$source_unit"; then
        echo "missing Web service hardening: $required" >&2
        exit 1
    fi
done
