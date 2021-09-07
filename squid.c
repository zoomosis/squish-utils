/*
 *  squid.c
 *
 *  Displays information about a Squish message base.  Just like SquishMail's
 *  SQINFO only crunchy.
 *
 *  Written by Andrew Clarke and released to the public domain.
 *
 *  Version history
 *  ---------------
 *
 *  1.0  1996-04-04  ozzmosis
 *
 *  Initial version.  Not very portable.
 *
 *  1.1  2002-10-17  ozzmosis
 *
 *  Portable to any ISO C platform (in theory).
 *
 *  1.2  2015-03-19  ozzmosis
 *
 *  Change hex style from DEADBEEFh to 0xdeafbeef in output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#define SQHDRID 0xafae4453UL

typedef struct
{
    unsigned short sz_sqbase;        /*   0 */
    unsigned short rsvd1;            /*   2 */
    unsigned long num_msg;           /*   4 */
    unsigned long high_msg;          /*   8 */
    unsigned long skip_msg;          /*  12 */
    unsigned long high_water;        /*  16 */
    unsigned long uid;               /*  20 */
    char base[80];                   /*  24 */
    unsigned long first_frame;       /* 104 */
    unsigned long last_frame;        /* 108 */
    unsigned long first_free_frame;  /* 112 */
    unsigned long last_free_frame;   /* 116 */
    unsigned long end_frame;         /* 120 */
    unsigned long max_msg;           /* 124 */
    unsigned short keep_days;        /* 128 */
    unsigned short sz_sqhdr;         /* 130 */
    unsigned char rsvd2[124];        /* 132 */
}                                    /* 256 */
SQBASE;

typedef struct
{
    unsigned long frame_id;          /*   0 */
    unsigned long next_frame;        /*   4 */
    unsigned long prev_frame;        /*   8 */
    unsigned long frame_len;         /*  12 */
    unsigned long msg_len;           /*  16 */
    unsigned long ctrl_len;          /*  20 */
    unsigned short frame_type;       /*  24 */
    unsigned short rsvd;             /*  26 */
}                                    /*  28 */
SQFRAME;

typedef struct
{
    unsigned long attr;              /*   0 */
    char from[36];                   /*   4 */
    char to[36];                     /*  40 */
    char subj[72];                   /*  76 */
    unsigned short orig_zone;        /* 148 */
    unsigned short orig_net;         /* 150 */
    unsigned short orig_node;        /* 152 */
    unsigned short orig_point;       /* 154 */
    unsigned short dest_zone;        /* 156 */
    unsigned short dest_net;         /* 158 */
    unsigned short dest_node;        /* 160 */
    unsigned short dest_point;       /* 162 */
    unsigned short date_written;     /* 164 */
    unsigned short time_written;     /* 166 */
    unsigned short date_arrived;     /* 168 */
    unsigned short time_arrived;     /* 170 */
    unsigned short utc_ofs;          /* 172 */
    unsigned long replyto;           /* 174 */
    unsigned long see[9];            /* 178 */
    unsigned long umsgid;            /* 214 */
    char ftsc_date[20];              /* 218 */
}                                    /* 238 */
SQXMSG;

#ifdef _NO_VCL

static void pauseOnExit(void)
{
    static int i = 0;

    if (i == 0)
    {
        atexit(pauseOnExit);
        i++;
    }
    else
    {
        printf("\nPress Enter to continue...");
        fflush(stdout);
        getchar();
    }
}

#endif

#define r2ul(x) \
    (unsigned long) ( \
    ((unsigned long) x[3] << 24) | ((unsigned long) x[2] << 16) | \
    ((unsigned long) x[1] << 8) | (unsigned long) x[0])

#define r2us(x) \
    (unsigned short) (((unsigned short) x[1] << 8) | (unsigned short) x[0])

FILE *ifp;

static void get_sqbase(SQBASE *x)
{
    unsigned char tmp4[4], tmp2[2];

    memset(x, 0, sizeof *x);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->sz_sqbase = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->rsvd1 = r2us(tmp2);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->num_msg = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->high_msg = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->skip_msg = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->high_water = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->uid = r2ul(tmp4);

    assert(fread(x->base, sizeof x->base, 1, ifp) == 1);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->first_frame = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->last_frame = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->first_free_frame = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->last_free_frame = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->end_frame = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->max_msg = r2ul(tmp4);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->keep_days = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->sz_sqhdr = r2us(tmp2);

    assert(fread(x->rsvd2, sizeof x->rsvd2, 1, ifp) == 1);
}

