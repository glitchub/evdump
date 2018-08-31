// Dump event stream from specfied /dev/input/eventX

#define _GNU_SOURCE // for asprintf
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
#include <poll.h>
#include "input.generated.h"

#define warn(...) fprintf(stderr, __VA_ARGS__)
#define die(...) warn(__VA_ARGS__), exit(1)

#define MAXDEVS 16

#define usage() die("\
Usage:\n\
\n\
    evdump device [... device]\n\
\n\
Show events from up to 16 specified event devices.\n\
\n\
Device names can be specified as a full path, such as '/dev/input/event3', or\n\
as 'event3', or just as '3'.\n\
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


int main(int argc, char* argv[])
{
    int ndevs;
    struct pollfd dev[MAXDEVS];
    char *name[MAXDEVS];

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
                int version;
                char dn[64];
                struct input_id id;

                if (ioctl(dev[ndevs].fd,EVIOCGVERSION,&version) == -1) die("%s EVIOCGVERSION failed: %s\n", name[ndevs], strerror(errno));
                if (version != EV_VERSION) die("%s unsupported event version %d\n", name[ndevs], version);
                if (ioctl(dev[ndevs].fd,EVIOCGNAME(sizeof dn),dn) == -1) die("%s: EVIOCGNAME failed: %s\n", name[ndevs], strerror(errno));
                if (ioctl(dev[ndevs].fd,EVIOCGID,&id) == -1) die("%s EVIOCGID failed: %s\n", name[ndevs], strerror(errno));
                
                printf("%s name='%s' bus=%d (%s) vendor=%d product=%d version=%d\n", 
                    name[ndevs], dn, id.bustype, ed_BUS(id.bustype)?:"unknown", id.vendor, id.product, id.version);

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
            struct input_event e;
            if (dev[n].revents & POLLIN)
            {
                if (read(dev[n].fd, &e, sizeof e) != sizeof e) die("%s read failed: %s\n", name[n], strerror(errno));
                printf("%s %ld.%06ld type=%d (%s) code=%d (%s) value=%d\n",
                    name[n], e.time.tv_sec, e.time.tv_usec, e.type, ed_EV(e.type)?:"unknown", e.code, code(e.type,e.code)?:"unknown", e.value);
            }        
            dev[n].revents=0;
        }
    }    
    die("Poll failed: %s\n", strerror(errno));
}
