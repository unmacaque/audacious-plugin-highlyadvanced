/*
 * highlyadvanced for audacious: GBA (GSF) chiptune plugin
 * Copyright (c) 2005-2007 William Pitcock <nenolod@sacredspiral.co.uk>
 *
 * highlyadvanced engine:
 * Copyright (c) 2005-2007 CaitSith2
 *
 */

#include <glib.h>
#include <string.h>

#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "VBA/psftag.h"
#include "gsf.h"

int defvolume=1000;
int relvolume=1000;
int TrackLength=0;
int FadeLength=0;
int IgnoreTrackLength, DefaultLength=150000;
int playforever=0;
int fileoutput=0;
int TrailingSilence=1000;
int DetectSilence=0, silencedetected=0, silencelength=5;

int cpupercent=0, sndSamplesPerSec, sndNumChannels;
int sndBitsPerSample=16;
int didseek = 0;
double playtime = 0;

int deflen=120,deffade=4;

extern unsigned short soundFinalWave[1470];
extern int soundBufferLen;

extern char soundEcho;
extern char soundLowPass;
extern char soundReverse;
extern char soundQuality;

static gchar *lastfn;

GThread *gsf_emulthread;

int LengthFromString(const char * timestring);
int VolumeFromString(const char * volumestring);

double decode_pos_ms; // current decoding position, in milliseconds
int seek_needed; // if != -1, it is the point that the decode thread should seek to, in ms.
gboolean stop_flag = FALSE;

static gboolean gsf_play(InputPlayback * data, const gchar * filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause);
gboolean gsf_play_loop(const gchar *filename);

Tuple *gsf_get_song_tuple(const gchar *filename, VFSFile *file)
{
  char tag[50001];
  char tmp_str[256];
  const gchar *fn;

  Tuple *ti;

  fn = g_filename_from_uri(filename, NULL, NULL);

  ti = tuple_new_from_filename(fn);

  psftag_readfromfile((void*)tag, fn);

  if (!psftag_getvar(tag, "title", tmp_str, sizeof(tmp_str)-1)) {
      tuple_set_str(ti, FIELD_TITLE, NULL, tmp_str);
  }

  if (!psftag_getvar(tag, "artist", tmp_str, sizeof(tmp_str)-1)) {
      tuple_set_str(ti, FIELD_ARTIST, NULL, tmp_str);
  }

  if (!psftag_getvar(tag, "game", tmp_str, sizeof(tmp_str)-1)) {
      tuple_set_str(ti, FIELD_ALBUM, NULL, tmp_str);
  }

  if (!psftag_getvar(tag, "year", tmp_str, sizeof(tmp_str)-1)) {
      tuple_set_str(ti, FIELD_DATE, NULL, tmp_str);
  }

  if (!psftag_getvar(tag, "copyright", tmp_str, sizeof(tmp_str)-1)) {
      tuple_set_str(ti, FIELD_COPYRIGHT, NULL, tmp_str);
  }

  if (!psftag_getvar(tag, "tagger", tmp_str, sizeof(tmp_str)-1)) {
      tuple_set_str(ti, -1, "tagger", tmp_str);
  }

  if (!psftag_raw_getvar(tag, "length", tmp_str, sizeof(tmp_str)-1)) {
      tuple_set_int(ti, FIELD_LENGTH, NULL, LengthFromString(tmp_str) + FadeLength);
  }

  if (!psftag_getvar(tag, "comment", tmp_str, sizeof(tmp_str)-1)) {
      tuple_set_str(ti, FIELD_COMMENT, NULL, tmp_str);
  }

  tuple_set_str(ti, FIELD_CODEC, NULL, "GameBoy Advanced Audio (GSF)");
  tuple_set_str(ti, FIELD_QUALITY, NULL, "sequenced");

  return ti;
}

static InputPlayback *_playback = NULL;

void end_of_track(void)
{
  stop_flag = TRUE;
}

void writeSound(void)
{
  int ret = soundBufferLen;
  static int countdown = 20;

  decode_pos_ms += (ret/(2*sndNumChannels) * 1000) / (float)sndSamplesPerSec;

  _playback->output->write_audio (soundFinalWave, soundBufferLen);

  if (--countdown == 0)
    {
      countdown = 20;
    }

  /* is seek request complete? (-2) */
  if (seek_needed == -2)
    {
      _playback->output->flush(seek_needed);
      seek_needed = -1;
    }

  if (lastfn != NULL && (seek_needed != -1))	//if a seek is initiated
    {
      if (seek_needed < decode_pos_ms)	//if we are asked to seek backwards.  we have to start from the beginning
        {
          GSFClose();
          GSFRun(lastfn);
          decode_pos_ms = 0;
        }
    }
}

static gboolean gsf_play(InputPlayback * playback, const gchar * filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
  soundLowPass = 0;
  soundEcho = 0;
  soundQuality = 0;

  DetectSilence=1;
  silencelength=5;
  IgnoreTrackLength=0;
  DefaultLength=150000;
  TrailingSilence=1000;
  playforever=0;

  _playback = playback;
  return gsf_play_loop(filename);
}

gboolean gsf_play_loop(const gchar * filename)
{
  int r;
  const gchar *fn;

  fn = g_filename_from_uri(filename, NULL, NULL);

  r = GSFRun(fn);
  if (!r)
    return -1;

  lastfn = g_strdup(fn);

  _playback->output->open_audio(FMT_S16_LE, sndSamplesPerSec, sndNumChannels);

  //gint length = tuple_get_int(ti, FIELD_LENGTH, NULL);

  _playback->set_params(_playback, sndSamplesPerSec*2*2*8, sndSamplesPerSec, sndNumChannels);

  decode_pos_ms = 0;
  seek_needed = -1;
  TrailingSilence=1000;

  stop_flag = FALSE;
  _playback->set_pb_ready(_playback);

  while(! stop_flag)
    EmulationLoop();

  GSFClose();

  stop_flag = TRUE;
  _playback->output->close_audio();
  g_free(lastfn);
  lastfn = NULL;

  return 0;
}

static void gsf_stop(InputPlayback *playback)
{
  stop_flag = TRUE;

  playback->output->abort_write ();

  if (lastfn != NULL)
    {
      lastfn = NULL;
    }
}

static void gsf_pause(InputPlayback *playback, gboolean pause)
{
  if (!stop_flag)
    playback->output->pause(pause);
}

static int gsf_is_our_fd(const gchar *filename, VFSFile *file)
{
  void *magic;
  const gchar *tmps;

  // Filter out gsflib [we use them, but we can't play them]
  static const gchar *teststr = "gsflib";
  if (strlen(teststr) < strlen(filename))
    {
      tmps = filename + strlen(filename);
      tmps -= strlen(teststr);
      if (!strcasecmp(tmps, teststr))
        return 0;
    }

  //vfs_fread(magic,1,4,file);

  if (!memcmp(magic,"PSF\"",4))
    return 1;

  return 0;
}

static void gsf_mseek(InputPlayback *playback, gint time)
{
  seek_needed = time;
}

static const gchar *gsf_fmts[] = { "gsf", "minigsf", NULL };

AUD_INPUT_PLUGIN
(
    .name = "Highly Advanced for Audacious",
    .play = gsf_play,
    .stop = gsf_stop,
    .pause = gsf_pause,
    .mseek = gsf_mseek,
    .probe_for_tuple = gsf_get_song_tuple,
    .is_our_file_from_vfs = gsf_is_our_fd,
    .extensions = gsf_fmts,
)

