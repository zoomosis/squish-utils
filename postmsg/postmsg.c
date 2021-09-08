/*
 *  postmsg.c
 *
 *  Post a message from stdin to a Squish or FTS-1 *.MSG message base.
 *
 *  Written in 2003 by Andrew Clarke and released to the public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "getopts.h"
#include "prseaddr.h"
#include "llist.h"
#include "msgapi.h"

#define VERSION  "2.0"

#if defined(__Linux__)
#define POSTMSG "Postmsg/Linux"
#elif defined(__FreeBSD__)
#define POSTMSG "Postmsg/FreeBSD"
#elif defined(__WIN32__)
#define POSTMSG "Postmsg/NT"
#elif defined(__OS2__)
#define POSTMSG "Postmsg/OS2"
#elif defined(__DOS__)
#define POSTMSG "Postmsg/DOS"
#elif defined(__UNIX__)
#define POSTMSG "Postmsg/UNIX"
#else
#define POSTMSG "Postmsg"
#endif

#define INPUT_BUFSIZE 4096


static struct
{
    struct
    {
        int usage;
        int version;
        int verbose;

        int no_msgid;
        int no_tzutc;
        int no_pid;

        char area_path[250];
        char area_type[10];
        char from[100];
        char to[100];
        char subj[100];
        char orig_addr[100];
        char dest_addr[100];
        char attr[30];
        char charset[30];
        char origin_text[70];
        char tear_text[70];
    }
    cmd;

    struct
    {
        char domain[100];
        int hasDomain, hasZone, hasNet, hasNode, hasPoint;
        int zone, net, node, point;
    }
    orig, dest;

    list_t *list;
    int total;

    word area_type;
    int timezone;
}
cfg;


static opt_t cmdline[] =
{
    { "?", OPTBOOL, &cfg.cmd.usage },
    { "h", OPTBOOL, &cfg.cmd.usage },
    { "-help", OPTBOOL, &cfg.cmd.usage },
    { "-verbose", OPTBOOL, &cfg.cmd.verbose },
    { "-no-msgid", OPTBOOL, &cfg.cmd.no_msgid },
    { "-no-tzutc", OPTBOOL, &cfg.cmd.no_tzutc },
    { "-no-pid", OPTBOOL, &cfg.cmd.no_pid },
    { "-version", OPTBOOL, &cfg.cmd.version },
    { "m", OPTSTR, cfg.cmd.area_path },
    { "b", OPTSTR, cfg.cmd.area_type },
    { "f", OPTSTR, cfg.cmd.from },
    { "tt", OPTSTR, cfg.cmd.tear_text },
    { "t", OPTSTR, cfg.cmd.to },
    { "s", OPTSTR, cfg.cmd.subj },
    { "ot", OPTSTR, cfg.cmd.origin_text },
    { "o", OPTSTR, cfg.cmd.orig_addr },
    { "n", OPTSTR, cfg.cmd.dest_addr },
    { "a", OPTSTR, cfg.cmd.attr },
    { "c", OPTSTR, cfg.cmd.charset },
    { NULL, 0, NULL }
};


static void usage(char *argv0)
{
    printf(
      "Post a message from stdin to a Squish or SDM (*.msg) format message base.\n"
      "\n"
      "Usage: %s [parameters] < input.txt\n"
      "\n"
      "  -m[path]     Output area path or filename (REQUIRED)\n"
      "  -o[addr]     Origination address (REQUIRED)\n"
      "  -n[addr]     Destination address (for netmail)\n"
      "  -b[type]     Output area type (\"squish\" or \"sdm\") (defaults to \"squish\")\n"
      "  -f[name]     From name (defaults to \"Postmsg\")\n"
      "  -t[name]     To name (defaults to \"All\")\n"
      "  -s[subj]     Subject (defaults to \"Automated posting\")\n"
      "  -a[attr]     Attributes [pcfkhr]\n"
      "  -c[chrs]     Character set (eg. LATIN-1 2)\n"
      "  -ot[text]    Place this text in the origin line\n"
      "  -tt[text]    Place this text after the tear line\n"
      "  --verbose    Verbose output\n"
      "  --no-msgid   Don't add a MSGID control line\n"
      "  --no-pid     Don't add a PID control line\n"
      "  --no-tzutc   Don't add a TZUTC control line\n"
      "  --version    Show version and copyright information, then exit\n"
      "  --help       Show this usage information, then exit\n",
      argv0
    );

    exit(EXIT_FAILURE);
}


static void show_version(void)
{
    printf(POSTMSG " " VERSION "\n");
    printf("Written by Andrew Clarke in June 2003 and released to the public domain.\n");
    printf("Build date: " __DATE__ " " __TIME__ "\n");
    printf("Report bugs to mail@ozzmosis.com or FidoNet 3:633/267.\n");

    exit(EXIT_FAILURE);
}


static void error(char *error_str)
{
    fprintf(stderr, "%s\n", error_str);
    exit(EXIT_FAILURE);
}


static void setup(void)
{
    cfg.total = 0;

    if (*cfg.cmd.area_path == '\0')
    {
        error("No output area path or filename specified");
    }

    cfg.area_type = MSGTYPE_SQUISH;

    if (*cfg.cmd.area_type != '\0')
    {
        if (strstr(cfg.cmd.area_type, "squish") == cfg.cmd.area_type)
        {
            cfg.area_type = MSGTYPE_SQUISH;
        }
        else if (strstr(cfg.cmd.area_type, "sdm") == cfg.cmd.area_type ||
          strstr(cfg.cmd.area_type, "fts-1") == cfg.cmd.area_type ||
          strstr(cfg.cmd.area_type, "*.msg") == cfg.cmd.area_type ||
          strstr(cfg.cmd.area_type, "msg") == cfg.cmd.area_type)
        {
            cfg.area_type = MSGTYPE_SDM;

            if (*cfg.cmd.dest_addr == '\0')
            {
                cfg.area_type |= MSGTYPE_ECHO;
            }
        }
        else
        {
            error("Invalid message area type");
        }
    }

    if (*cfg.cmd.orig_addr == '\0')
    {
        error("No origination address specified");
    }

    prseaddr(cfg.cmd.orig_addr, cfg.orig.domain, &cfg.orig.hasDomain,
      &cfg.orig.zone, &cfg.orig.hasZone, &cfg.orig.net, &cfg.orig.hasNet,
      &cfg.orig.node, &cfg.orig.hasNode, &cfg.orig.point, &cfg.orig.hasPoint);

    if (!cfg.orig.hasNode || !cfg.orig.hasNet || !cfg.orig.hasZone)
    {
        error("Must specify at least zone, net and node in origination address");
    }

    if (cfg.orig.zone == 0)
    {
        error("Origination zone cannot be zero");
    }

    if (cfg.orig.net == 0)
    {
        error("Origination net cannot be zero");
    }

    if (*cfg.cmd.dest_addr != '\0')
    {
        prseaddr(cfg.cmd.dest_addr, cfg.dest.domain, &cfg.dest.hasDomain,
          &cfg.dest.zone, &cfg.dest.hasZone, &cfg.dest.net, &cfg.dest.hasNet,
          &cfg.dest.node, &cfg.dest.hasNode, &cfg.dest.point, &cfg.dest.hasPoint);

        if (!cfg.dest.hasNode)
        {
            error("Must specify at least node number in destination address");
        }

        if (!cfg.dest.hasZone)
        {
            cfg.dest.zone = cfg.orig.zone;
            cfg.dest.hasZone = 1;
        }

        if (!cfg.dest.hasNet)
        {
            cfg.dest.net = cfg.orig.net;
            cfg.dest.hasNet = 1;
        }
    }

    if (*cfg.cmd.from == '\0')
    {
        strcpy(cfg.cmd.from, "Postmsg");
    }

    if (*cfg.cmd.to == '\0')
    {
        strcpy(cfg.cmd.to, "All");
    }

    if (*cfg.cmd.subj == '\0')
    {
        strcpy(cfg.cmd.subj, "Automated posting");
    }
}


static void addtext(char *str)
{
    char *p;

    p = malloc(strlen(str) + 1);

    if (p == NULL)
    {
        error("malloc() failed");
    }

    strcpy(p, str);

    if (list_add_item(cfg.list, p) == 0)
    {
        error("list_add_item() failed");
    }

    cfg.total += strlen(p);
}


static void input(void)
{
    char buf[INPUT_BUFSIZE], *p;

    cfg.list = list_init();

    if (cfg.list == NULL)
    {
        error("list_init() failed");
    }

    while (fgets(buf, sizeof buf, stdin) != NULL)
    {
        p = buf;

        while (*p != '\0')
        {
            if (*p == '\n')
            {
                *p = '\r';
            }

            p++;
        }

        addtext(buf);
    }

    if (*cfg.cmd.dest_addr == '\0')
    {
        strcpy(buf, "\r");
        addtext(buf);
        sprintf(buf, "--- %s\r", *cfg.cmd.tear_text != '\0' ? cfg.cmd.tear_text : POSTMSG " " VERSION);
        addtext(buf);
        sprintf(buf, " * Origin: %s (%s)\r", *cfg.cmd.origin_text != '\0' ? cfg.cmd.origin_text : "Postmsg", cfg.cmd.orig_addr);
        addtext(buf);
    }
}


static int isleap(int year)
{
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}


static unsigned long unixtime(const struct tm *tm)
{
    int year, i;
    unsigned long result;

    result = 0UL;
    year = tm->tm_year + 1900;

    /* traverse through each year */

    for (i = 1970; i < year; i++)
    {
        result += 31536000UL;  /* 60s * 60m * 24h * 365d = 31536000s */
        if (isleap(i))
        {
            /* it was a leap year; add a day's worth of seconds */
            result += 86400UL;  /* 60s * 60m * 24h = 86400s */
        }
    }

    result += 86400UL * tm->tm_yday;
    result += 3600UL * tm->tm_hour;
    result += 60UL * tm->tm_min;
    result += (unsigned long) tm->tm_sec;

    return result;
}


