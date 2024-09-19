/* VideoGenerator
 * Copyright (C) 2024 Igalia, S.L.
 *     Author: Stephane Cerveau <scerveau@igalia.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You
 * may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <video_generator.h>
#include <video_convert.h>
#ifndef _WIN32
#include "getopt_long.h"
#endif
/* ----------------------------------------------------------------------------------- */

static video_generator_settings cfg;
static uint32_t max_frames;
static char* filename;
#define DEFAULT_FILENAME "output.yuv"

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
    printf("    -n, --max-frames    max frames\n");
    printf("    -F, --fps           fps\n");
    printf("    -f, --format        format\n");
    printf("    -b, --bitdepth      bitdepth\n");
    printf("    -o, --output        filename, default " DEFAULT_FILENAME "\n");
}

int parse_options(int argc, char **argv) {
    // prevent unrecognised arguments from being shunted to the audio driver
    setenv("POSIXLY_CORRECT", "", 1);

    static struct option long_options[] = {
        {"help",      no_argument,        NULL, 'h'},
        {"output",    required_argument,  NULL, 'o'},
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
                              "+hW:H:n:f:F:b:o:",
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
            case 'n':
                max_frames = atoi(optarg);
            case 'o':
                free(filename);
                filename = (char*)malloc(strlen(optarg) + 1);
                strcpy (filename,optarg);
                break;
        }
    }
    return optind;
}
#endif

int main(int argc, char* argv[]) {

  FILE* video_fp = NULL;
  filename = (char*)malloc(strlen(DEFAULT_FILENAME) + 1);
  strcpy (filename,DEFAULT_FILENAME);
  int res;

  video_generator gen;

  video_fp = fopen("output.yuv", "wb");

  cfg.width = 720;
  cfg.height = 480;
  max_frames = 30;
  cfg.format = 420;
  cfg.bitdepth = 8;

#ifndef _WIN32
  parse_options(argc, argv);
#endif

  if (res = video_generator_init(&cfg, &gen)) {
    printf("Error: cannot initialize the generator %d.\n", res);
    exit(1);
  }

  printf("Create a YUV file: %s \nwidth: %d\nheight: %d \nfps: %d\nframes: %d\nformat: %d\nbitdepth: %d\n\n",
            filename,
            cfg.width,
            cfg.height,
            cfg.fps,
            max_frames,
            cfg.format,
            cfg.bitdepth);

  while (gen.frame < max_frames) {
    uint16_t *nv12Y, *nv12UV, *srcY, *srcU, *srcV;
    video_generator_update(&gen);

    convert8to16(gen.y, &srcY, gen.ybytes);
    convert8to16(gen.u, &srcU, gen.ubytes);
    convert8to16(gen.v, &srcV, gen.vbytes);

    I420ToNV12_10bit(srcY, srcU, srcV, &nv12Y, &nv12UV, cfg.width, cfg.height);
    NV12ToI420_10bit(nv12Y, nv12UV, &srcY, &srcU, &srcV, cfg.width, cfg.height);

    convert16to8(srcY, &gen.y, gen.ybytes / 2);
    convert16to8(srcU, &gen.u, gen.ubytes / 2 );
    convert16to8(srcV, &gen.v, gen.vbytes / 2);

    // write video planes to a file
    fwrite((char*)gen.y, gen.ybytes, 1,  video_fp);
    fwrite((char*)gen.u, gen.ubytes, 1, video_fp);
    fwrite((char*)gen.v, gen.vbytes, 1, video_fp);
  }
  printf("Frames generated: %zu\n", gen.frame);

fclose(video_fp);
free(filename);
video_generator_clear(&gen);
}

/* ----------------------------------------------------------------------------------- */