static void get_sqframe(SQFRAME *x)
{
    unsigned char tmp4[4], tmp2[2];

    memset(x, 0, sizeof *x);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->frame_id = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->next_frame = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->prev_frame = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->frame_len = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->msg_len = r2ul(tmp4);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->ctrl_len = r2ul(tmp4);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->frame_type = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->rsvd = r2us(tmp2);
}

static void get_sqxmsg(SQXMSG *x)
{
    unsigned char tmp4[4], tmp2[2];
    int i;

    memset(x, 0, sizeof *x);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->attr = r2ul(tmp4);

    assert(fread(x->from, sizeof x->from, 1, ifp) == 1);
    assert(fread(x->to, sizeof x->to, 1, ifp) == 1);
    assert(fread(x->subj, sizeof x->subj, 1, ifp) == 1);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->orig_zone = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->orig_net = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->orig_node = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->orig_point = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->dest_zone = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->dest_net = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->dest_node = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->dest_point = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->date_written = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->time_written = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->date_arrived = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->time_arrived = r2us(tmp2);

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    x->utc_ofs = r2us(tmp2);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->replyto = r2ul(tmp4);

    for (i = 0; i < 9; i++)
    {
        assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
        x->see[i] = r2ul(tmp4);
    }

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    x->umsgid = r2ul(tmp4);

    assert(fread(x->ftsc_date, sizeof x->ftsc_date, 1, ifp) == 1);
}

static void divider(void)
{
    printf("------------------------------------------------------------------------------\n\n");
}

static void dump_sqbase(SQBASE *x)
{
    printf("Dump of SQBASE structure\n");

    divider();

    printf("sz_sqbase      : %hu\n", x->sz_sqbase);
    printf("sz_rsvd1       : %hu\n", x->rsvd1);
    printf("num_msg        : %lu\n", x->num_msg);
    printf("high_msg       : %lu", x->high_msg);

    if (x->high_msg != x->num_msg)
    {
        printf(" (Should be %lu!)", x->num_msg);
    }

    putchar('\n');

    printf("skip_msg       : %lu\n", x->skip_msg);
    printf("high_water     : %lu\n", x->high_water);
    printf("uid            : %lu\n", x->uid);
    printf("base           : %s\n", x->base);
    printf("first_frame    : 0x%08lx (%lu)\n", x->first_frame, x->first_frame);
    printf("last_frame     : 0x%08lx (%lu)\n", x->last_frame, x->last_frame);
    printf("first_free_frm : 0x%08lx (%lu)\n", x->first_free_frame, x->first_free_frame);
    printf("last_free_frm  : 0x%08lx (%lu)\n", x->last_free_frame, x->last_free_frame);
    printf("end_frame      : 0x%08lx (%lu)\n", x->end_frame, x->end_frame);
    printf("max_msg        : %lu\n", x->max_msg);
    printf("sz_sqhdr       : %hu\n", x->sz_sqhdr);
    printf("keep_days      : %hu\n", x->keep_days);
}

static void dump_sqframe(SQFRAME *x)
{
    printf("\n\nDump of SQFRAME structure\n");

    divider();

    printf("frame_id       : 0x%08lx (", x->frame_id);

    if (x->frame_id == SQHDRID)
    {
        printf("OK");
    }
    else
    {
        printf("Should be 0x%08lx!", SQHDRID);
    }

    puts(")\n");

    printf("next_frame     : 0x%08lx (%lu)\n", x->next_frame, x->next_frame);
    printf("prev_frame     : 0x%08lx (%lu)\n", x->prev_frame, x->prev_frame);
    printf("frame_len      : %lu\n", x->frame_len);
    printf("msg_len        : %lu\n", x->msg_len);
    printf("ctrl_len       : %lu\n", x->ctrl_len);
    printf("frame_type     : %hu (", x->frame_type);

    switch (x->frame_type)
    {
    case 0:
        printf("Normal frame");
        break;
    case 1:
        printf("Free frame");
        break;
    case 2:
        printf("LZSS frame");
        break;
    case 3:
        printf("Frame update");
        break;
    default:
        printf("Unknown");
        break;
    }

    puts(")\n");
}

