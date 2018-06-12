// Dump event stream from specfied /dev/input/eventX

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <linux/input.h>
#include "input.generated.h"

#define warn(...) fprintf(stderr, __VA_ARGS__)
#define die(...) warn(__VA_ARGS__), exit(1)

const char *code(int t, int c)
{
    switch(t)
    {
        case EV_SYN: return ed_SYN(c)?:"unknown SYN event";
        case EV_KEY: return ed_KEY(c)?:"unknown KEY event";
        case EV_REL: return ed_REL(c)?:"unknown REL event";
        case EV_ABS: return ed_ABS(c)?:"unknown ABS event";
        case EV_MSC: return ed_MSC(c)?:"unknown MSC event";
        case EV_SW:  return ed_SW(c) ?:"unknown SW event";
        case EV_LED: return ed_LED(c)?:"unknown LED event";
        case EV_SND: return ed_SND(c)?:"unknown SND event";
        case EV_REP: return ed_REP(c)?:"unknown REP event";
        case EV_FF_STATUS: return ed_FF_STATUS(c)?:"unknown FF status";
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    int f;
    struct input_event e;
    int version;
    char name[64];
    struct input_id id;

    if (argc != 2) die("Usage: evdump /dev/input/eventX");
    f=open(argv[1], O_RDWR);
    if (f<0) die("Can't open %s: %s\n", argv[1], strerror(errno));
    
    if (ioctl(f,EVIOCGVERSION,&version) == -1) die("EVIOCGVERSION failed: %s\n", strerror(errno));
    if (version != EV_VERSION) die("Unsupported event version %d\n", version);

    if (ioctl(f,EVIOCGNAME(sizeof name),name) == -1) die("EVIOCGNAME failed: %s\n", strerror(errno));
    if (ioctl(f,EVIOCGID,&id) == -1) die("EVIOCGID failed: %s\n", strerror(errno));

    printf("Name: %s, bus type=%s (%d), vendor=%d, product=%d, version=%d\n", 
        name, ed_BUS(id.bustype)?:"unknown", id.bustype, id.vendor, id.product, id.version);

    while(read(f, &e, sizeof e) == sizeof e)
    {
        printf("%ld.%06ld: type=%d (%s) code=%d (%s) value=%d\n",
            e.time.tv_sec, e.time.tv_usec, e.type, ed_EV(e.type)?:"unknown", e.code, code(e.type,e.code)?:"unknown", e.value);
    }
    die("Read failed: %s\n", strerror(errno));
}