static unsigned long sec_time(void)
{
    time_t now;
    struct tm *tm;
    now = time(NULL);
    tm = gmtime(&now);
    return unixtime(tm);
}


static int tz_my_offset(void)
{
    time_t now;
    struct tm *tm;
    int gm_minutes;
    long gm_days;
    int local_minutes;
    long local_days;

#ifdef HAVE_TZSET
    tzset();
#endif

    now = time(NULL);

    tm = localtime(&now);
    local_minutes = tm->tm_hour * 60 + tm->tm_min;
    local_days = (long) tm->tm_year * 366L + (long) tm->tm_yday;

    tm = gmtime(&now);
    gm_minutes = tm->tm_hour * 60 + tm->tm_min;
    gm_days = (long) tm->tm_year * 366L + (long) tm->tm_yday;

    if (gm_days < local_days)
    {
        local_minutes += 1440;
    }
    else if (gm_days > local_days)
    {
        gm_minutes += 1440;
    }

    return local_minutes - gm_minutes;
}


static dword attr(void)
{
    dword rc;
    char *p;

    rc = MSGLOCAL;

    p = cfg.cmd.attr;

    while (*p != '\0')
    {
        switch (tolower(*p))
        {
        case 'p':
            rc |= MSGPRIVATE;
            break;

        case 'c':
            rc |= MSGCRASH;
            break;

        case 'f':
            rc |= MSGFILE;
            break;

        case 'k':
            rc |= MSGKILL;
            break;

        case 'h':
            rc |= MSGHOLD;
            break;

        case 'r':
            rc |= MSGFRQ;
            break;

        default:
            break;
        }
        p++;
    }

    return rc;
}


