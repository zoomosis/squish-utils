/*
 *  squ2mbox.c
 *
 *  Converts Squish messagebases to UNIX mbox format.
 *
 *  Written by Andrew Clarke and released to the public domain.
 *
 *
 *  ChangeLog
 *  ---------
 *
 *  1.9  2002-10-30:
 *
 *	Bug fix: squ2mbox was not generating a date in the correct format for
 *	the Date: field.
 *
 *  1.8  2002-10-19:
 *
 *	Strips FidoNet control information (0x01 lines & SEEN-BYs), but not the
 *	Origin line (because it's partly text and partly control information
 *	and we want to know where the message came from...)
 *
 *  1.7  2002-10-17:
 *
 *	Code cleanup. Added ability convert free (deleted) frames (define
 *	CONVERT_FREE_FRAMES and recompile). Generate a new Message-Id for
 *	messages without one.
 *
 *  1.6  2002-10-16:
 *
 *	Minor user interface improvements.
 *
 *  1.5  2002-10-15:
 *
 *	Converted to ISO C. More error checking with assert().
 *
 *  1.4  2002-10-15:
 *
 *	Use the date field from the header instead of the FTSC date field.
 *
 *	Fixes to support dates prior to 1980 (although the SquishMail 1.00
 *	software wasn't released until November 1991...)
 *
 *  1.3  2002-10-15:
 *
 *	Fix to allow compiling with the Borland Free Command-line Tools. If
 *	compiling from the Borland C++ Builder IDE, _NO_VCL will be defined
 *	in the Conditional Defines menu (Project:Options:Directories/Conditionals).
 *
 *  1.2  2002-10-12:
 *
 *	Parses the FTSC date and converts it to a mbox-compatible date.
 *
 *  1.1  2002-10-12:
 *
 *	Some bugs fixed. May still crash on a corrupt message base.
 *
 *  1.0  2002-10-07:
 *
 *	Initial version.
 */

#define PROGRAM "squ2mbox"
#define VERSION "1.9"
#define HOSTNAME "localhost"
#define USERNAME "fidonet"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#ifdef __THINK__
/* Symantec Think C on MacOS */
#include <console.h>
#endif

#define SQHDRID 0xafae4453UL

#define raw2ulong(x) \
    (unsigned long) ( \
    ((unsigned long) x[3] << 24) | ((unsigned long) x[2] << 16) | \
    ((unsigned long) x[1] << 8) | (unsigned long) x[0])

#define raw2ushort(x) \
    (unsigned short) (((unsigned short) x[1] << 8) | (unsigned short) x[0])

FILE *ifp, *ofp;

static int output_ctl_lines = 0;
static int strip_high_bit = 1;

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

static char *gen_msgid(void)
{
    static int MsgIdPfx = 'A';
    static char buf[127];
    static int fakepid = 0;
    time_t now;
    struct tm *tm;

    now = time(NULL);
    tm = gmtime(&now);

    sprintf(buf, "%d%02d%02d%02d%02d%02d.G%c%d@%s",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
        tm->tm_min, tm->tm_sec, (char) MsgIdPfx, fakepid, HOSTNAME);

    fakepid++;

    MsgIdPfx = (MsgIdPfx == 'Z') ? 'A' : MsgIdPfx + 1;

    return buf;
}

static void escape_msgid_reply(char *p)
{
    while (*p != '\0')
    {
        if (*p == '@')
        {
            *p = '#';
        }
        else if (*p == ' ')
        {
            *p = '@';
        }
        p++;
    }
}

static void output_msg_txt(char *str, unsigned long msg_num)
{
    static int got_origin = 0;
    static unsigned long old_msg_num = 0;
    char *p;

    assert(str != NULL);

    if (msg_num != old_msg_num)
    {
        got_origin = 0;
    }

    old_msg_num = msg_num;

    p = str;

    if (*p == '\n')
    {
        p++;
    }

    if (*p != 0x01)
    {
        if (got_origin && strncmp(p, "SEEN-BY: ", 9) == 0)
        {
            return;
        }

        if (msg_num == old_msg_num && strncmp(p, " * Origin: ", 11) == 0)
        {
            got_origin = 1;
        }

        if (strncmp(p, "From ", 5) == 0)
        {
            assert(fputc('>', ofp) != EOF);
        }

        while (*p != '\0')
        {
            if (!strip_high_bit)
            {
                assert(fputc(*p, ofp) != EOF);
            }
            else if ((unsigned char) *p > 0x7e)
            {
                assert(fprintf(ofp, "=%d", (unsigned char) *p) != EOF);
            }
            else
            {
                assert(fputc(*p, ofp) != EOF);
            }

            p++;
        }

        assert(fputc('\n', ofp) != EOF);
    }
}

