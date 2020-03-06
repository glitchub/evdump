#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
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

// true if bit b is set in byte array a
#define set(a, b) (a[(b)/8] & (1 << ((b) & 7)))

int main(void)
{
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

        printf("%s:\n", device);

        char name[64];
        if (ioctl(fd, EVIOCGNAME(sizeof name), name) < 0) *name = 0;
        printf("  Name \"%s\"\n", *name ? name : "Name unknown");

        int version;
        if (ioctl(fd, EVIOCGVERSION, &version))
        {
            printf("  EVIOCGVERSION failed\n");
            goto next;
        }
        else if (version != EV_VERSION)
        {
            printf("  Unexpected stack version %d\n", version);
            goto next;
        }

        struct input_id id;
        if (ioctl(fd, EVIOCGID, &id))
        {
            printf("  EVIOCGID failed\n");
            goto next;
        }
        printf("  Vendor %d\n", id.vendor);
        printf("  Product %d\n", id.product);
        printf("  Version %d\n", id.version);
        printf("  Bus type %d is %s\n", id.bustype, ed_BUS(id.bustype)?:"unknown");

        uint8_t capabilities[(EV_MAX+7)/8];
        if (ioctl(fd, EVIOCGBIT(0, sizeof capabilities), capabilities) != sizeof capabilities)
        {
            printf("  EVIOCGBIT(0) failed\n");
            goto next;
        }

        for (int bit = 1; bit < EV_MAX; bit++) // note, skip bit 0 = EV_SYN
        if (set(capabilities, bit))
        {
            printf("  Capability %d is %s\n", bit, ed_EV(bit)?:"unknown");
            switch(bit)
            {
                case EV_KEY:
                {
                    uint32_t repeat[2];
                    if (!ioctl(fd, EVIOCGREP, &repeat))
                    {
                        printf("    Repeat delay %d mS\n", repeat[0]);
                        printf("    Repeat period %d mS\n", repeat[1]);
                    }
                    // Don't list all supported keys, there are hundreds, just show active
                    uint8_t status[(KEY_MAX+7)/8];
                    if (ioctl(fd, EVIOCGKEY(sizeof(status)), status) == sizeof status)
                        for (int bit=0; bit < KEY_MAX; bit++)
                            if (set(status, bit))
                                printf("    Active key %d is %s\n", bit, ed_KEY(bit)?:"unknown");
                    break;
                }
                case EV_REL:
                {
                    uint8_t supported[(REL_MAX+7)/8];
                    if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof supported), supported) == sizeof supported)
                        for (int bit=0; bit < REL_MAX; bit++)
                            if (set(supported, bit))
                                printf("    Relative event %d is %s\n", bit, ed_REL(bit)?:"unknown");
                    break;
                }
                case EV_ABS:
                {
                    uint8_t supported[(ABS_MAX+7)/8];
                    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(supported)), supported) == sizeof supported)
                        for (int bit=0; bit < ABS_MAX; bit++)
                            if (set(supported, bit))
                            {
                                printf("    Axis %d is %s\n", bit, ed_ABS(bit)?:"unknown");
                                struct input_absinfo i;
                                if (ioctl(fd, EVIOCGABS(bit), &i))
                                {
                                    printf("      Value     : %d\n", i.value);
                                    printf("      Minimum   : %d\n", i.minimum);
                                    printf("      Maximum   : %d\n", i.maximum);
                                    printf("      Fuzz      : %d\n", i.fuzz);
                                    printf("      Flat      : %d\n", i.flat);
                                    printf("      Resolution: %d\n", i.resolution);
                                }
                            }
                    break;
                }
                case EV_MSC:
                {
                    uint8_t supported[(MSC_MAX+7)/8];
                    if (ioctl(fd, EVIOCGBIT(EV_MSC, sizeof supported), supported) == sizeof supported)
                        for (int bit=0; bit < MSC_MAX; bit++)
                            if (set(supported, bit))
                                printf("    Misc event %d is %s\n", bit, ed_MSC(bit)?:"unknown");
                    break;
                }
                case EV_SW:
                {
                    uint8_t supported[(SW_MAX+7)/8];
                    if (ioctl(fd, EVIOCGBIT(EV_SW, sizeof supported), supported) == sizeof supported)
                    {
                        uint8_t status[sizeof supported];
                        bool valid=true;
                        if (ioctl(fd, EVIOCGSW(sizeof(status)), status) != sizeof status) valid=false;
                        for (int bit=0; bit < SW_MAX; bit++)
                            if (set(supported, bit))
                            {
                                if (valid && set(status, bit))
                                    printf("    Active switch %d is %s\n", bit, ed_SW(bit)?:"unknown");
                                else
                                    printf("    Switch %d is %s\n", bit, ed_SW(bit)?:"unknown");
                            }
                    }
                }
                case EV_LED:
                {
                    uint8_t supported[(LED_MAX+7)/8];
                    if (ioctl(fd, EVIOCGBIT(EV_LED, sizeof supported), supported) == sizeof supported)
                    {
                        bool valid=true;
                        uint8_t status[sizeof supported];
                        if (ioctl(fd, EVIOCGLED(sizeof(status)), status) != sizeof status) valid=false;
                        for (int bit=0; bit < LED_MAX; bit++)
                            if (set(supported, bit))
                            {
                                if (valid && set(status, bit))
                                    printf("    Active LED %d is %s\n", bit, ed_LED(bit)?:"unknown");
                                else
                                    printf("    LED %d is %s\n", bit, ed_LED(bit)?:"unknown");
                            }
                    }
                    break;
                }
                case EV_SND:
                {
                    uint8_t supported[(SND_MAX+7)/8];
                    if (ioctl(fd, EVIOCGBIT(EV_SND, sizeof supported), supported) == sizeof supported)
                    {
                        bool valid=true;
                        uint8_t status[sizeof supported];
                        if (ioctl(fd, EVIOCGSND(sizeof(status)), status) != sizeof status) valid=false;
                        for (int bit=0; bit < SND_MAX; bit++)
                            if (set(supported, bit))
                            {
                                if (valid && set(status, bit))
                                    printf("    Active sound %d is %s\n", bit, ed_SND(bit)?:"unknown");
                                else
                                    printf("    Sound %d is %s\n", bit, ed_SND(bit)?:"unknown");
                            }
                    }
                    break;
                }

            }
        }
        next: close(fd);
    }
}
