// Dump event stream from specfied /dev/input/eventX

#define _GNU_SOURCE // for asprintf
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <linux/input.h>
#include <poll.h>
#include "input.generated.h"

#define warn(...) fprintf(stderr, __VA_ARGS__)
#define die(...) warn(__VA_ARGS__), exit(1)

#define MAXDEVS 16

#define usage() die("\
Usage:\n\
\n\
    evdump [options] device [... device]\n\
\n\
Dump events from up to 16 specified event devices to stdout.\n\
\n\
Options are:\n\
\n\
    -t X    - only output type X events\n\
    -c X    - only output code X events\n\
    -v X    - only output value X events\n\
\n\
Device names can be specified as a full path, such as '/dev/input/event3', or\n\
as 'event3', or just as '3'.\n\
\n\
Devices names containing the word 'mouse' or 'mice' are treated as generic\n\
three-octet devices and generate virtual event streams, similar to what would\n\
be reported by a real event device.\n\
")

const char *code(int t, int c)
{
    switch(t)
    {
        case EV_SYN: return ed_SYN(c)?:"unknown SYN event";
        case EV_KEY: return ed_KEY(c)?:ed_BTN(c)?:"unknown KEY event";
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

// event filters
int onlytype=-1, onlycode=-1, onlyvalue=-1;

// report the given event
void report(char *ename, struct timeval *etime, int etype, int ecode, int evalue)
{
    // filter out unwanted events
    if (onlytype != -1 && etype != onlytype) return;
    if (onlycode != -1 && ecode != onlycode) return;
    if (onlyvalue != -1 && evalue != onlyvalue) return;

    printf("%s %ld.%06ld type=%d (%s) code=%d (%s) value=%d\n", ename, etime->tv_sec, etime->tv_usec, etype, ed_EV(etype)?:"unknown", ecode, code(etype,ecode)?:"unknown", evalue);
    fflush(stdout);
}

int main(int argc, char*argv[])
{
    int ndevs;
    struct pollfd dev[MAXDEVS];
    char *name[MAXDEVS];
    bool ismouse[MAXDEVS]; // true if the device generates three-octet event codes

    while (1) switch (getopt(argc,argv,":t:c:v:"))
    {
        case 't': onlytype=strtoul(optarg,NULL,0); break;
        case 'c': onlycode=strtoul(optarg,NULL,0); break;
        case 'v': onlyvalue=strtoul(optarg,NULL,0); break;

        case ':':            // missing
        case '?': usage();   // or invalid options
        case -1: goto optx;  // no more options
    } optx:
    argc-=optind-1;
    argv+=optind-1;

    if (argc < 2) usage();

    for (ndevs=0; ndevs < argc-1; ndevs++)
    {
        char *paths[] = {"%s", "/dev/%s", "/dev/input/%s", "/dev/input/event%s", NULL };
        if (ndevs >= MAXDEVS) die("Can't specify more than %d devices\n", MAXDEVS);
        for (char**p=paths;;p++)
        {
            if (!*p) die("Can't find event device '%s'\n", argv[ndevs+1]);
            asprintf(&name[ndevs], *p, argv[ndevs+1]);
            if ((dev[ndevs].fd = open(name[ndevs], O_RDWR))>=0)
            {
                ismouse[ndevs] = (strstr(name[ndevs],"mouse") || strstr(name[ndevs],"mice"));

                if (!ismouse[ndevs])
                {
                    int version;
                    char dn[64];
                    struct input_id id;
                    if (ioctl(dev[ndevs].fd,EVIOCGVERSION,&version) == -1) die("%s EVIOCGVERSION failed: %s\n", name[ndevs], strerror(errno));
                    if (version != EV_VERSION) die("%s unsupported event version %d\n", name[ndevs], version);
                    if (ioctl(dev[ndevs].fd,EVIOCGNAME(sizeof dn),dn) == -1) die("%s: EVIOCGNAME failed: %s\n", name[ndevs], strerror(errno));
                    if (ioctl(dev[ndevs].fd,EVIOCGID,&id) == -1) die("%s EVIOCGID failed: %s\n", name[ndevs], strerror(errno));
                    warn("%s name='%s' bus=%d (%s) vendor=%d product=%d version=%d\n", name[ndevs], dn, id.bustype, ed_BUS(id.bustype)?:"unknown", id.vendor, id.product, id.version);
                }
                else
                {
                    warn("%s name='mouse'\n", name[ndevs]);
                }
                dev[ndevs].events=POLLIN;
                dev[ndevs].revents=0;
                break;
            }
            if (errno==EACCES) die("%s permission denied (are you root?)\n",name[ndevs]);
            free(name[ndevs]);
        }
    }

    while (poll(dev, ndevs, -1) > 0)
    {
        for (int n=0; n < ndevs; n++)
        {
            if (dev[n].revents & POLLIN)
            {
                if (ismouse[n])
                {
                    // Simulate relative mouse:
                    //   EV_KEY, BTN_MOUSE|BTN_RIGHT|BTN_MIDDLE, 1 or 0 (for each key change)
                    //   EV_REL, REL_X, value
                    //   EV_REL, REL_Y, value
                    //   EV_SYN, SYN_REPORT, 0
                    static int8_t pressed=0;
                    struct timeval t;
                    int8_t bxy[3];
                    if (read(dev[n].fd, bxy, 3) != 3) die("%s read failed: %s\n", name[n], strerror(errno));
                    gettimeofday(&t, NULL);
                    int8_t change=(bxy[0] ^ pressed);
                    pressed=bxy[0];
                    if (change & 1) report(name[n], &t, EV_KEY, BTN_MOUSE, (bxy[0] & 1) != 0);
                    if (change & 2) report(name[n], &t, EV_KEY, BTN_RIGHT, (bxy[0] & 2) != 0);
                    if (change & 4) report(name[n], &t, EV_KEY, BTN_MIDDLE, (bxy[0] & 4) != 0);
                    if (bxy[1]) report(name[n], &t, EV_REL, REL_X, bxy[1]);
                    if (bxy[2]) report(name[n], &t, EV_REL, REL_Y, -bxy[2]); // y is negated
                    report(name[n], &t, EV_SYN, SYN_REPORT, 0);
                } else
                {
                    struct input_event e;
                    if (read(dev[n].fd, &e, sizeof e) != sizeof e) die("%s read failed: %s\n", name[n], strerror(errno));
                    report(name[n], &e.time, e.type, e.code, e.value);
                }
            }
            dev[n].revents=0;
        }
    }
    die("Poll failed: %s\n", strerror(errno));
}
