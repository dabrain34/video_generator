/*

  Video Generator
  ================

  The Video Generator library is used to create a continuous YUV420P
  video signal . This library was created to test video encoders over
   a longer period. Therefore the video signal contains a
  time field that shows how long the generator has been running. The
  time is based on the generated number of video frames. It's up to
  the user to make sure that the `video_generator_update()` function
  is called often enough to keep up with the number of frames you want
  to generate.


  Using the Video Generator
  -------------------------

  First initialize the generator using `video_generator_init()` then
  call `video_generator_update()` every time you want to generate a
  new video frame. When you call `video_generator_update()` the
  `frame` member of the `video_generator` struct is updated. Each time
  you call `video_generator_update()` it will update the contents of
  the Y, U, and V planes.



  video_generator_init()       - initialize, see below for the declaration.
  video_generator_update()     - generate a new video frame, see below for the declaration.
  video_generator_clear()      - frees allocated memory, see below for the declaration.


  Settings:
  ---------

  When you call `video_generator_init()` you pass it a `video_generator_settings` object that
  describes some things about the video you want to generate. You can set the
  following settings.

  width            - width of the video frames (e.g. 640).
  height           - height of the video frames (e.g. 480).
  fps              - framerate (e.g. 25)

  Specification
  ---------------


  Video: YUV420P / I420P
         1 continuous block of memory
         y-stride = width
         u-stride = width / 2
         v-stride = width / 2


  Convert video with avconv
  ----------------------------------

  You can write the video frames into a file and use avconv to encode it to some format:

       ````sh
       ./avconv -f rawvideo -pix_fmt yuv420p -s 640x480 -i output.yuv -vcodec h264 -r 25 -y out.mov
       ````

  Example:
  --------
  <example>

     fp = fopen("output.yuv", "wb");

     video_generator gen;
     video_geneator_settings cfg;

     cfg.width = WIDTH;
     cfg.height = HEIGHT;
     cfg.fps = FPS;

     if (0 != video_generator_init(&cfg, &gen)) {
       printf("Error: cannot initialize the generator.\n");
       exit(1);
     }

     while(1) {

        printf("Frame: %llu\n", gen.frame);

        video_generator_update(&gen);

        // write video planes to a file
        fwrite((char*)gen.y, gen.ybytes,1,  fp);
        fwrite((char*)gen.u, gen.ubytes,1, fp);
        fwrite((char*)gen.v, gen.vbytes,1, fp);

        if (gen.frame > 250) {
          break;
        }

        usleep(gen.fps);
     }

    fclose(fp);

    video_generator_clear(&gen);

  </example>
 */

#ifndef VIDEO_GENERATOR_H
#define VIDEO_GENERATOR_H

#define RXS_MAX_CHARS 11
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* ----------------------------------------------------------------------------------- */
/*                          V I D E O   G E N E R A T O  R                             */
/* ----------------------------------------------------------------------------------- */

typedef struct video_generator video_generator;
typedef struct video_generator_settings video_generator_settings;
typedef struct video_generator_char video_generator_char;

struct video_generator_char {
  int id;
  int x;
  int y;
  int width;
  int height;
  int xoffset;
  int yoffset;
  int xadvance;
};

struct video_generator_settings {
  uint32_t width;
  uint32_t height;
  uint32_t fps;
  uint32_t format;
};

struct video_generator {

  /* video  */
  uint64_t frame;                                         /* current frame number, which is incremented by one in `video_generator_update`. */
  uint8_t* y;                                             /* points to the y-plane. */
  uint8_t* u;                                             /* points to the u-plane. */
  uint8_t* v;                                             /* points to the v-plane. */
  uint32_t width;                                         /* width of the video frame (and y-plane). */
  uint32_t height;                                        /* height of the video frame (and y-plane). */
  uint32_t ybytes;                                        /* number of bytes in the y-plane. */
  uint32_t ubytes;                                        /* number of bytes in the u-plane. */
  uint32_t vbytes;                                        /* number of bytes in the v-plane. */
  uint32_t nbytes;                                        /* total number of bytes in the allocated buffer for the yuv420p buffer. */
  double   u_factor;
  double   v_factor;
  int fps_num;                                            /* framerate numerator e.g. 1. */
  int fps_den;                                            /* framerate denominator e.g. 25. */
  double fps;                                             /* framerate in microseconds, 1 fps == 1.000.000 us. */
  double step;                                            /* used to create/translate the moving bar. */
  double perc;                                            /* position of the moving bar in percentages. */
  video_generator_char chars[RXS_MAX_CHARS];              /* bitmap characters, `0-9` and `:` */
  int font_w;                                             /* width of the bitmap (which is stored in video_generator.c). */
  int font_h;                                             /* height of the bitmap (which is stored in video_generator.c). */
  int font_line_height;

};

int video_generator_init(video_generator_settings* cfg, video_generator* g);
int video_generator_update(video_generator* g);
int video_generator_clear(video_generator* g);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif
