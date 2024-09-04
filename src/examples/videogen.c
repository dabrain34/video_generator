#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <video_generator.h>
#ifndef _WIN32
#include "getopt_long.h"
#endif
/* ----------------------------------------------------------------------------------- */

static video_generator_settings cfg;
static uint32_t max_frames;

#ifndef _WIN32
void usage(char *progname) {
    printf("Usage: %s [options...]\n", progname);
    printf("  or:  %s [options...]\n", progname);

    printf("\n");
    printf("Mandatory arguments to long options are mandatory for short options too.\n");

    printf("\n");
    printf("Options:\n");
    printf("    -h, --help          show this help\n");
    printf("    -W, --width         width\n");
    printf("    -H, --height        height\n");
    printf("    -n, --max-frames    height\n");
    printf("    -F, --fps           fps\n");
    printf("    -f, --format        format\n");
    printf("    -b, --bitdepth      bitdepth\n");
}

int parse_options(int argc, char **argv) {
    // prevent unrecognised arguments from being shunted to the audio driver
    setenv("POSIXLY_CORRECT", "", 1);

    static struct option long_options[] = {
        {"help",      no_argument,        NULL, 'h'},
        {"daemon",    no_argument,        NULL, 'd'},
        {"width",     required_argument,  NULL, 'W'},
        {"height",    required_argument,  NULL, 'H'},
        {"max-frames",required_argument,  NULL, 'n'},
        {"fps",       required_argument,  NULL, 'f'},
        {"format",    required_argument,  NULL, 'F'},
        {"bitdepth",  required_argument,  NULL, 'b'},
        {NULL,        0,                  NULL,   0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv,
                              "+hW:H:M:f:F:b:",
                              long_options, NULL)) > 0) {
        switch (opt) {
            default:
                usage(argv[0]);
                exit(1);
            case 'h':
                usage(argv[0]);
                exit(0);
            case 'W':
                cfg.width = atoi(optarg);
                break;
            case 'H':
                cfg.height = atoi(optarg);
                break;
            case 'f':
                cfg.fps = atoi(optarg);
                break;
            case 'F':
                cfg.format = atoi(optarg);
                break;
            case 'b':
                cfg.bitdepth = atoi(optarg);
                break;
            case 'M':
                max_frames = atoi(optarg);
                break;
        }
    }
    return optind;
}
#endif

int main(int argc, char* argv[]) {

  FILE* video_fp = NULL;

  video_generator gen;

  video_fp = fopen("output.yuv", "wb");

  cfg.width = 720;
  cfg.height = 480;
  cfg.fps = 3;
  max_frames = 30;
  cfg.format = 420;
  cfg.bitdepth = 8;

#ifndef _WIN32
  parse_options(argc, argv);
#endif

  if (0 != video_generator_init(&cfg, &gen)) {
    printf("Error: cannot initialize the generator.\n");
    exit(1);
  }

  while(1) {

    printf("Frame: %zu\n", gen.frame);

    video_generator_update(&gen);

    // write video planes to a file
    fwrite((char*)gen.y, gen.ybytes, 1,  video_fp);
    fwrite((char*)gen.u, gen.ubytes, 1, video_fp);
    fwrite((char*)gen.v, gen.vbytes, 1, video_fp);

    if (gen.frame > max_frames) {
      break;
    }

  }

fclose(video_fp);

video_generator_clear(&gen);
}

/* ----------------------------------------------------------------------------------- */



