# Third-party notices

## IgH EtherCAT Master 1.6.3 ABI headers

This project includes the following files from the IgH EtherCAT Master 1.6.3 tag:

- `third_party/igh-ethercat-1.6.3/master/ioctl.h`
- `third_party/igh-ethercat-1.6.3/master/globals.h`
- `third_party/igh-ethercat-1.6.3/globals.h`
- `third_party/igh-ethercat-1.6.3/include/ecrt.h`

Upstream source: <https://gitlab.com/etherlab.org/ethercat/-/tags/1.6.3>

Copyright (C) Florian Pose, Ingenieurgemeinschaft IgH, and the contributors named in the imported files.

The imported files are distributed under the GNU Lesser General Public License, version 2.1. Their original copyright and license notices are preserved. A locally supplied `config.h` contains only the build constants required to reproduce the ABI layout of the installed single-device IgH driver.

The installed MagicLab RK3588 module reports version 1.6.3 while retaining ioctl magic 32. Project-owned `include/ethercat_diag/infrastructure/igh_ioctl_abi.h` records this runtime compatibility value. Automated ABI tests verify that the upstream 1.6.3 request numbers, structure sizes, and key field offsets match the installed `/bin/ethercat` DWARF definitions before those structures are used.

No source from the GPL-2.0 `tool/MasterDevice.cpp` or `tool/MasterDevice.h` implementation is included.
