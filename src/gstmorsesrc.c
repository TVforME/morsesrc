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
gst-launch-1.0 morsesrc text="CQ CQ DE VK3DG" ! audioconvert ! autoaudiosink

Currently Not working.. Looking fo assitance..
*/


#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <math.h>
#include <ctype.h>

#define PACKAGE "morsesrc"

#define DEFAULT_RATE 44100
#define DEFAULT_FREQUENCY 880.0
#define DEFAULT_VOLUME 0.5
#define DEFAULT_WPM 20

static unsigned short morse_table[128] = {
    /*00*/ 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*08*/ 0000, 0000, 0412, 0000, 0000, 0412, 0000, 0000,
    /*10*/ 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*18*/ 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*20*/ 0000, 0665, 0622, 0000, 0000, 0000, 0502, 0636,
    /*28*/ 0515, 0000, 0000, 0512, 0663, 0000, 0652, 0511,
    /*30*/ 0537, 0536, 0534, 0530, 0520, 0500, 0501, 0503,
    /*38*/ 0507, 0517, 0607, 0625, 0000, 0521, 0000, 0614,
    /*40*/ 0000, 0202, 0401, 0405, 0301, 0100, 0404, 0303,
    /*48*/ 0400, 0200, 0416, 0305, 0402, 0203, 0201, 0307,
    /*50*/ 0406, 0413, 0302, 0300, 0101, 0304, 0410, 0306,
    /*58*/ 0411, 0415, 0403, 0000, 0000, 0000, 0000, 0000,
    /*60*/ 0000, 0202, 0401, 0405, 0301, 0100, 0404, 0303,
    /*68*/ 0400, 0200, 0416, 0305, 0402, 0203, 0201, 0307,
    /*70*/ 0406, 0413, 0302, 0300, 0101, 0304, 0410, 0306,
    /*78*/ 0411, 0415, 0403, 0000, 0000, 0000, 0000, 0000
};

typedef struct _GstMorseSrc {
    GstPushSrc parent;
    gint rate;
    gdouble frequency;
    gdouble volume;
    gint wpm;
    gchar *text;
    GString *generated_morse;
    guint position;
    guint samples_per_dot;
    guint samples_per_dash;
    guint samples_per_space;
    GstClockTime timestamp;
    GstSegment segment;
} GstMorseSrc;

typedef struct _GstMorseSrcClass {
    GstPushSrcClass parent_class;
} GstMorseSrcClass;

#define GST_TYPE_MORSE_SRC (gst_morse_src_get_type())
#define GST_MORSE_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_MORSE_SRC, GstMorseSrc))

G_DEFINE_TYPE(GstMorseSrc, gst_morse_src, GST_TYPE_PUSH_SRC)

enum {
    PROP_0,
    PROP_RATE,
    PROP_FREQUENCY,
    PROP_VOLUME,
    PROP_WPM,
    PROP_TEXT,
    LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static void gst_morse_src_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstMorseSrc *src = GST_MORSE_SRC(object);