static void traverse_frame_list(unsigned long frame_ofs, unsigned long total_msgs)
{
    unsigned long next_frame, msg_num;
    unsigned char tmp4[4], tmp2[2];

    next_frame = frame_ofs;

    msg_num = 0;

    while (next_frame != 0)
    {
        unsigned long id, msg_len, ctl_len;
        unsigned short frame_type;
        char *ctl, *new_ctl, *msgid, *reply;
        char from[36], to[36], subject[72], date[27], mboxdate[25];
        unsigned char datewritten[4];
        struct tm tm_msg, *tm_msg_new;
        unsigned short idate, itime;
        time_t msg_time;

        msg_num++;

        printf("%lu/%lu\r", msg_num, total_msgs);

        assert(fseek(ifp, next_frame, SEEK_SET) == 0);

        assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
        id = raw2ulong(tmp4);

        assert(id == SQHDRID);

        assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
        next_frame = raw2ulong(tmp4);

        assert(fseek(ifp, 8, SEEK_CUR) == 0);

        assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
        msg_len = raw2ulong(tmp4);

        assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
        ctl_len = raw2ulong(tmp4);

        assert(fseek(ifp, 2, SEEK_CUR) == 0);

        assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
        frame_type = raw2ushort(tmp2);

        if (frame_type != 0)
        {
            break;
        }

        assert(fseek(ifp, 4, SEEK_CUR) == 0);

        assert(fread(from, sizeof from, 1, ifp) == 1);
        assert(fread(to, sizeof to, 1, ifp) == 1);
        assert(fread(subject, sizeof subject, 1, ifp) == 1);

        assert(fseek(ifp, 16, SEEK_CUR) == 0);

        assert(fread(datewritten, sizeof datewritten, 1, ifp) == 1);

        assert(fseek(ifp, 70, SEEK_CUR) == 0);

        memset(&tm_msg, 0, sizeof tm_msg);

        idate = (unsigned short) (((unsigned short) datewritten[1] << 8) | (unsigned short) datewritten[0]);
        itime = (unsigned short) (((unsigned short) datewritten[3] << 8) | (unsigned short) datewritten[2]);

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

        tm_msg_new = gmtime(&msg_time);

        if (tm_msg_new != NULL)
        {
            /* Thu Oct  3 18:21:13 2002 */
            strftime(mboxdate, sizeof mboxdate, "%a %b %d %H:%M:%S %Y", tm_msg_new);

            /* Thu, 3 Oct 2002 18:21:14 +1000 */
            strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S", tm_msg_new);
        }
        else
        {
            *date = '\0';
            *mboxdate = '\0';
        }

        assert(fprintf(ofp, "From localhost %s\n", mboxdate) != EOF);
        assert(fprintf(ofp, "From: %s <" USERNAME "@" HOSTNAME ">\n", from) != EOF);
        assert(fprintf(ofp, "To: %s <" USERNAME "@" HOSTNAME ">\n", to) != EOF);

        if (*subject != '\0')
        {
            assert(fprintf(ofp, "Subject: %s\n", subject) != EOF);
        }

        if (*date != '\0')
        {
            assert(fprintf(ofp, "Date: %s +0000\n", date) != EOF);
        }

        assert(fprintf(ofp, "Content-Type: text/plain;\n") != EOF);
        assert(fprintf(ofp, "X-Converted-by: %s %s\n", PROGRAM, VERSION) != EOF);

        ctl = NULL;
        new_ctl = NULL;
        msgid = NULL;
        reply = NULL;

        if (ctl_len != 0)
        {
            char *p;
            int ctls;

            ctl = malloc((size_t) ctl_len + 1);
            assert(ctl != NULL);
            assert(fread(ctl, (size_t) ctl_len, 1, ifp) == 1);
            ctl[(size_t) ctl_len] = '\0';

            ctls = 0;
            p = ctl;
            while (*p != '\0')
            {
                if (*p == '\1')
                {
                    ctls++;
                }
                p++;
            }

            new_ctl = malloc((size_t) ctl_len + ctls + 1);
            assert(new_ctl != NULL);
            *new_ctl = '\0';

            p = strtok(ctl, "\1");
            while (p != NULL)
            {
                if (msgid == NULL && strncmp(p, "MSGID: ", 7) == 0)
                {
                    msgid = malloc(strlen(p + 7) + 1);
                    assert(msgid != NULL);
                    strcpy(msgid, p + 7);
                }

                if (reply == NULL && strncmp(p, "REPLY: ", 7) == 0)
                {
                    reply = malloc(strlen(p + 7) + 1);
                    assert(reply != NULL);
                    strcpy(reply, p + 7);
                }

                strcat(new_ctl, "\n");
                strcat(new_ctl, "\1");
                strcat(new_ctl, p);
                p = strtok(NULL, "\1");
            }
        }

        if (msgid != NULL)
        {
            escape_msgid_reply(msgid);
            assert(fprintf(ofp, "Message-ID: <%s>\n", msgid) != EOF);
        }
        else
        {
            assert(fprintf(ofp, "Message-ID: <%s>\n", gen_msgid()) != EOF);
        }

        if (reply != NULL)
        {
            escape_msgid_reply(reply);
            assert(fprintf(ofp, "In-Reply-To: <%s>\n", reply) != EOF);
        }

        if (new_ctl != NULL)
        {
            if (output_ctl_lines)
            {
                assert(fprintf(ofp, "%s", new_ctl) != EOF);
            }
        }

        if (reply != NULL)
        {
            free(reply);
        }

        if (msgid != NULL)
        {
            free(msgid);
        }

        if (new_ctl != NULL)
        {
            free(new_ctl);
        }

        if (ctl != NULL)
        {
            free(ctl);
        }
        else
        {
            assert(fputc('\n', ofp) != EOF);
        }

        assert(fputc('\n', ofp) != EOF);

        if (msg_len != 0)
        {
            msg_len -= (ctl_len + 238);
        }

        if (msg_len == 0)
        {
            assert(fputc('\n', ofp) != EOF);
        }
        else
        {
            char *txt, *p, *q;

            txt = malloc((size_t) msg_len + 1);
            assert(txt != NULL);
            assert(fread(txt, (size_t) msg_len, 1, ifp) == 1);
            txt[(size_t) msg_len] = '\0';

            p = txt;

            q = strchr(txt, '\r');

            while (q != NULL)
            {
                char *str;

                *q = '\0';

                str = malloc(strlen(p) + 1);
                assert(str != NULL);
                strcpy(str, p);

                output_msg_txt(str, msg_num);

                free(str);

                p = q + 1;
                q = strchr(p, '\r');
            }

            if (txt != NULL)
            {
                free(txt);
            }

            assert(fputc('\n', ofp) != EOF);
        }
    }
}

