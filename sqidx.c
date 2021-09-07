/*
 *  sqidx.c
 *
 *  Create an index of messages in a Squish message base in CSV format.
 *
 *  Written by Andrew Clarke and released to the public domain.
 *
 *
 *  Messages are listed in reverse order (newest to oldest), with the
 *  following fields:
 *
 *  Offset, Hash, FromName, ToName, Subject, DateTime
 *
 *  Offset: the offset in bytes to the beginning of the message frame
 *  Hash: a unique hex value of the hashed DateTime followed by the hashed FromName + ToName + Subject
 *  FromName: who wrote the message
 *  ToName: who the message is for
 *  Subject: the message subject
 *  DateTime: when the message was written in YYYY-mm-dd HH:MM:SS format
 *
 *  Example:
 *
 *  "4346","7e9aebf9c3a91be0e9104abd01d65","Josh Lewis","Jeff Roule","Help me","1996-06-23 01:21:20"
 *  "2498","7e9892d5506e99d85662b2ee464441be","Neil Walker","Richard Lionheart","help","1996-06-19 11:36:02"
 *  "256","7e9832a687c789c246ef9e25204ca286","Francois Blais","Leo V. Mironoff","Finished product?","1996-06-15 16:35:46"
 */

#define PROGRAM "sqidx"
#define VERSION "1.3"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#define SQHDRID 0xafae4453UL

#define raw2ulong(x) \
    (unsigned long) ( \
    ((unsigned long) x[3] << 24) | ((unsigned long) x[2] << 16) | \
    ((unsigned long) x[1] << 8) | (unsigned long) x[0])

#define raw2ushort(x) \
    (unsigned short) (((unsigned short) x[1] << 8) | (unsigned short) x[0])

static FILE *ifp;

#ifdef PAUSE_ON_EXIT

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

/*  strhash():
 *
 *  http://www.cs.berkeley.edu/~smcpeak/elkhound/sources/smbase/strhash.cc
 *
 *  Adapted from glib's g_str_hash().
 *  Investigation by Karl Nelson <kenelson@ece.ucdavis.edu>.
 *  Do a web search for "g_str_hash X31_HASH" if you want to know more.
 *  update: this is the same function as that described in Kernighan and Pike,
 *  "The Practice of Programming", section 2.9
 */

static unsigned long strhash(char * key)
{
    unsigned long h;

    h = 0;

    if (key == NULL)
    {
    	return h;
    }

    while (*key != '\0')
    {
        h = (h << 5) - h + *key;  /* hash * 31 + *key */
    	key++;
    }

    return h;
}

static char *escape_quotes(char *dest, const char *src)
{
    while (*src != '\0')
    {
        *dest = *src;

        if (*src == '\"')
        {
            dest++;
            *dest = '\"';
        }
        else if (*src == '\\')
        {
            dest++;
            *dest = '\\';
        }

    	dest++;
        src++;
    }

    *dest = '\0';

    return dest;
}

