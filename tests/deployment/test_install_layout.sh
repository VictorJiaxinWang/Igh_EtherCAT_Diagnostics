#!/usr/bin/env bash

set -eu

build_directory=${1:?build directory is required}
project_root=${2:?project root is required}

staging_directory=$(mktemp -d /tmp/igh-ethercat-diagnostics-install.XXXXXX)
trap 'rm -rf -- "$staging_directory"' EXIT

DESTDIR="$staging_directory" cmake --install "$build_directory"

installed_binary="$staging_directory/usr/local/bin/igh-ethercat-diagnostics"
installed_web_binary="$staging_directory/usr/local/bin/igh-ethercat-diagnostics-web"
installed_unit="$staging_directory/usr/local/lib/systemd/system/igh-ethercat-diagnostics.service"
installed_web_unit="$staging_directory/usr/local/lib/systemd/system/igh-ethercat-diagnostics-web.service"
source_unit="$project_root/packaging/systemd/igh-ethercat-diagnostics.service"
source_web_unit="$project_root/packaging/systemd/igh-ethercat-diagnostics-web.service"
installed_assets="$staging_directory/usr/local/share/igh-ethercat-diagnostics/web"

if [ ! -x "$installed_binary" ]; then
    echo "missing installed executable: $installed_binary" >&2
    exit 1
fi

if [ ! -x "$installed_web_binary" ]; then
    echo "missing installed Web executable: $installed_web_binary" >&2
    exit 1
fi

if [ ! -f "$installed_unit" ]; then
    echo "missing installed systemd unit: $installed_unit" >&2
    exit 1
fi

if ! cmp -s "$source_unit" "$installed_unit"; then
    echo "installed systemd unit differs from source" >&2
    exit 1
fi

if [ ! -f "$installed_web_unit" ] || ! cmp -s "$source_web_unit" "$installed_web_unit"; then
    echo "missing or different installed Web systemd unit" >&2
    exit 1
fi

for asset in index.html styles.css app.js; do
    if [ ! -s "$installed_assets/$asset" ]; then
        echo "missing installed Web asset: $installed_assets/$asset" >&2
        exit 1
    fi
done
