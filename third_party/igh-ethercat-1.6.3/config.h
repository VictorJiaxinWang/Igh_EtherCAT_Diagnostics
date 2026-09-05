#ifndef ETHERCAT_DIAG_IGH_1_6_3_CONFIG_H
#define ETHERCAT_DIAG_IGH_1_6_3_CONFIG_H

/*
 * Values needed by the imported IgH 1.6.3 userspace ioctl ABI headers.
 * The installed RK3588 driver request EC_IOCTL_MASTER is 0x80f8a401,
 * confirming that it was built with the default single-device layout.
 */
#define VERSION "1.6.3"
#define REV unknown
#define EC_MAX_NUM_DEVICES 1

#endif
