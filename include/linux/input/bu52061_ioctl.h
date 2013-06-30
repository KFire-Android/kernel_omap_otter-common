#ifndef _BU52061_IOCTL_H
#define _BU52061_IOCTL_H

#include <linux/ioctl.h>

struct ioctl_cmd {
       unsigned int halt_key;
};

#define IOC_MAGIC 'b'
#define IOCTL_VALSET _IOW(IOC_MAGIC, 1, struct ioctl_cmd)

#endif