    switch (prop_id) {
        case PROP_RATE:
            src->rate = g_value_get_int(value);
            break;
        case PROP_FREQUENCY:
            src->frequency = g_value_get_double(value);
            break;
        case PROP_VOLUME:
            src->volume = g_value_get_double(value);
            break;
        case PROP_WPM:
            src->wpm = g_value_get_int(value);
            break;
        case PROP_TEXT:
            if (src->text) {
                g_free(src->text);
            }
            src->text = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void gst_morse_src_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstMorseSrc *src = GST_MORSE_SRC(object);

    switch (prop_id) {
        case PROP_RATE:
            g_value_set_int(value, src->rate);
            break;
        case PROP_FREQUENCY:
            g_value_set_double(value, src->frequency);
            break;
        case PROP_VOLUME:
            g_value_set_double(value, src->volume);
            break;
        case PROP_WPM:
            g_value_set_int(value, src->wpm);
            break;
        case PROP_TEXT:
            g_value_set_string(value, src->text);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("audio/x-raw, format=(string)S16LE, layout=(string)interleaved, channels=(int)1, rate=(int)[ 1, MAX ]")
);

static void morse_send_char(GString *text, int ch) {
    int nsyms, bitreg;

    bitreg = morse_table[ch & 0x7f];
    if ((nsyms = (bitreg >> 6) & 07) == 0)
        nsyms = 8;
    bitreg &= 077;
    while (nsyms-- > 0) {
        g_string_append_c(text, ' ');
        if (bitreg & 01) {
            g_string_append_c(text, '-');
        } else {
            g_string_append_c(text, '.');
        }
        bitreg >>= 1;
    }
    g_string_append_c(text, ' '); // space between characters
}

static void morse_send_string(GString *text, const char *str) {
    while (*str) {
        morse_send_char(text, toupper(*str));
        str++;
    }
    g_string_append(text, "   "); // space between words
}

static GstFlowReturn gst_morse_src_create(GstPushSrc *pushsrc, GstBuffer **buffer) {
    GstMorseSrc *src = GST_MORSE_SRC(pushsrc);

    if (!src->generated_morse || src->position >= src->generated_morse->len) {
        return GST_FLOW_EOS;
    }

    guint samples_per_dot = src->samples_per_dot;
    guint samples_per_dash = src->samples_per_dash;
    guint samples_per_space = src->samples_per_space;
    guint max_samples = 4096;

    GstBuffer *buf = gst_buffer_new_and_alloc(max_samples * sizeof(gint16));
    GstMapInfo map;
    gst_buffer_map(buf, &map, GST_MAP_WRITE);

    gint16 *data = (gint16 *)map.data;
    gsize length = map.size / sizeof(gint16);

    guint i = 0;
    while (i < length && src->position < src->generated_morse->len) {
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

        for (guint j = 0; j < num_samples && i < length; j++) {
            if (symbol == ' ') {
                data[i++] = 0;
            } else {
                data[i++] = (gint16)(src->volume * 32767.0 * sin(2.0 * G_PI * src->frequency * j / src->rate));
            }
        }

        if (num_samples < samples_per_space) {
            for (guint j = num_samples; j < samples_per_space && i < length; j++) {
                data[i++] = 0;
            }
        }

        src->position++;
    }

    gst_buffer_unmap(buf, &map);

    GST_BUFFER_PTS(buf) = src->timestamp;
    GST_BUFFER_DURATION(buf) = gst_util_uint64_scale(length, GST_SECOND, src->rate);
    src->timestamp += GST_BUFFER_DURATION(buf);

    *buffer = buf;

    return GST_FLOW_OK;
}

static gboolean gst_morse_src_start(GstBaseSrc *basesrc) {
    GstMorseSrc *src = GST_MORSE_SRC(basesrc);

    if (src->generated_morse) {
        g_string_free(src->generated_morse, TRUE);
    }

    src->generated_morse = g_string_new(NULL);
    morse_send_string(src->generated_morse, src->text);
    src->position = 0;

    src->samples_per_dot = src->rate * 60 / (src->wpm * 50);
    src->samples_per_dash = src->samples_per_dot * 3;
    src->samples_per_space = src->samples_per_dot;

    src->timestamp = 0;

    GstSegment *segment = &src->segment;
    gst_segment_init(segment, GST_FORMAT_TIME);
    segment->start = 0;
    segment->time = 0;
    gst_base_src_new_segment(basesrc, segment);

    return TRUE;
}

static gboolean gst_morse_src_stop(GstBaseSrc *basesrc) {
    GstMorseSrc *src = GST_MORSE_SRC(basesrc);

    if (src->generated_morse) {
        g_string_free(src->generated_morse, TRUE);
        src->generated_morse = NULL;
    }

    return TRUE;
}

static void gst_morse_src_class_init(GstMorseSrcClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS(klass);
    GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS(klass);

    gobject_class->set_property = gst_morse_src_set_property;
    gobject_class->get_property = gst_morse_src_get_property;

    properties[PROP_RATE] = g_param_spec_int("rate", "Rate", "Sample rate in Hz", 1, G_MAXINT, DEFAULT_RATE, G_PARAM_READWRITE);
    properties[PROP_FREQUENCY] = g_param_spec_double("frequency", "Frequency", "Frequency in Hz", 0.0, G_MAXDOUBLE, DEFAULT_FREQUENCY, G_PARAM_READWRITE);
    properties[PROP_VOLUME] = g_param_spec_double("volume", "Volume", "Volume", 0.0, 1.0, DEFAULT_VOLUME, G_PARAM_READWRITE);
    properties[PROP_WPM] = g_param_spec_int("wpm", "Words per minute", "Words per minute", 1, G_MAXINT, DEFAULT_WPM, G_PARAM_READWRITE);
    properties[PROP_TEXT] = g_param_spec_string("text", "Morse text", "String to convert to Morse code", NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(gobject_class, LAST_PROP, properties);

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
        "Morse Source", "Source/Audio",
        "Generates Morse code audio",
        "Robert Hensel vk3dgtv@gmail.com");

    gst_element_class_add_pad_template(GST_ELEMENT_CLASS(klass),
        gst_static_pad_template_get(&src_template));

    basesrc_class->start = GST_DEBUG_FUNCPTR(gst_morse_src_start);
    basesrc_class->stop = GST_DEBUG_FUNCPTR(gst_morse_src_stop);
    pushsrc_class->create = GST_DEBUG_FUNCPTR(gst_morse_src_create);
}

static void gst_morse_src_init(GstMorseSrc *src) {
    src->rate = DEFAULT_RATE;
    src->frequency = DEFAULT_FREQUENCY;
    src->volume = DEFAULT_VOLUME;
    src->wpm = DEFAULT_WPM;
    src->text = NULL;
    src->generated_morse = NULL;
    src->position = 0;
    src->timestamp = 0;
    gst_segment_init(&src->segment, GST_FORMAT_TIME);
}

static gboolean plugin_init(GstPlugin *plugin) {
    return gst_element_register(plugin, "morsesrc", GST_RANK_NONE, GST_TYPE_MORSE_SRC);
}

GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    morsesrc,
    "Generates Morse code audio",
    plugin_init,
    "1.0",
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)

