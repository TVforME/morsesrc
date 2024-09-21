/*
  MIT License

  Copyright (c) 2024 VK3DG

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  A GStreamer audio source plugin to convert TEXT to MORSE CODE using a table.

  USAGE:
  gst-launch-1.0 morsesrc text="CQ CQ DE VK3DG" ! autoaudiosink

*/

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/audio/audio.h>
#include <math.h>
#include <ctype.h>

#define PACKAGE "morsesrc"

#define FORMAT_STR  " { S16LE, S16BE, U16LE, U16BE, "	\
  "S24_32LE, S24_32BE, U24_32LE, U24_32BE, "		\
  "S32LE, S32BE, U32LE, U32BE, "			\
  "S24LE, S24BE, U24LE, U24BE, "			\
  "S20LE, S20BE, U20LE, U20BE, "			\
  "S18LE, S18BE, U18LE, U18BE, "			\
  "F32LE, F32BE, F64LE, F64BE, "			\
  "S8, U8 }"

#define DEFAULT_FORMAT_STR GST_AUDIO_NE ("S16")

#define DEFAULT_RATE 44100
#define DEFAULT_FREQUENCY 880.0
#define DEFAULT_VOLUME 0.5
#define DEFAULT_WPM 20

static unsigned short morse_table[128] = {
  /*00 */ 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
  /*08 */ 0000, 0000, 0412, 0000, 0000, 0412, 0000, 0000,
  /*10 */ 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
  /*18 */ 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
  /*20 */ 0000, 0665, 0622, 0000, 0000, 0000, 0502, 0636,
  /*28 */ 0515, 0000, 0000, 0512, 0663, 0000, 0652, 0511,
  /*30 */ 0537, 0536, 0534, 0530, 0520, 0500, 0501, 0503,
  /*38 */ 0507, 0517, 0607, 0625, 0000, 0521, 0000, 0614,
  /*40 */ 0000, 0202, 0401, 0405, 0301, 0100, 0404, 0303,
  /*48 */ 0400, 0200, 0416, 0305, 0402, 0203, 0201, 0307,
  /*50 */ 0406, 0413, 0302, 0300, 0101, 0304, 0410, 0306,
  /*58 */ 0411, 0415, 0403, 0000, 0000, 0000, 0000, 0000,
  /*60 */ 0000, 0202, 0401, 0405, 0301, 0100, 0404, 0303,
  /*68 */ 0400, 0200, 0416, 0305, 0402, 0203, 0201, 0307,
  /*70 */ 0406, 0413, 0302, 0300, 0101, 0304, 0410, 0306,
  /*78 */ 0411, 0415, 0403, 0000, 0000, 0000, 0000, 0000
};

struct _GstMorseSrc;

typedef void (*CW_GENERATE_FUNC) (struct _GstMorseSrc*, guint8 *, gint);

typedef struct _GstMorseSrc
{
  GstPushSrc parent;

  gdouble frequency;
  gdouble volume;
  gint wpm;

  CW_GENERATE_FUNC cwfunc;
  GstAudioFormatPack packfunc;
  guint packsize;
  
  gchar *text;
  GString *generated_morse;
  guint position;

  guint samples_per_dot;
  guint samples_per_dash;
  guint samples_per_space;
  
  GstClockTime timestamp;
  GstSegment segment;
  GstAudioInfo info;
} GstMorseSrc;

typedef struct _GstMorseSrcClass
{
  GstPushSrcClass parent_class;
} GstMorseSrcClass;

#define GST_TYPE_MORSE_SRC (gst_morse_src_get_type())
#define GST_MORSE_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_MORSE_SRC, GstMorseSrc))

G_DEFINE_TYPE (GstMorseSrc, gst_morse_src, GST_TYPE_PUSH_SRC)

     enum {
  PROP_0,
  PROP_FREQUENCY,
  PROP_VOLUME,
  PROP_WPM,
  PROP_TEXT,
  LAST_PROP
};

