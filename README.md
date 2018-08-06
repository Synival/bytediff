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