static void get_sqbase(void)
{
    unsigned short sz_sqbase;
    unsigned char tmp4[4], tmp2[2];
    unsigned long total_msgs, first_frame;

#ifdef CONVERT_FREE_FRAMES
    unsigned long first_free_frame;
#endif

    assert(fread(tmp2, sizeof tmp2, 1, ifp) == 1);
    sz_sqbase = raw2ushort(tmp2);

    assert(sz_sqbase == 256);

    assert(fseek(ifp, 2, SEEK_CUR) == 0);

    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    total_msgs = raw2ulong(tmp4);

    assert(fseek(ifp, 104, SEEK_SET) == 0);
    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    first_frame = raw2ulong(tmp4);

#ifdef CONVERT_FREE_FRAMES
    assert(fseek(ifp, 112, SEEK_SET) == 0);
    assert(fread(tmp4, sizeof tmp4, 1, ifp) == 1);
    first_free_frame = raw2ulong(tmp4);
#endif

    traverse_frame_list(first_frame, total_msgs);

#ifdef CONVERT_FREE_FRAMES
    if (first_frame != 0 && first_free_frame != 0)
    {
        putchar('\n');
    }

    traverse_frame_list(first_free_frame, 0);
#endif
}

int main(int argc, char **argv)
{
#ifdef __THINK__
    argc = ccommand(&argv);
#endif

#ifdef PAUSE_ON_EXIT
    pauseOnExit();
#endif

    if (argc != 3)
    {
        fprintf(
          stderr,
          PROGRAM " " VERSION "\n"
          "\n"
          "Converts Squish messagebases to UNIX mbox format.\n"
          "Written by Andrew Clarke and released to the public domain.\n"
          "\n"
          "Usage: " PROGRAM " sqdfile mboxfile\n"
        );
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], argv[2]) == 0)
    {
        fprintf(
           stderr,
           "Source and destination filenames are identical, `%s`, aborting...\n",
           argv[1]
        );
        return EXIT_FAILURE;
    }

    ifp = fopen(argv[1], "rb");

    if (ifp == NULL)
    {
        fprintf(stderr, PROGRAM ": Cannot open `%s` for reading: %s", argv[1],
          strerror(errno));
        return EXIT_FAILURE;
    }

    ofp = fopen(argv[2], "wb");

    if (ofp == NULL)
    {
        fprintf(stderr, PROGRAM ": Cannot open `%s` for writing: %s", argv[2],
          strerror(errno));
        return EXIT_FAILURE;
    }

    printf(
      PROGRAM ": Converting Squish message base to mbox format ...\n"
      "Input: %s  Output: %s\n",
      argv[1], argv[2]);

    get_sqbase();

    fclose(ofp);
    fclose(ifp);

    puts("\n" "Finished.");

    return 0;
}