static void dosdate_to_tm(struct tm *x, unsigned short d, unsigned short t)
{
    x->tm_mday = d & 0x1f;
    x->tm_mon = ((d >> 5) & 0x0f) - 1;
    x->tm_year = ((d >> 9) & 0x7f) + 80;

    /* fix for years prior to 1980 */

    if (x->tm_year > 127)
    {
        x->tm_year -= 128;
    }

    x->tm_hour = (t >> 11) & 0x1f;
    x->tm_min = (t >> 5) & 0x3f;
    x->tm_sec = (t & 0x1f) << 1;
}

static void dump_sqxmsg(SQXMSG *x)
{
    struct tm tm_written, tm_arrived;

    dosdate_to_tm(&tm_written, x->date_written, x->time_written);
    dosdate_to_tm(&tm_arrived, x->date_arrived, x->time_arrived);
    
    printf("\n\nDump of SQXMSG structure\n");

    divider();
    
    printf("attr           : 0x%08lx (%lu)\n", x->attr, x->attr);
    printf("orig_user      : \"%s\"\n", x->from);
    printf("orig_addr      : %hu:%hu/%hu.%hu\n", x->orig_zone, x->orig_net, x->orig_node, x->orig_point);
    printf("dest_user      : \"%s\"\n", x->to);
    printf("dest_addr      : %hu:%hu/%hu.%hu\n", x->dest_zone, x->dest_net, x->dest_node, x->dest_point);
    printf("subject        : \"%s\"\n", x->subj);

    printf("date/time writ : %04hXh %04hXh (%04d-%02d-%02d %02d:%02d:%02d)\n",
      x->date_written, x->time_written,
      tm_written.tm_year + 1900, tm_written.tm_mon + 1, tm_written.tm_mday,
      tm_written.tm_hour, tm_written.tm_min, tm_written.tm_sec
    );

    printf("date/time ariv : %04hXh %04hXh (%04d-%02d-%02d %02d:%02d:%02d)\n",
      x->date_arrived, x->time_arrived,
      tm_arrived.tm_year + 1900, tm_arrived.tm_mon + 1, tm_arrived.tm_mday,
      tm_arrived.tm_hour, tm_arrived.tm_min, tm_arrived.tm_sec
    );

    printf("utc_ofs        : %hd\n", (signed short) x->utc_ofs);
    printf("replyto        : %lu\n", x->replyto);
    printf("see            : %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
      x->see[0], x->see[1], x->see[2], x->see[3], x->see[4],
      x->see[5], x->see[6], x->see[7], x->see[8]);
    printf("umsgid         : %lu\n", x->umsgid);
    printf("ftsc_date      : \"%s\"\n", x->ftsc_date);
}

static void traverse_frame_list(unsigned long frame_ofs, char *frame_type)
{
    unsigned long filesize;
    SQFRAME sqf;
    SQXMSG sqx;

    assert(fseek(ifp, 0, SEEK_END) == 0);
    filesize = (unsigned long) ftell(ifp);
    assert((long) filesize != -1L); 

    assert(fseek(ifp, frame_ofs, SEEK_SET) == 0);

    if (frame_ofs == 0)
    {
        printf("\n%s list is empty.\n", frame_type);
        return;
    }

    while (frame_ofs != 0)
    {
        if (frame_ofs > filesize)
        {
            printf("\nFrame offset too high (Offset=0x%08lx Filesize=0x%08lx)\n",
              frame_ofs, filesize);
            return;
        }

        printf("\n\nCurrent frame offset: 0x%08lx (%lu)\n", frame_ofs, frame_ofs);
        assert(fseek(ifp, frame_ofs, SEEK_SET) == 0);
        get_sqframe(&sqf);
        dump_sqframe(&sqf);
        frame_ofs = sqf.next_frame;
        get_sqxmsg(&sqx);
        dump_sqxmsg(&sqx);
    }
}

int main(int argc, char **argv)
{
    SQBASE sqb;

#ifdef _NO_VCL
    pauseOnExit();
#endif

    if (argc != 2)
    {
        fprintf(stderr,
          "Displays information about a Squish message base.\n"
          "\n"
          "usage: squid sqdfile\n"
        );
        return EXIT_FAILURE;
    }

    ifp = fopen(argv[1], "rb");

    if (ifp == NULL)
    {
        fprintf(stderr, "squid: Cannot open `%s` for reading: %s\n", argv[1],
          strerror(errno));
        return EXIT_FAILURE;
    }

    get_sqbase(&sqb);
    dump_sqbase(&sqb);

    traverse_frame_list(sqb.first_frame, "Stored");
    traverse_frame_list(sqb.first_free_frame, "Free");

    fclose(ifp);

    return EXIT_SUCCESS;
}