static void traverse_frame_list(unsigned long frame_ofs)
{
    unsigned char tmp4[4], tmp2[2];

    while (frame_ofs != 0)
    {
        unsigned long id;
        unsigned long prev_frame;
        unsigned short frame_type;
        char from[37], to[37], subject[73], date[50], hash[80];
        char escfrom[80], escto[80], escsubject[160];
        unsigned char datewritten[4];
        struct tm tm_msg, *tm_msg_new;
        unsigned short idate, itime;
        time_t msg_time;

        assert(fseek(ifp, frame_ofs, SEEK_SET) == 0);

        /* validate this is a real frame */

        assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
        id = raw2ulong(tmp4);
        assert(id == SQHDRID);

        /* get offset to the previous frame */

        assert(fseek(ifp, frame_ofs + 8, SEEK_SET) == 0);
        assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
        prev_frame = raw2ulong(tmp4);

        /* if this isn't a normal message frame (eg. it has been deleted),
           skip over it */

        assert(fseek(ifp, frame_ofs + 24, SEEK_SET) == 0);
        assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
        frame_type = raw2ushort(tmp2);

        if (frame_type == 0)
        {
            /* get from/to/subject lines */

            from[sizeof from - 1] = '\0';
            to[sizeof to - 1] = '\0';
            subject[sizeof subject - 1] = '\0';

            assert(fseek(ifp, frame_ofs + 28 + 4, SEEK_SET) == 0);
            assert(fread(from, sizeof from - 1, 1, ifp) == 1);
            assert(fread(to, sizeof to - 1, 1, ifp) == 1);
            assert(fread(subject, sizeof subject - 1, 1, ifp) == 1);

            /* get time + date the message was written */

            assert(fseek(ifp, frame_ofs + 28 + 164, SEEK_SET) == 0);
            assert(fread(datewritten, sizeof datewritten, 1, ifp) == 1);

            memset(&tm_msg, 0, sizeof tm_msg);

            idate = (unsigned short)(((unsigned short)datewritten[1] << 8) | (unsigned short)datewritten[0]);
            itime = (unsigned short)(((unsigned short)datewritten[3] << 8) | (unsigned short)datewritten[2]);

            tm_msg.tm_mday = idate & 0x1f;
            tm_msg.tm_mon = ((idate >> 5) & 0x0f) - 1;
            tm_msg.tm_year = ((idate >> 9) & 0x7f) + 80;

            /* fix for years prior to 1980 */

            if (tm_msg.tm_year > 127)
            {
                tm_msg.tm_year -= 128;
            }

            tm_msg.tm_hour = (itime >> 11) & 0x1f;
            tm_msg.tm_min = (itime >> 5) & 0x3f;
            tm_msg.tm_sec = (itime & 0x1f) << 1;

            msg_time = mktime(&tm_msg);

            if (msg_time == -1)
            {
                msg_time = 0;
            }

            tm_msg_new = localtime(&msg_time);

            if (tm_msg_new != NULL)
            {
                strftime(date, sizeof date, "%Y-%m-%d %H:%M:%S", tm_msg_new);
            }
            else
            {
                *date = '\0';
            }

            /* calculate hashes of the date & (from + to + subject) */

            sprintf(hash, "%08lx%08lx", strhash(date), strhash(from) + strhash(to) + strhash(subject));

            /* "FrameOfs","Hash","From","To","Subject","Date" */

            escape_quotes(escfrom, from);
            escape_quotes(escto, to);
            escape_quotes(escsubject, subject);

            printf(
              "\"" "%lu" "\","
              "\"" "%s" "\","
              "\"" "%s" "\","
              "\"" "%s" "\","
              "\"" "%s" "\","
              "\"" "%s" "\""
              "\n", frame_ofs, hash, escfrom, escto, escsubject, date
            );

        }
        frame_ofs = prev_frame;
    }
}

static void get_sqbase(char *base)
{
    char sqd_fn[250];
    unsigned short sz_sqbase;
    unsigned char tmp4[4], tmp2[2];
    unsigned long frame_ofs;

    strcpy(sqd_fn, base);
    strcat(sqd_fn, ".sqd");

    ifp = fopen(sqd_fn, "rb");

    if (ifp == NULL)
    {
        fprintf(stderr, PROGRAM ": Cannot open `%s` for reading: %s", sqd_fn, strerror(errno));
        return;
    }

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    sz_sqbase = raw2ushort(tmp2);
    assert(sz_sqbase == 256);

    /* get offset to the last message frame in the base */

    assert(fseek(ifp, 108, SEEK_SET) == 0);
    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    frame_ofs = raw2ulong(tmp4);

    /* traverse backwards through each frame in the base */

    traverse_frame_list(frame_ofs);

    fclose(ifp);
}

int main(int argc, char **argv)
{
#ifdef PAUSE_ON_EXIT
    pauseOnExit();
#endif

    if (argc != 2)
    {
        fprintf(
          stderr,
          PROGRAM " " VERSION "\n"
          "\n"
          "Create indexes from a Squish message base.\n"
          "Written in 2003 by Andrew Clarke and released to the public domain.\n"
          "\n" "Usage: " PROGRAM " base\n"
        );

        return EXIT_FAILURE;
    }

    argv++;

    get_sqbase(*argv);

    return 0;
}
