# bytediff

This tool is used for sequences in binary files where the differences between consequtive bytes are the same as a user-provided sequence of bytes or a string. You can use it to find a sequence of bytes you know exists within a file - like a word in lower case - but you don't know what the exact corresponding bytes are.

Let's say you wanted to find an ascending pattern of 5 bytes, or a sequences of bytes that may spell out the word "donut". Run bytediff like so:

<pre># bytediff my-file.bin 0x01 0x02 0x03 0x04 0x05 -v
Match at 0x00002d0a | 0x60 0x61 0x62 0x63 0x64
Match at 0x000074d0 | 0x42 0x43 0x44 0x45 0x46

# bytediff homer-simpson.bin -s "donut" -v
Match at 0x00001d10 | 0x04 0x0f 0x0e 0x15 0x14
Match at 0x00001d20 | 0x04 0x0f 0x0e 0x15 0x14
Match at 0x00001d30 | 0x04 0x0f 0x0e 0x15 0x14
Match at 0x00001d40 | 0x04 0x0f 0x0e 0x15 0x14</pre>

You can provide additional flags to change endianness, switch to 16-, 24-, or 32- bit mode, or scale the values of a string so each character is offset by (for example) 0x20 instead of the typical 0x01. In this case, A=0x00, B=0x20, C=0x40, etc.

You can also use this tool to find and match strings using a character map. An example character map for the *Zelda - A Link to The Past* randomizer is provided. The example below reads the ALttPR rom file using the character map provided and prints all recognizable strings found between addresses 0xe00b2 and 0xe51dd:

<pre># bytediff <rom-file> -M alttpr_map.txt -o 0xe00b2 -O 0xe51dd -p
0x000e00b2: "I'M JUST GOING_OUT FOR A PACK|OF SMOKES."
0x000e00db: "I'VE FALLEN_AND I CAN'T|GET UP, TAKE*^THIS."
0x000e0107: "ONLY ADULTS_SHOULD TRAVEL|AT NIGHT."
0x000e012b: "YOU CAN PUSH X_TO SEE THE|MAP."
0x000e014a: "PRESS THE A_BUTTON TO LIFT|THINGS BY YOU."
...</pre>

# Installation
Pretty simple (assuming it compiles):
<pre>make
sudo make install</pre>

# Command-line parameters
Taken from running `bytediff -h`:
<pre>Usage: bytediff [OPTIONS]... FILE REF DIFF1 [DIFF2] ... [DIFFn]

   -b BITS    Read 8-, 16-, 24-, or 32-bit integers based on BITS
   -e         Arrange bytes for little endian (default)
   -E         Arrange bytes for big endian
   -h         Print this help message
   -M FILE    Convert a string supplied by -s to bytes mapped in FILE
              If the -p string is provided, print strings recognized by
              the character map file
   -o OFFSET  Start searching at a specific OFFSET
   -O OFFSET  Stop searching before reaching OFFSET
   -p         If a character map is supplied, print all strings read that
              are found to match. Matching is not attempted so REF, DIFF
              and the -s option are ignored
   -s STRING  Translate a string into a series of bytes to search for
              In this case, REF and DIFF are ignored
   -S SCALE   Multiply all differences by SCALE. This is useful when
              using the -s option
   -v         Show verbose output
   -w         Ignore warnings
   -x         The reference byte must be matched exactly

Examples:
   Check `file.bin' for pattern similar to `abcdjj' or `ABCDJJ':
      bytediff file.bin 1 2 3 4 10 0x0a
   Check piped input for sequence of 4 bytes descending by 10:
      cat file.bin | bytediff -- 0 -10 -20 -30
      * Note: The '--' is required so the negative differences aren't
              interpreted as command line options
   Check `file.bin' for 16-bit matches in the same order of the string
   `castle' with a difference of 32 (0x20) between each character:
      bytediff file.bin -b 16 -s castle -S 0x20
   Print all strings in a file that are recognized by mapping file
   `map.txt' within range 0x4000 and 0x6000:
      bytediff file.bin -M map.txt -o 0x4000 -O 0x6000</pre>
