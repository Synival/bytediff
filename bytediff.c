#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PIN  program_invocation_name
#define PISN program_invocation_short_name

typedef long int          bigint;
typedef unsigned long int ubigint;

typedef struct _bytediff_t bytediff_t;
typedef struct _charmap_t charmap_t;

struct _bytediff_t {
    size_t  offset, diff_pos;
    ubigint first_ubyte, *umatch;
    bigint  first_sbyte, *smatch;
    bytediff_t *prev, *next;
};

struct _charmap_t {
    bigint index;
    int ch;
    charmap_t *prev, *next;
};

void bytediff_free(bytediff_t **top, bytediff_t *bd) {
    if (bd->umatch) free(bd->umatch);
    if (bd->smatch) free(bd->smatch);
    if (bd->prev) bd->prev->next = bd->next;
    else          *top           = bd->next;
    if (bd->next) bd->next->prev = bd->prev;
    free(bd);
}

charmap_t *charmap_get_by_index(charmap_t *top, bigint index) {
    for (; top != NULL; top = top->next)
        if (top->index == index)
            return top;
    return NULL;
}

charmap_t *charmap_get_by_ch(charmap_t *top, int ch) {
    for (; top != NULL; top = top->next)
        if (top->ch == ch)
            return top;
    return NULL;
}

bigint charmap_get_index(charmap_t *top, int ch) {
    if (top == NULL)
        return ch;
    top = charmap_get_by_ch(top, ch);
    return (top == NULL) ? (bigint) -1 : top->index;
}

int charmap_get_ch(charmap_t *top, bigint index) {
    if (top == NULL)
        return (int) index;
    top = charmap_get_by_index(top, index);
    return (top == NULL) ? -1 : top->ch;
}

void charmap_free(charmap_t **top, charmap_t *cm) {
    if (cm->prev) cm->prev->next = cm->next;
    else          *top           = cm->next;
    if (cm->next) cm->next->prev = cm->prev;
    free(cm);
}

void print_options(int detailed) {
    fprintf(stderr,
        "Usage: %s [OPTIONS]... FILE REF DIFF1 [DIFF2] ... [DIFFn]\n", PIN);

    if (!detailed) {
        fprintf(stderr,
            "Run with -h option for a list of options and examples.\n");
    }
    else {
        fprintf(stderr, "\n"
"   -b BITS    Read 8-, 16-, 24-, or 32-bit integers based on BITS\n"
"   -e         Arrange bytes for little endian (default)\n"
"   -E         Arrange bytes for big endian\n"
"   -h         Print this help message\n"
"   -M FILE    Convert a string supplied by -s to bytes mapped in FILE\n"
"              If the -p string is provided, print strings recognized by\n"
"              the character map file\n"
"   -o OFFSET  Start searching at a specific OFFSET\n"
"   -O OFFSET  Stop searching before reaching OFFSET\n"
"   -p         If a character map is supplied, print all strings read that\n"
"              are found to match. Matching is not attempted so REF, DIFF\n"
"              and the -s option are ignored\n"
"   -s STRING  Translate a string into a series of bytes to search for\n"
"              In this case, REF and DIFF are ignored\n"
"   -S SCALE   Multiply all differences by SCALE. This is useful when\n"
"              using the -s option\n"
"   -v         Show verbose output\n"
"   -w         Ignore warnings\n"
"   -x         The reference byte must be matched exactly\n");
        fprintf(stderr, "\n"
"Examples:\n"
"   Check `file.bin' for pattern similar to `abcdjj' or `ABCDJJ':\n"
"      %s file.bin 1 2 3 4 10 0x0a\n"
"   Check piped input for sequence of 4 bytes descending by 10:\n"
"      cat file.bin | %s -- 0 -10 -20 -30\n"
"   Check `file.bin' for 16-bit matches in the same order of the string\n"
"   `castle' with a difference of 32 (0x20) between each character:\n"
"      %s file.bin -b 16 -s castle -S 0x20\n"
"   Print all strings in a file that are recognized by mapping file\n"
"   `map.txt' within range 0x4000 and 0x6000:\n"
"      %s file.bin -M map.txt -o 0x4000 -O 0x6000\n\n"
            , PIN, PIN, PIN, PIN);
    }
}

