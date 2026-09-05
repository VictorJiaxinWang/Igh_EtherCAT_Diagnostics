#!/usr/bin/env bash

set -eu

project_root=${1:?project root is required}
unit_file="$project_root/packaging/systemd/igh-ethercat-diagnostics.service"

if [ ! -f "$unit_file" ]; then
    echo "missing systemd unit: $unit_file" >&2
    exit 1
fi

require_line()
{
    expected=$1

    if ! grep -Fxq "$expected" "$unit_file"; then
        echo "missing setting: $expected" >&2
        exit 1
    fi
}

require_line "[Unit]"
require_line "Requires=ethercat.service"
require_line "After=ethercat.service"
require_line "[Service]"
require_line "Type=simple"
require_line "User=root"
require_line "ExecStart=/usr/local/bin/igh-ethercat-diagnostics"
require_line "StateDirectory=igh-ethercat-diagnostics"
require_line "WorkingDirectory=/var/lib/igh-ethercat-diagnostics"
require_line "Restart=on-failure"
require_line "[Install]"
require_line "WantedBy=multi-user.target"

temporary_unit=$(mktemp /tmp/igh-ethercat-diagnostics-unit.XXXXXX.service)
trap 'rm -f -- "$temporary_unit"' EXIT

# Verify unit syntax independently of whether the production binary is installed.
sed 's#^ExecStart=.*#ExecStart=/bin/true#' "$unit_file" > "$temporary_unit"

system_unit_path=$(systemd-analyze unit-paths | paste -sd:)
SYSTEMD_UNIT_PATH="$(dirname "$temporary_unit"):$system_unit_path" \
    systemd-analyze verify "$temporary_unit"