static void post(void)
{
    struct _minf minf;
    MSGA *ap;
    MSGH *mp;
    XMSG x;
    union stamp_combo sc;
    time_t now;
    struct tm *tm;
    node_t *node;
    unsigned long unix_now;
    char buf[250], ctrl[250];

    memset(&minf, 0, sizeof minf);

    minf.def_zone = (word) cfg.orig.zone;

    if (MsgOpenApi(&minf) != 0)
    {
        error("MsgOpenApi() failed");
    }

    ap = MsgOpenArea((byte *) cfg.cmd.area_path, MSGAREA_CRIFNEC, cfg.area_type);

    if (ap == NULL)
    {
        error("MsgOpenArea() failed");
    }

    mp = MsgOpenMsg(ap, MOPEN_CREATE, 0);

    if (mp == NULL)
    {
        error("MsgOpenMsg() failed");
    }

    memset(&x, 0, sizeof x);

    now = time(NULL);
    tm = localtime(&now);

    x.date_written = TmDate_to_DosDate(tm, &sc)->msg_st;
    x.date_arrived = x.date_written;

    x.attr = attr();

    x.orig.zone = (word) cfg.orig.zone;
    x.orig.net = (word) cfg.orig.net;
    x.orig.node = (word) cfg.orig.node;
    x.orig.point = (word) cfg.orig.point;

    x.dest.zone = (word) cfg.dest.zone;
    x.dest.net = (word) cfg.dest.net;
    x.dest.node = (word) cfg.dest.node;
    x.dest.point = (word) cfg.dest.point;

    strncpy((char *) x.from, cfg.cmd.from, sizeof x.from);
    strncpy((char *) x.to, cfg.cmd.to, sizeof x.to);
    strncpy((char *) x.subj, cfg.cmd.subj, sizeof x.subj);

    if (cfg.cmd.verbose)
    {
        printf("From : %s, %d:%d/%d.%d\n", cfg.cmd.from, cfg.orig.zone, cfg.orig.net, cfg.orig.node, cfg.orig.point);
        printf("To   : %s", cfg.cmd.to);

        if (*cfg.cmd.dest_addr != '\0')
        {
            printf(", %d:%d/%d.%d\n", cfg.dest.zone, cfg.dest.net, cfg.dest.node, cfg.dest.point);
        }
        else
        {
            putchar('\n');
        }

        printf("Subj : %s\n", cfg.cmd.subj);

        printf(
          "Attr : %s%s%s%s%s%s%s\n",
          x.attr & MSGLOCAL   ? "Loc" : "",
          x.attr & MSGPRIVATE ? " Pvt" : "",
          x.attr & MSGCRASH   ? " Cra" : "",
          x.attr & MSGFILE    ? " File" : "",
          x.attr & MSGKILL    ? " K/S" : "",
          x.attr & MSGHOLD    ? " Hold" : "",
          x.attr & MSGFRQ     ? " FREQ" : ""
        );

        putchar('\n');
    }

    *ctrl = '\0';

    if (!cfg.cmd.no_msgid)
    {
        /* TODO: replace MSGID generation with the same one used in MakeNL */

        unix_now = sec_time();
        sprintf(buf, "\01MSGID: %s %08lx", cfg.cmd.orig_addr, unix_now);
        strcat(ctrl, buf);

        if (cfg.cmd.verbose)
        {
            printf("%s\n", buf + 1);
        }
    }

    if (!cfg.cmd.no_pid && *cfg.cmd.tear_text != '\0')
    {
        sprintf(buf, "\01PID: %s", POSTMSG " " VERSION);
        strcat(ctrl, buf);

        if (cfg.cmd.verbose)
        {
            printf("%s\n", buf + 1);
        }
    }

    if (*cfg.cmd.charset)
    {
        sprintf(buf, "\01CHRS: %s", cfg.cmd.charset);
        strcat(ctrl, buf);

        if (cfg.cmd.verbose)
        {
            printf("%s\n", buf + 1);
        }
    }

    if (!cfg.cmd.no_tzutc)
    {
        cfg.timezone = tz_my_offset();

        sprintf(buf, "\01TZUTC: %.4li", (long)(((cfg.timezone / 60) * 100) + (cfg.timezone % 60)));
        strcat(ctrl, buf);

        if (cfg.cmd.verbose)
        {
            printf("%s\n", buf + 1);
        }
    }

    MsgWriteMsg(mp, 0, &x, NULL, 0, cfg.total, strlen(ctrl), (byte *) ctrl);

    /* TODO: check retval from MsgWritemsg() */

    node = list_node_first(cfg.list);

    while (node != NULL)
    {
        char *str;

        str = list_get_item(node);

        if (str != NULL)
        {
            MsgWriteMsg(mp, 1, NULL, (byte *) str, strlen(str), cfg.total, 0, NULL);
            free(str);
        }

        node = list_node_next(node);
    }

    if (cfg.cmd.verbose)
    {
        if (*cfg.cmd.tear_text != '\0')
        {
            printf("Set tear line text to \"%s\"\n", cfg.cmd.tear_text);
        }

        if (*cfg.cmd.origin_text != '\0')
        {
            printf("Set origin line text to \"%s\"\n", cfg.cmd.origin_text);
        }
    }

    if (cfg.area_type & MSGTYPE_SDM)
    {
        /* *.MSG needs a nul-terminator */

        MsgWriteMsg(mp, 1, NULL, (unsigned char *) "", 1, cfg.total, 0, NULL);
    }

    if (MsgCloseMsg(mp) != 0)
    {
        error("MsgCloseMsg() failed");
    }

    if (MsgCloseArea(ap) != 0)
    {
        error("MsgCloseArea() failed");
    }

    if (MsgCloseApi() != 0)
    {
        error("MsgCloseApi() failed");
    }

    list_term(cfg.list);
}


int main(int argc, char **argv)
{
    getopts(argc, argv, cmdline);

    if (argc < 2 || cfg.cmd.usage)
    {
        usage(*argv);
    }

    if (cfg.cmd.version)
    {
        show_version();
    }

    setup();
    input();
    post();

    return EXIT_SUCCESS;
}