int main(int argc, char **argv) {
    /* command-line options. */
    size_t initial_offset = 0, max_offset = 0;
    unsigned int scale = 1, first_arg = 0, diff_sbytes = 0;
    uint8_t endianness = 0, verbose = 0, output_mode = 0, exact = 0,
        close_input = 0, piped_input = 0, ignore_warnings = 0,
        read_size = 1;
    char *diffstr = NULL, *charmap_fname = NULL;

    /* all variables. */
    bytediff_t *bytediffs = NULL, *bd, *bd_next;
    charmap_t *charmap = NULL, *cm;
    FILE *input, *charmap_file;
    bigint i, int_sbyte, ref_sbyte, *diff_sbyte;
    ubigint int_ubyte, charmap_limit;
    size_t offset, bytes_read, len;
    int last_ubyte, last_sbyte, c;
    unsigned int line_num, output_len;
    uint8_t ubyte[4];
    char *line, *left, *space, *right, output_buf[65536], buf[16];

    /* if no arguments are supplied, print a usage string. */
    piped_input = !isatty(fileno(stdin));
    if (argc == 1 && !piped_input) {
        print_options(0);
        return 1;
    }

    /* read options. */
    do {
        c = getopt(argc, argv, "b:eEhM:o:O:ps:S:vwx");
        if (c == -1)
            break;
        if (optopt != 0)
            return 1;
        switch (c) {
            case 'b':
                     if (strcmp(optarg,  "8") == 0) { read_size=1; }
                else if (strcmp(optarg, "16") == 0) { read_size=2; }
                else if (strcmp(optarg, "24") == 0) { read_size=3; }
                else if (strcmp(optarg, "32") == 0) { read_size=4; }
                else {
                    fprintf(stderr, "%s: invalid bit size of `%s'. valid "
                        "options are 8, 16, 24 or 32\n", PIN, optarg);
                    return 1;
                }
                break;
            case 'e': endianness       = 0; break;
            case 'E': endianness       = 1; break;
            case 'h': print_options(1); return 0;
            case 'M': charmap_fname    = optarg; break;
            case 'o': initial_offset   = strtol(optarg, NULL, 0); break;
            case 'O': max_offset       = strtol(optarg, NULL, 0); break;
            case 'p': output_mode     ^= 1; break;
            case 's': diffstr          = optarg; break;
            case 'S': scale            = strtol(optarg, NULL, 0); break;
            case 'v': verbose         ^= 1; break;
            case 'w': ignore_warnings ^= 1; break;
            case 'x': exact           ^= 1; break;
            default:  abort();
        }
    } while(1);

    /* get our input. if stdin is a tty, let's use a filename instead. */
    first_arg = optind;
    if (piped_input) {
        input = stdin;
        close_input = 0;
    }
    else {
        if ((int) first_arg >= argc) {
            fprintf(stderr, "%s: missing input file\n", PIN);
            return 1;
        }
        input = NULL;
        close_input = 1;
        first_arg++;
    }

    /* make sure we have enough other arguments. */
    if (output_mode == 1) {
        if (charmap_fname == NULL) {
            fprintf(stderr, "%s: the -p option requires a character "
                "mapping provided by -M\n", PIN);
            return 1;
        }
    }
    else {
        if (diffstr == NULL) {
            if ((int) first_arg >= argc) {
                fprintf(stderr, "%s: missing reference byte\n", PIN);
                return 1;
            }
            if ((int) first_arg + 1 >= argc) {
                fprintf(stderr, "%s: missing difference byte(s)\n", PIN);
                return 1;
            }
        }
        else if (strlen(diffstr) < 2) {
            fprintf(stderr, "%s: diff string from `-s' "
                "must be at least 2 characters\n", PIN);
            return 1;
        }
    }

    /* are we opening a file? */
    if (close_input) {
        input = fopen(argv[first_arg - 1], "r");
        if (input == NULL) {
            fprintf(stderr, "%s: couldn't open file `%s' -- %s\n", PIN,
                argv[first_arg - 1], strerror(errno));
            return 1;
        }
        fseek(input, initial_offset, SEEK_SET);
    }
    else {
        for (i = 0; i < (int) initial_offset; i++)
            fread(&c, sizeof(char), 1, input);
    }

    /* are we using a character map? */
    charmap_limit = 256;
    for (i = 1; i < read_size; i++)
        charmap_limit <<= 8;
    if (charmap_fname != NULL) {
        /* attempt to open the file. */
        charmap_file = fopen(charmap_fname, "r");
        if (charmap_file == NULL) {
            fprintf(stderr, "%s: couldn't open character map file `%s' "
                "-- %s\n", PIN, charmap_fname, strerror(errno));
            return 1;
        }

        /* set the map line-by-line with format:
         *    0 space
         *    1 a
         *    2 b
         *    64 special
         *    ...
         * or:
         *    0x00 space
         *    0x01 a
         *    0x02 b
         *    0x40 special
         */
        line = NULL;
        len = 0;
        line_num = 0;
        do {
            /* get our line and remove \r + \n. skip if blank or a comment
             * (starts with #). */
            line_num++;
            c = getline(&line, &len, charmap_file);
            if (c < 0)
                break;
            for (i = strlen(line) - 1; i >= 0; i--) {
                if (!(line[i] == '\n' || line[i] == '\r'))
                    break;
                line[i] = '\0';
            }
            left = line;
            while (*left == ' ')
                left++;
            if (*left == '\0' || *left == '#')
                continue;

            /* make sure the line can be split as '[left] [right]'. */
            if ((space = strchr(left, ' ')) == NULL)
                right = "";
            else {
                *space = '\0';
                for (right = space + 1; *right == ' '; right++);
            }
            if (*right == '\0') {
                fprintf(stderr, "%s: badly-formatted line in character map "
                    "`%s' on line %d\n", PIN, line, line_num);
                return 1;
            }

            /* make sure our mapping value is valid. */
            if (strcmp(right, "space") == 0)
                c = ' ';
            else if (strcmp(right, "special") == 0)
                c = -2;
            else if (strlen(right) == 1)
                c = right[0];
            else {
                fprintf(stderr, "%s: invalid mapping value `%s' on line "
                    "%d -- must be `space', `skip', or a single character\n",
                    PIN, right, line_num);
                return 1;
            }

            /* make sure the mapping index is valid. */
            int_sbyte = strtol(left, NULL, 0);
            if (int_sbyte < 0 || int_sbyte >= (bigint) charmap_limit) {
                fprintf(stderr, "%s: mapping index `%ld' on line %d "
                    "out of range -- must be from 0 to %lu\n", PIN,
                    int_sbyte, line_num, charmap_limit);
                return 1;
            }

            /* typically we want unique mappings, so print warnings. */
            if (!ignore_warnings) {
                if (c >= 0 && (cm = charmap_get_by_ch(charmap, c)) != NULL) {
                    fprintf(stderr, "warning: mapping value `%c' on line %u "
                        "is already assigned to index `%ld'\n",
                        (char) cm->ch, line_num, cm->index);
                }
                if (int_sbyte >= 0 && (cm = charmap_get_by_index(
                    charmap, int_sbyte)) != NULL)
                {
                    fprintf(stderr, "warning: mapping index `%ld' on line %u "
                        "is already assigned to value `%c'\n",
                        cm->index, line_num, (char) cm->ch);
                }
            }

            /* assign! */
            cm = calloc(1, sizeof(charmap_t));
            cm->ch    = c;
            cm->index = int_sbyte;
            cm->next  = charmap;
            if (cm->next)
                cm->next->prev = cm;
            charmap = cm;
        } while(1);
        if (line != NULL)
            free(line);
        fclose(charmap_file);
    }

    /* if ref/diff bytes are ignored, use placeholders for saftey. */
    if (output_mode == 1) {
        ref_sbyte   = 0;
        diff_sbytes = 1;
        diff_sbyte  = calloc(1, sizeof(bigint));
    }
    /* get our reference byte and difference bytes. */
    else if (diffstr == NULL) {
        ref_sbyte   = strtol(argv[first_arg], NULL, 0);
        diff_sbytes = argc - first_arg - 1;
        diff_sbyte = malloc(sizeof(bigint) * diff_sbytes);
        for (i = 0; i < (int) diff_sbytes; i++)
            diff_sbyte[i] = strtol(argv[first_arg + i + 1], NULL, 0)
                - ref_sbyte;
    }
    else {
        /* make sure our string is properly mapped. */
        for (i = 0; diffstr[i] != '\0'; i++) {
            c = charmap_get_index(charmap, diffstr[i]);
            if (c == -1) {
                fprintf(stderr, "%s: unmapped difference string character "
                    "`%c'\n", PIN, diffstr[i]);
                return 1;
            }
        }

        /* build reference byte and difference bytes. */
        ref_sbyte = charmap_get_index(charmap, diffstr[0]);
        diff_sbytes = strlen(diffstr) - 1;
        diff_sbyte = malloc(sizeof(bigint) * diff_sbytes);
        for (i = 0; i < (int) diff_sbytes; i++)
            diff_sbyte[i] = charmap_get_index(charmap, diffstr[i + 1])
                - ref_sbyte;
    }

    /* apply scaling if applicable. */
    if (scale != 1) {
        ref_sbyte *= scale;
        for (i = 0; i < (int) diff_sbytes; i++)
            diff_sbyte[i] *= scale;
    }

    /* some initialization. */
    bytes_read    = 0;
    last_sbyte    = 0;
    last_ubyte    = 0;
    offset        = initial_offset;
    output_buf[0] = '\0';
    output_len    = 0;

    /* start reading our file, checking for matches. */
    do {
        c = fread(ubyte, sizeof(uint8_t), read_size, input);
        if (c < (int) read_size)
            break;

        /* convert our raw data to proper sign and endianness. */
        bytes_read += c;
        int_ubyte = 0;
        for (i = 0; i < (int) read_size; i++) {
            if (endianness == 1) {
                int_ubyte <<= 8;
                int_ubyte |= ubyte[i];
            }
            else
                int_ubyte |= (ubyte[i] << (8 * i));
        }
        int_sbyte = int_ubyte;
        if (int_sbyte >= ((bigint) 1 << (read_size * 8 - 1)))
            int_sbyte -= ((bigint) 1 << (read_size * 8));

        /* if -p and -M are provided, print matching strings! */
        if (output_mode == 1) {
            cm = charmap_get_by_index(charmap, int_ubyte);
            if (cm == NULL) {
                if (output_len > 0)
                    printf("%s\"\n", output_buf);
                output_buf[0] = '\0';
                output_len = 0;
            }
            else if (output_len < (int) sizeof(output_buf)) {
                if (output_len == 0) {
                    output_buf[0] = '\0';
                    output_len = snprintf(output_buf, sizeof(output_buf),
                        "0x%08x: \"", (unsigned int) offset);
                }
                if (cm->ch < ' ' || cm->ch > '~')
                    snprintf(buf, sizeof(buf), "[%0*x]", read_size * 2,
                        (unsigned int) cm->index);
                else {
                    buf[0] = cm->ch;
                    buf[1] = '\0';
                }
                output_len += snprintf(output_buf + output_len,
                    sizeof(output_buf) - output_len, "%s", buf);
            }
        }

        /* perform matching. */
        if (output_mode == 0 && bytes_read >= (read_size * 2)) {
            /* for current match candidates, check for removal. */
            for (bd = bytediffs; bd != NULL; bd = bd_next) {
                bd_next = bd->next;
                if (!(bd->first_ubyte + diff_sbyte[bd->diff_pos] == int_ubyte
                   || bd->first_sbyte + diff_sbyte[bd->diff_pos] == int_sbyte))
                {
                    bytediff_free(&bytediffs, bd);
                    continue;
                }
                bd->diff_pos++;
                bd->umatch[bd->diff_pos] = int_ubyte;
                bd->smatch[bd->diff_pos] = int_sbyte;
            }

            /* add new match candidates if two consequtive bytes are valid. */
            if ((last_ubyte + diff_sbyte[0] == (bigint) int_ubyte ||
                 last_sbyte + diff_sbyte[0] == int_sbyte) &&
                (!exact || last_ubyte == ref_sbyte))
            {
                bd = calloc(1, sizeof(bytediff_t));
                bd->diff_pos    = 1;
                bd->offset      = offset - read_size;
                bd->first_ubyte = last_ubyte;
                bd->first_sbyte = last_sbyte;

                bd->umatch = calloc(diff_sbytes + 1, sizeof(ubigint));
                bd->umatch[0] = last_ubyte;
                bd->umatch[1] = int_ubyte;

                bd->smatch = calloc(diff_sbytes + 1, sizeof(bigint));
                bd->smatch[0] = last_sbyte;
                bd->smatch[1] = int_sbyte;

                bd->next = bytediffs;
                if (bd->next)
                    bd->next->prev = bd;
                bytediffs = bd;
            }

            /* output and remove completed matches. */
            for (bd = bytediffs; bd != NULL; bd = bd_next) {
                bd_next = bd->next;
                if (bd->diff_pos >= diff_sbytes) {
                    if (!verbose)
                        printf("0x%08x\n", (unsigned int) bd->offset);
                    else {
                        printf("Match at 0x%08x", (unsigned int) bd->offset);
                        for (i = 0; i < (int) diff_sbytes + 1; i++)
                            printf("%s0x%0*x", (i == 0) ? " | " : " ",
                                read_size * 2, (unsigned int) bd->umatch[i]);
                        printf("\n");
                        bytediff_free(&bytediffs, bd);
                    }
                }
            }
        }

        /* keep track of our previous byte and bail if we've reached the
         * output limit. */
        last_sbyte = int_sbyte;
        last_ubyte = int_ubyte;
        offset += c;
        if (max_offset > 0 && offset + read_size > max_offset)
            break;
    } while(1);

    /* print any output that may have been built but not printed. */
    if (output_len > 0)
        printf("%s\"\n", output_buf);

    /* free all allocated memory. */
    if (diff_sbyte) free(diff_sbyte);
    while (bytediffs != NULL)
        bytediff_free(&bytediffs, bytediffs);
    while (charmap != NULL)
        charmap_free(&charmap, charmap);

    /* close input if we fopen()'d a file. */
    if (close_input)
        fclose(input);
    return 0;
}
