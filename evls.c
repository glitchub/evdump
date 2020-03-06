#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <string.h>
#include "input.generated.h"

#define die(...) fprintf(stderr, __VA_ARGS__), exit(1)

#define set(a, b) (a[(b)/8] & (1 << ((b) & 7)))

int main(void)
{
    printf("EVIOCGABS(0)     = 0x%lX\n", (unsigned long)EVIOCGABS(0));
    printf("EVIOCGABS(15)    = 0x%lX\n", (unsigned long)EVIOCGABS(15));
    printf("EVIOCGBIT(0,8)   = 0x%lX\n", (unsigned long)EVIOCGBIT(0,8));
    printf("EVIOCGBIT(10,8)  = 0x%lX\n", (unsigned long)EVIOCGBIT(10,8));
    printf("EVIOCGBIT(10,15) = 0x%lX\n", (unsigned long)EVIOCGBIT(10,15));

    for (int ev = 0; ev < 32; ev++)
    {
        char device[32];
        sprintf(device, "/dev/input/event%d", ev);
        int fd = open(device, O_RDONLY);
        if (fd < 0)
        {
            if (errno != ENOENT) die("Can't open %s: %s\n", device, strerror(errno));
            goto next;
        }

        printf("%s\n", device);

        char name[64];
        if (ioctl(fd, EVIOCGNAME(sizeof name), name) < 0) *name = 0;
        printf("  %s\n", *name ? name : "Name unknown");

        unsigned char capabilities[(EV_MAX+7)/8];
        int got = ioctl(fd, EVIOCGBIT(0, sizeof capabilities), capabilities);
        if (got != sizeof capabilities)
        {
            printf("  EVIOCGBIT(0) returned %d\n", got);
            goto next;
        }

        for (int bit = 1; bit < EV_MAX; bit++) // note, skip bit 0 = EV_SYN
        if (set(capabilities,bit))
        {
            printf("  Capability %d is %s\n", bit, ed_EV(bit) ?: "unknown");
            switch(bit)
            {
                case EV_ABS:
                {
                    uint8_t axes[(ABS_MAX+7)/8];
                    int got = ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(axes)), axes);
                    if (got != sizeof axes)
                    {
                        printf("EVIOCGBIT(EV_ABS) returned %d\n", got);
                        break;
                    }
                    for (int bit=0; bit < ABS_MAX; bit++) if (set(axes,bit))
                    {
                        printf("    Axis %d is %s\n", bit, ed_ABS(bit)?:"unknown");
                        struct input_absinfo i;
                        if (ioctl(fd, EVIOCGABS(bit), i))
                        {
                            printf("      EVIOCGABS failed: %s\n", strerror(errno));
                            continue;
                        }
                        printf("      Value     : %d\n", i.value);
                        printf("      Minimum   : %d", i.minimum);
                        printf("      Maximum   : %d", i.maximum);
                        printf("      Fuzz      : %d", i.fuzz);
                        printf("      Flat      : %d", i.flat);
                        printf("      Resolution: %d\n", i.resolution);
                    }
                }
            }
        }
        next: close(fd);
    }
}