static void
gst_morse_src_set_property (GObject *object, guint prop_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
  GstMorseSrc *src = GST_MORSE_SRC (object);

  switch (prop_id)
    {
    case PROP_FREQUENCY:
      src->frequency = g_value_get_double (value);
      break;
    case PROP_VOLUME:
      src->volume = g_value_get_double (value);
      break;
    case PROP_WPM:
      src->wpm = g_value_get_int (value);
      break;
    case PROP_TEXT:
      if (src->text)
	{
	  g_free (src->text);
	}
      src->text = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gst_morse_src_get_property (GObject *object, guint prop_id, GValue *value,
			    GParamSpec *pspec)
{
  GstMorseSrc *src = GST_MORSE_SRC (object);

  switch (prop_id)
    {
    case PROP_FREQUENCY:
      g_value_set_double (value, src->frequency);
      break;
    case PROP_VOLUME:
      g_value_set_double (value, src->volume);
      break;
    case PROP_WPM:
      g_value_set_int (value, src->wpm);
      break;
    case PROP_TEXT:
      g_value_set_string (value, src->text);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GstStaticPadTemplate src_template =
  GST_STATIC_PAD_TEMPLATE ("src",
			   GST_PAD_SRC,
			   GST_PAD_ALWAYS,
			   GST_STATIC_CAPS ("audio/x-raw, "
					    "format = (string) " FORMAT_STR ", "
					    "rate = " GST_AUDIO_RATE_RANGE ", "
					    "layout = interleaved,"
					    "channels = " GST_AUDIO_CHANNELS_RANGE)
			   );

static void
morse_send_char (GString *text, int ch)
{
  int nsyms, bitreg;

  if (ch == ' ')
    {
      g_string_append (text, "  ");
      return;
    }

  bitreg = morse_table[ch & 0x7f];
  if ((nsyms = (bitreg >> 6) & 07) == 0)
    nsyms = 8;
  bitreg &= 077;
  while (nsyms-- > 0)
    {
      g_string_append_c (text, ' ');
      if (bitreg & 01)
	{
	  g_string_append_c (text, '-');
	}
      else
	{
	  g_string_append_c (text, '.');
	}
      bitreg >>= 1;
    }
  g_string_append_c (text, ' ');	// space between characters
}

static void
morse_send_string (GString *text, const char *str)
{
  while (*str)
    {
      morse_send_char (text, toupper (*str));
      str++;
    }
  g_string_append (text, "   ");	// space between words
}

#define CW_GENERATOR(sample_t, scale)					\
  static void								\
  MORSE_CW_GENERATE_##sample_t (GstMorseSrc *src,			\
				guint8 *buf, gint samples)		\
  {									\
    sample_t *data = (sample_t *) buf;					\
    for (gint i = 0; i < samples; i++)					\
      for (gint j = 0; j < GST_AUDIO_INFO_CHANNELS (&src->info); j++) {	\
	data[i*GST_AUDIO_INFO_CHANNELS (&src->info)+j] = (sample_t)	\
	  (src->volume * scale * sin(2.0 * G_PI * src->frequency * i	\
				     / GST_AUDIO_INFO_RATE (&src->info))); \
      }									\
  }

CW_GENERATOR (gint16, 32767.0)
CW_GENERATOR (gint32, 2147483647.0)
CW_GENERATOR (gfloat, 1.0)
CW_GENERATOR (gdouble, 1.0)

static GstFlowReturn
gst_morse_src_create (GstPushSrc *pushsrc, GstBuffer **buffer)
{
  GstMorseSrc *src = GST_MORSE_SRC (pushsrc);

  if (!src->generated_morse || src->position >= src->generated_morse->len)
    {
      return GST_FLOW_EOS;
    }

  guint samples_per_dot	  = src->samples_per_dot;
  guint samples_per_dash  = src->samples_per_dash;
  guint samples_per_space = src->samples_per_space;
  guint max_samples	  = 5292*10;

  size_t bpf = src->packfunc
    ? src->packsize * GST_AUDIO_INFO_CHANNELS (&src->info)
    : (size_t) GST_AUDIO_INFO_BPF (&src->info);

  GstBuffer *buf = gst_buffer_new_and_alloc (max_samples * bpf);

  GstMapInfo map;
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  gst_buffer_memset (buf, 0, 0, max_samples * bpf);
  
  guint i = 0;
  while (i < max_samples && src->position < src->generated_morse->len)
    {
      char symbol = src->generated_morse->str[src->position];
      guint num_samples = 0;

      switch (symbol) {
      case '.':
	num_samples = samples_per_dot;
	break;
      case '-':
	num_samples = samples_per_dash;
	break;
      case ' ':
	num_samples = samples_per_space;
	break;
      }

      num_samples = MIN (max_samples - i, num_samples);

      if (symbol != ' ')
	src->cwfunc (src, map.data + i * bpf, num_samples);
      
      i += num_samples;

      if (num_samples < samples_per_space
	  && samples_per_space < max_samples - i)
	i += samples_per_space - num_samples;
      
      src->position++;
    }

  if (src->packfunc)
    {
      GstBuffer *rbuf = gst_buffer_new_and_alloc (
        max_samples * GST_AUDIO_INFO_BPF (&src->info)
      );

      GstMapInfo rmap;
      gst_buffer_map (rbuf, &rmap, GST_MAP_WRITE);

      src->packfunc (src->info.finfo, 0, map.data, rmap.data, i);

      gst_buffer_set_size (rbuf, GST_AUDIO_INFO_BPF (&src->info) * i);
      gst_buffer_unmap (rbuf, &rmap);
      gst_buffer_unref (buf);

      buf = rbuf;
    }
  else
    {
      gst_buffer_unmap (buf, &map);
      gst_buffer_set_size (buf, i * bpf);
    }
  
  GST_BUFFER_PTS (buf) = src->timestamp;
  GST_BUFFER_DURATION (buf) =
    gst_util_uint64_scale (i, GST_SECOND, GST_AUDIO_INFO_RATE (&src->info));
  src->timestamp += GST_BUFFER_DURATION (buf);

  *buffer = buf;

  return GST_FLOW_OK;
}

static gboolean
gst_morse_src_start (GstBaseSrc *basesrc)
{
  GstMorseSrc *src = GST_MORSE_SRC (basesrc);

  if (src->generated_morse)
    {
      g_string_free (src->generated_morse, TRUE);
    }

  src->generated_morse = g_string_new (NULL);
  morse_send_string (src->generated_morse, src->text);
  src->position = 0;

  src->timestamp = 0;

  GstSegment *segment = &src->segment;
  gst_segment_init (segment, GST_FORMAT_TIME);
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  segment->start = 0;
  segment->time = 0;
  gst_base_src_new_segment (basesrc, segment);

  return TRUE;
}

static gboolean
gst_morse_src_stop (GstBaseSrc *basesrc)
{
  GstMorseSrc *src = GST_MORSE_SRC (basesrc);

  if (src->generated_morse)
    {
      g_string_free (src->generated_morse, TRUE);
      src->generated_morse = NULL;
    }

  return TRUE;
}

static GstCaps *
gst_morse_src_fixate (GstBaseSrc *bsrc, GstCaps *caps)
{
  GstMorseSrc *src = GST_MORSE_SRC (bsrc);
  GstStructure *structure;
  gint channels;

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (src, "fixating samplerate to %d", GST_AUDIO_DEF_RATE);

  gst_structure_fixate_field_nearest_int (structure, "rate", GST_AUDIO_DEF_RATE);

  gst_structure_fixate_field_string (structure, "format", DEFAULT_FORMAT_STR);

  gst_structure_fixate_field_string (structure, "layout", "interleaved");

  /* fixate to mono unless downstream requires stereo, for backwards compat */
  gst_structure_fixate_field_nearest_int (structure, "channels", 1);

  if (gst_structure_get_int (structure, "channels", &channels) && channels > 2) {
    if (!gst_structure_has_field_typed (structure, "channel-mask",
					GST_TYPE_BITMASK))
      gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK, 0ULL,
			 NULL);
  }

  caps = GST_BASE_SRC_CLASS (gst_morse_src_parent_class)->fixate (bsrc, caps);

  return caps;
}

static gboolean
gst_morse_src_setcaps (GstBaseSrc *basesrc, GstCaps *caps)
{
  GstMorseSrc *src = GST_MORSE_SRC (basesrc);
  GstAudioInfo info;

  if (!gst_audio_info_from_caps (&info, caps))
    goto invalid_caps;

  GST_DEBUG_OBJECT (src, "negotiated to caps %" GST_PTR_FORMAT, (void *) caps);
  
  src->samples_per_dot	 = GST_AUDIO_INFO_RATE (&info) * 60 / (src->wpm * 50);
  src->samples_per_dash	 = src->samples_per_dot * 3;
  src->samples_per_space = src->samples_per_dot;

  src->cwfunc	= NULL;
  src->packfunc = NULL;
  src->packsize = 0;

  src->info = info;

  switch (GST_AUDIO_FORMAT_INFO_FORMAT (src->info.finfo))
    {
    case GST_AUDIO_FORMAT_S16:
      src->cwfunc = MORSE_CW_GENERATE_gint16;
      break;
    case GST_AUDIO_FORMAT_S32:
      src->cwfunc = MORSE_CW_GENERATE_gint32;
      break;
    case GST_AUDIO_FORMAT_F32:
      src->cwfunc = MORSE_CW_GENERATE_gfloat;
      break;
    case GST_AUDIO_FORMAT_F64:
      src->cwfunc = MORSE_CW_GENERATE_gdouble;
      break;
    default:
      switch (src->info.finfo->unpack_format)
	{
        case GST_AUDIO_FORMAT_S32:
	  src->cwfunc = MORSE_CW_GENERATE_gint32;
          src->packfunc = src->info.finfo->pack_func;
          src->packsize = sizeof (gint32);
          break;
        case GST_AUDIO_FORMAT_F64:
	  src->cwfunc = MORSE_CW_GENERATE_gdouble;
          src->packfunc = src->info.finfo->pack_func;
          src->packsize = sizeof (gdouble);
          break;
        default:
          g_assert_not_reached ();
	}
    }
  
  return TRUE;

  /* ERROR */
 invalid_caps:
  {
    GST_ERROR_OBJECT (basesrc, "received invalid caps");
    return FALSE;
  }
}

static void
gst_morse_src_class_init (GstMorseSrcClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS(klass);

  gobject_class->set_property = gst_morse_src_set_property;
  gobject_class->get_property = gst_morse_src_get_property;

  g_object_class_install_property (gobject_class, PROP_FREQUENCY,
				   g_param_spec_double ("frequency", "Frequency", "Frequency in Hz", 0.0,
							G_MAXDOUBLE, DEFAULT_FREQUENCY, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_VOLUME,
				   g_param_spec_double ("volume", "Volume", "Volume", 0.0, 1.0,
							DEFAULT_VOLUME, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_WPM,
				   g_param_spec_int ("wpm", "Words per minute", "Words per minute", 1,
						     G_MAXINT, DEFAULT_WPM, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TEXT,
				   g_param_spec_string ("text", "Morse text",
							"String to convert to Morse code", NULL,
							G_PARAM_READWRITE));

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
					     &src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
					 "Morse Source", "Source/Audio",
					 "Generates Morse code audio",
					 "Robert Hensel vk3dgtv@gmail.com");

  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_morse_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_morse_src_stop);
  basesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_morse_src_fixate);
  basesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_morse_src_setcaps);
  pushsrc_class->create = GST_DEBUG_FUNCPTR (gst_morse_src_create);
}

static void
gst_morse_src_init (GstMorseSrc *src)
{
  src->frequency = DEFAULT_FREQUENCY/2;
  src->volume = DEFAULT_VOLUME;
  src->wpm = DEFAULT_WPM;

  src->text = NULL;
  src->generated_morse = NULL;

  src->position = 0;
  src->timestamp = 0;

  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_segment_init (&src->segment, GST_FORMAT_TIME);
}

static gboolean
plugin_init (GstPlugin *plugin)
{
  return gst_element_register (plugin, "morsesrc", GST_RANK_NONE,
			       GST_TYPE_MORSE_SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		   GST_VERSION_MINOR,
		   morsesrc,
		   "Generates Morse code audio",
		   plugin_init,
		   "1.0", "LGPL", "GStreamer", "http://gstreamer.net/")
