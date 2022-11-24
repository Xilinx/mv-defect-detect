/*
 * Copyright 2022, Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gst/video/video.h>
#include <string.h>
#include <unistd.h>
#include <memory>
#include <stdexcept>
#include <glob.h>
#include <sstream>

using namespace std;

GST_DEBUG_CATEGORY (defectdetect_app);
#define GST_CAT_DEFAULT defectdetect_app

#define PRE_PROCESS_JSON_FILE        "preprocess-accelarator.json"
#define PRE_PROCESS_STRIDE_JSON_FILE "preprocess-accelarator-stride.json"
#define OTSU_ACC_JSON_FILE           "otsu-accelarator.json"
#define CCA_ACC_JSON_FILE            "cca-accelarator.json"
#define TEXT_2_OVERLAY_JSON_FILE     "text2overlay.json"
#define DRM_BUS_ID                   "fd4a0000.display"
#define CAPTURE_FORMAT_Y8            "GRAY8"
#define MAX_WIDTH                    1920
#define MAX_HEIGHT                   1080
#define MAX_FRAME_RATE_DENOM         1
#define MAX_DEMO_MODE_FRAME_RATE     4

typedef enum {
    DD_SUCCESS,
    DD_ERROR_FILE_IO = -1,
    DD_ERROR_PIPELINE_CREATE_FAIL = -2,
    DD_ERROR_PIPELINE_LINKING_FAIL = -3,
    DD_ERROR_STATE_CHANGE_FAIL = -4,
    DD_ERROR_RESOLUTION_NOT_SUPPORTED = -5,
    DD_ERROR_INPUT_OPTIONS_INVALID = -6,
    DD_ERROR_OVERLAY_CREATION_FAIL = -7,
    DD_ERROR_FILE_DUMP_IN_DEMO_NOT_SUPPORTED = -8,
    DD_ERROR_OTHER = -99,
} DD_ERROR_LOG;

typedef struct _AppData {
    GstElement *pipeline, *capsfilter, *src, *rawvparse;
    GstElement *sink;
    GstElement *queue, *q_raw,*q_raw_1;
    GstElement *perf, *videorate;
    GstElement *preprocess, *otsu, *cca, *text2overlay;
    GstElement *capsfilter_vr, *capsfilter_op;
} AppData;

GMainLoop *loop;
gboolean file_playback = FALSE;
gboolean file_dump = FALSE;
gboolean demo_mode = FALSE;
static gchar* in_file = NULL;
static gchar* config_path  = (gchar *)"/opt/xilinx/xlnx-app-kr260-mv-defect-detect/share/vvas/";
static gchar *msg_firmware = (gchar *)"Load the HW accelerator firmware first. Use command: xmutil loadapp kr260-mv-camera\n";
static gchar* out_file = NULL;
guint width =  1920;
guint height = 1080;
guint stride_align = 256;
guint dp = 0;
guint framerate = 60;
static string dev_node("");

static GOptionEntry entries[] =
{
    { "infile",       'i', 0, G_OPTION_ARG_FILENAME, &in_file, "Location of input file", "file path"},
    { "outfile",      'f', 0, G_OPTION_ARG_FILENAME, &out_file, "Location of output file", "file path"},
    { "width",        'w', 0, G_OPTION_ARG_INT, &width, "Resolution width of the input", "1920"},
    { "height",       'h', 0, G_OPTION_ARG_INT, &height, "Resolution height of the input", "1080"},
    { "output",       'o', 0, G_OPTION_ARG_INT, &dp, "Display/dump stage on DP/File", "0"},
    { "framerate",    'r', 0, G_OPTION_ARG_INT, &framerate, "Framerate of input source", "60"},
    { "demomode",     'd', 0, G_OPTION_ARG_INT, &demo_mode, "For Demo mode value must be 1", "0"},
    { "cfgpath",      'c', 0, G_OPTION_ARG_STRING, &config_path, "JSON config file path", "/opt/xilinx/xlnx-app-kr260-mv-defect-detect/share/vvas/"},
    { NULL }
};

/* Handler for the pad-added signal */
static void pad_added_cb (GstElement *src, GstPad *pad, AppData *data);

DD_ERROR_LOG set_pipeline_config (AppData *data);

const gchar * error_to_string (gint error_code);

DD_ERROR_LOG create_pipeline (AppData *data);

DD_ERROR_LOG link_pipeline (AppData *data);

void
signal_handler (gint sig) {
     signal(sig, SIG_IGN);
     GST_DEBUG ("Hit Ctrl-C, Quitting the app now");
     if (loop && g_main_loop_is_running (loop)) {
        GST_DEBUG ("Quitting the loop");
        g_main_loop_quit (loop);
     }
     return;
}

static std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

/** @brief
 *  This function is the callback function required to hadnle the
 *  incoming bus messages.
 *
 *  This function will be receiving bus message from different elements
 *  used in the pipeline.
 *
 *  @param bus is an object responsible for delivering GstMessage packets
 *  in a first-in first-out way from the streaming threads
 *  @param msg is the structure which holds the required information
 *  @param data is the application structure.
 *  @return gboolean.
 */
static gboolean
message_cb (GstBus *bus, GstMessage *msg, AppData *data) {
    GError *err;
    gchar *debug;
    switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_INFO:
        gst_message_parse_info (msg, &err, &debug);
        if (debug)
            GST_INFO ("INFO: %s", debug);
    break;
    case GST_MESSAGE_ERROR:
        gst_message_parse_error (msg, &err, &debug);
        g_printerr ("Error: %s\n", err->message);
        g_error_free (err);
        g_free (debug);
        if (loop && g_main_loop_is_running (loop)) {
            GST_DEBUG ("Quitting the loop");
            g_main_loop_quit (loop);
        }
    break;
    case GST_MESSAGE_EOS:
        /* end-of-stream */
        GST_DEBUG ("End Of Stream");
        if (loop && g_main_loop_is_running (loop)) {
            GST_DEBUG ("Quitting the loop");
            g_main_loop_quit (loop);
        }
    break;
    default:
      /* Unhandled message */
      break;
    }
    return TRUE;
}

/** @brief
 *  This function will be called to convert the error number to
 *  meaningful string.
 *
 *  This function will be called to convert the error number to
 *  meaningful string so that, user can easily understand the issue
 *  and fix the issue.
 *
 *  @param error_code The integer error which has to be translated
 *  into string.
 *  @return string.
 */
const gchar *
error_to_string (gint error_code) {
    switch (error_code) {
        case DD_SUCCESS :
            return "Success";
        case DD_ERROR_FILE_IO :
            return "File I/O Error";
        case DD_ERROR_PIPELINE_CREATE_FAIL :
            return "pipeline creation failed";
        case DD_ERROR_PIPELINE_LINKING_FAIL :
            return "pipeline linking failed";
        case DD_ERROR_STATE_CHANGE_FAIL :
            return "state change failed";
        case DD_ERROR_RESOLUTION_NOT_SUPPORTED :
            return "Resolution WxH should be 1920x1080";
        case DD_ERROR_INPUT_OPTIONS_INVALID :
            return "Input options are incorrect";
        case DD_ERROR_OVERLAY_CREATION_FAIL :
            return "overlay creation is failed for the display";
	case DD_ERROR_FILE_DUMP_IN_DEMO_NOT_SUPPORTED:
	    return "Demo mode is not supported for file sink";
        default :
            return "Unknown Error";
    }
    return "Unknown Error";
}
void
on_deep_element_added (GstBin *bin,
                       GstBin *sub_bin,
                       GstElement *element,
                       gpointer user_data) {
    guint *enable_roi = (guint *) user_data;
    GstElementFactory *factory = gst_element_get_factory (element);
    const gchar *klass = gst_element_factory_get_klass(factory);
    if (!g_strcmp0 (klass, "Source/Video") && dp ==0 && !file_dump) {
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(element), "io-mode")) {
            GST_DEBUG ("Setting io-mode to dmabuf-import");
            g_object_set (G_OBJECT(element), "io-mode", 5, NULL);
        }
    }
}
/** @brief
 *  This function is to set the GstElement properties.
 *
 *  All gstreamer element has some properties and to work
 *  need to set few of the properties. This function does the
 *  same.
 *
 *  @param data is the application structure.
 *  @return Error code.
 */
DD_ERROR_LOG
set_pipeline_config (AppData *data) {
    gint block_size;
    GstCaps *caps;
    string config_file(config_path);
    gint ret = DD_SUCCESS;
    if (file_playback) {
        block_size = width * height;
        g_object_set(G_OBJECT(data->src),            "location",  in_file,         NULL);
        g_object_set(G_OBJECT(data->src),            "blocksize", block_size,      NULL);
    } else {
        g_object_set(G_OBJECT(data->src),            "media-device", "/dev/media0", NULL);
        g_signal_connect (GST_BIN (data->src),       "deep-element-added", G_CALLBACK (on_deep_element_added), NULL);
    }
    if (file_dump) {
        g_object_set(G_OBJECT(data->sink),       "location",  out_file,        NULL);
        //g_object_set(G_OBJECT(data->sink_preprocess),"location",  preprocess_out, NULL);
        //g_object_set(G_OBJECT(data->sink_display),   "location",  final_out,      NULL);
    } else {
        g_object_set(G_OBJECT(data->sink),          "bus-id",       DRM_BUS_ID,  NULL);
        if (demo_mode) {
            caps  = gst_caps_new_simple ("video/x-raw",
                                         "framerate", GST_TYPE_FRACTION, MAX_DEMO_MODE_FRAME_RATE, MAX_FRAME_RATE_DENOM,
                                         NULL);
            GST_DEBUG ("new Caps for raw capsfilter %" GST_PTR_FORMAT, caps);
            g_object_set (G_OBJECT (data->capsfilter_vr),  "caps",  caps, NULL);
            gst_caps_unref (caps);
            if (file_playback) {
                g_object_set (G_OBJECT (data->rawvparse),  "use-sink-caps", FALSE,                    NULL);
                g_object_set (G_OBJECT (data->rawvparse),  "width",         width,                    NULL);
                g_object_set (G_OBJECT (data->rawvparse),  "height",        height,                   NULL);
                g_object_set (G_OBJECT (data->rawvparse),  "format",        GST_VIDEO_FORMAT_GRAY8,     NULL);
                g_object_set (G_OBJECT (data->rawvparse),  "framerate",     MAX_DEMO_MODE_FRAME_RATE, MAX_FRAME_RATE_DENOM, NULL);
            }
        }
    }
    caps  = gst_caps_new_simple ("video/x-raw",
                                 "width",     G_TYPE_INT,        width,
                                 "height",    G_TYPE_INT,        height,
                                 "format",    G_TYPE_STRING,     CAPTURE_FORMAT_Y8,
                                 "framerate", GST_TYPE_FRACTION, framerate, MAX_FRAME_RATE_DENOM,
                                 NULL);
    GST_DEBUG ("new Caps for src capsfilter %" GST_PTR_FORMAT, caps);
    g_object_set (G_OBJECT (data->capsfilter),  "caps",  caps, NULL);
    gst_caps_unref (caps);


    caps  = gst_caps_new_simple ("video/x-raw",
                                 "width",     G_TYPE_INT,        width,
                                 "height",    G_TYPE_INT,        height,
                                 "stride-align",    G_TYPE_INT,  stride_align,
                                 "format",    G_TYPE_STRING,     CAPTURE_FORMAT_Y8,
                                 NULL);
    GST_DEBUG ("new Caps for src capsfilter_op %" GST_PTR_FORMAT, caps);

    g_object_set (G_OBJECT (data->capsfilter_op),  "caps",  caps, NULL);
    gst_caps_unref (caps);
    if (file_dump){
    config_file.append(PRE_PROCESS_JSON_FILE);
    }else{
    config_file.append(PRE_PROCESS_STRIDE_JSON_FILE);
    }
    g_object_set (G_OBJECT(data->preprocess), "kernels-config", config_file.c_str(), NULL);
    GST_DEBUG ("Config file path is %s", config_file.c_str());

    config_file.erase (config_file.begin()+ strlen(config_path), config_file.end()-0);
    config_file.append(OTSU_ACC_JSON_FILE);
    g_object_set (G_OBJECT(data->otsu),   "kernels-config", config_file.c_str(), NULL);
    GST_DEBUG ("Config file path is %s", config_file.c_str());

    config_file.erase (config_file.begin()+ strlen(config_path), config_file.end()-0);
    config_file.append(CCA_ACC_JSON_FILE);
    g_object_set (G_OBJECT(data->cca),    "kernels-config", config_file.c_str(), NULL);
    GST_DEBUG ("Config file path is %s", config_file.c_str());

    config_file.erase (config_file.begin()+ strlen(config_path), config_file.end()-0);
    config_file.append(TEXT_2_OVERLAY_JSON_FILE);
    g_object_set (G_OBJECT(data->text2overlay), "kernels-config", config_file.c_str(), NULL);
    GST_DEBUG ("Config file path is %s", config_file.c_str());
    return DD_SUCCESS;
}

/** @brief
 *  This function is to link all the elements required to run defect
 *  detect use case.
 *
 *  Link live playback pipeline.
 *
 *  @param data is the application structure pointer.
 *  @return Error code.
 */
DD_ERROR_LOG
link_pipeline (AppData *data) {
    gchar *name1, *name2;
    gint ret = DD_SUCCESS;
    if (!file_playback && !demo_mode) {
        if (dp ==0) {
            if (!gst_element_link_many(data->capsfilter, data->q_raw, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for capsfilter --> queue --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linked for capsfilter --> queue --> perf --> sink successfully");
        } else if (dp ==1) {
            if (!gst_element_link_many(data->capsfilter, data->otsu, data->queue, data->preprocess, data->capsfilter_op, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for capsfilter --> otsu --> queue --> preprocess --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linking for capsfilter --> otsu --> queue --> preprocess --> perf --> sink successfully");
        } else if (dp ==2) {
            if (!gst_element_link_many(data->capsfilter, data->otsu, data->queue, data->preprocess, data->q_raw, data->cca, \
                                        /*data->q_raw,*/ data->text2overlay, data->q_raw_1,data->capsfilter_op, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for caps --> otsu --> q --> preprocess --> q --> cca --> text2overlay --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linking for caps --> otsu --> q --> preprocess --> q --> cca --> text2overlay --> perf --> sink successfully");
        }
    } else if (!file_playback && demo_mode) {
        if (dp ==0) {
            if (!gst_element_link_many(data->capsfilter, data->videorate, data->capsfilter_vr, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for capsfilter --> videorate --> capfilter --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linked for capsfilter --> videorate --> capfilter --> perf --> sink successfully");
        } else if (dp ==1) {
            if (!gst_element_link_many(data->capsfilter, data->otsu, data->preprocess, data->videorate, \
                                       data->capsfilter_vr, data->capsfilter_op, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for caps --> otsu --> preprocess --> rate --> caps --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linking for caps --> otsu --> preprocess --> rate --> caps --> perf --> sink successfully");
        } else if (dp ==2) {
            if (!gst_element_link_many(data->capsfilter, data->otsu,data->queue, data->preprocess,data->q_raw, data->cca, data->text2overlay, \
                                       data->q_raw_1, data->videorate, data->capsfilter_vr, data->capsfilter_op, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for caps --> otsu --> pp --> cca --> text2overlay --> rate --> caps --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linking for caps --> otsu --> pp --> cca --> text2overlay --> rate --> caps --> perf --> sink successfully");
        } 
    } else if (file_playback && !demo_mode) {
        if (dp == 0) {
            if (!gst_element_link_many(data->src, data->capsfilter, data->q_raw, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for src --> caps --> queue --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linked for src --> caps --> queue --> perf --> sink successfully");
        } else if (dp ==1) {
            if (!gst_element_link_many(data->src, data->capsfilter, data->otsu, data->queue, data->preprocess, \
                                       data->capsfilter_op, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for src --> caps --> otsu --> queue --> preprocess --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linking for src --> caps --> otsu --> queue --> preprocess --> perf --> sink successfully");
        } else if (dp ==2) {
            if (!gst_element_link_many(data->src, data->queue, data->capsfilter, data->otsu/*, data->queue*/, data->preprocess, data->q_raw, \
                                       data->cca, data->text2overlay, data->q_raw_1, data->capsfilter_op, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for src --> caps --> otsu --> q --> pp --> q --> cca --> text2overlay --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linking for src --> caps --> otsu --> q --> pp --> q --> cca --> text2overlay --> perf --> sink successfully");
        }
    } else if (file_playback && demo_mode) {
        if (dp ==0) {
            if (!gst_element_link_many(data->src,data->capsfilter, data->rawvparse, data->capsfilter_op, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for src --> vparse --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linked for src --> vparse --> perf --> sink successfully");
        } else if (dp ==1) {
            if (!gst_element_link_many(data->src, data->rawvparse, data->otsu, data->preprocess, \
                                       data->capsfilter_op, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for src --> vparse --> otsu --> preprocess --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linking for src --> vparse --> otsu --> preprocess --> perf --> sink successfully");
        } else if (dp ==2) {
            if (!gst_element_link_many(data->src, data->rawvparse, data->otsu, data->queue, data->preprocess,data->q_raw, data->cca, data->text2overlay, \
                                       data->q_raw_1, data->capsfilter_op, data->perf, data->sink, NULL)) {
                GST_ERROR ("Error linking for src --> vparse --> otsu --> preprocess --> cca --> text2overlay --> perf --> sink");
                return DD_ERROR_PIPELINE_LINKING_FAIL;
            }
            GST_DEBUG ("Linking for src --> vparse --> otsu --> preprocess --> cca --> text2overlay --> perf --> sink successfully");
        }
    }
    return DD_SUCCESS;
}

/** @brief
 *  This function is to create a pipeline required to run defect
 *  detect use case.
 *
 *  All GStreamer elements instance has to be created and add
 *  into the pipeline bin. Also link file based pipeline.
 *
 *  @param data is the application structure pointer.
 *  @return Error code.
 */
DD_ERROR_LOG
create_pipeline (AppData *data) {
    data->pipeline =   gst_pipeline_new("defectdetection");
    if (file_playback) {
        data->src               =  gst_element_factory_make("filesrc",      NULL);
    } else {
        data->src               =  gst_element_factory_make("mediasrcbin",  NULL);
    }
    if (file_dump) {
        data->sink          =  gst_element_factory_make("filesink",     NULL);
        //data->sink_preprocess   =  gst_element_factory_make("filesink",     NULL);
        //data->sink_display      =  gst_element_factory_make("filesink",     NULL);
    } else {
        if (dp == 0) {
            data->sink     =  gst_element_factory_make("kmssink",       "display-raw");
        } else if (dp == 1) {
            data->sink     =  gst_element_factory_make("kmssink",       "display-preprocess");
        } else if (dp == 2) {
            data->sink     =  gst_element_factory_make("kmssink",       "display-final");
        }
    }
    data->capsfilter      =  gst_element_factory_make("capsfilter",    NULL);
    data->capsfilter_vr   =  gst_element_factory_make("capsfilter",    NULL);
    data->capsfilter_op   =  gst_element_factory_make("capsfilter",    NULL);
    data->rawvparse       =  gst_element_factory_make("rawvideoparse", NULL);
    data->preprocess      =  gst_element_factory_make("vvas_xfilter",  "pre-process");
    data->otsu            =  gst_element_factory_make("vvas_xfilter",  "otsu");
    data->cca             =  gst_element_factory_make("vvas_xfilter",  "cca");
    data->text2overlay    =  gst_element_factory_make("vvas_xfilter",  "text2overlay");
    data->queue           =  gst_element_factory_make("queue",         NULL);
    data->q_raw           =  gst_element_factory_make("queue",         NULL);
    data->q_raw_1           =  gst_element_factory_make("queue",         NULL);
    data->perf            =  gst_element_factory_make("perf",          "perf-raw");
    data->videorate       =  gst_element_factory_make("videorate",     NULL);

    if (!data->pipeline || !data->src || !data->rawvparse \
        || !data->preprocess || !data->otsu || !data->cca || !data->text2overlay \
        || !data->sink \
        || !data->queue \
        || !data->q_raw \
        || !data->q_raw_1 \
        || !data->perf || !data->videorate || !data->capsfilter || !data->capsfilter_vr || !data->capsfilter_op) {
           GST_ERROR ("could not create few elements");
           return DD_ERROR_PIPELINE_CREATE_FAIL;
    }

    GST_DEBUG ("All elements are created");
    gst_bin_add_many(GST_BIN(data->pipeline), data->src, data->rawvparse, \
                     data->preprocess, data->otsu, data->cca, data->text2overlay, \
                     data->sink, data->q_raw, data->q_raw_1,\
                     data->perf, data->videorate, \
                     data->capsfilter, data->capsfilter_vr, data->capsfilter_op, data->queue, NULL);
    return DD_SUCCESS;
}

static std::string
find_capture_dev() {
    glob_t globbuf;

    glob("/dev/media*", 0, NULL, &globbuf);
    for (int i = 0; i < globbuf.gl_pathc; i++) {
        std::ostringstream cmd;
        cmd << "media-ctl -d " << globbuf.gl_pathv[i] << " -p | grep driver | grep xilinx-video | wc -l";

        std::string a = exec(cmd.str().c_str());
        a=a.substr(0, a.find("\n"));
        if ( a == std::string("1")) {
            dev_node = globbuf.gl_pathv[i];
            break;
        }
    }
    globfree(&globbuf);
    return dev_node;
}

static gint
check_capture_src() {
    std::string capturedev("");
    capturedev = find_capture_dev();
    if (capturedev == "") {
        g_printerr("ERROR: Capture device is not ready.\n%s", msg_firmware);
        return 1;
    }

    if ( access( capturedev.c_str(), F_OK ) != 0) {
        g_printerr("ERROR: Device %s is not ready.\n%s", capturedev.c_str(), msg_firmware);
        return 1;
    }
    return 0;
}

gint
main (int argc, char **argv) {
    AppData data;
    GstBus *bus;
    gint ret = DD_SUCCESS;
    guint bus_watch_id;
    GOptionContext *optctx;
    GError *error = NULL;

    memset (&data, 0, sizeof(AppData));

    gst_init(&argc, &argv);
    signal(SIGINT, signal_handler);

    GST_DEBUG_CATEGORY_INIT (defectdetect_app, "defectdetect-app", 0, "defect detection app");
    optctx = g_option_context_new ("- Application to detect the defect of Mango on Xilinx board");
    g_option_context_add_main_entries (optctx, entries, NULL);
    g_option_context_add_group (optctx, gst_init_get_option_group ());
    if (!g_option_context_parse (optctx, &argc, &argv, &error)) {
        g_printerr ("Error parsing options: %s\n", error->message);
        g_option_context_free (optctx);
        g_clear_error (&error);
        return -1;
    }
    g_option_context_free (optctx);

    if (in_file) {
        file_playback = TRUE;
    }

    if (out_file) {
        file_dump = true;
    }

    if (in_file) {
        GST_DEBUG ("In file is %s", in_file);
    }

    GST_DEBUG ("Width : %d", width);
    GST_DEBUG ("Height : %d", height);
    GST_DEBUG ("Framerate : %d", framerate);
    GST_DEBUG ("Display stage : %d", dp);
    GST_DEBUG ("File playback mode : %s", file_playback ? "TRUE" : "FALSE");
    GST_DEBUG ("File dump : %s", file_dump ? "TRUE" : "FALSE");
    GST_DEBUG ("Demo mode : %s", demo_mode ? "On" : "Off");

    if (config_path)
        GST_DEBUG ("config path is %s", config_path);

    if (dev_node.c_str())
        GST_DEBUG ("media node is %s", dev_node.c_str());

    if (width > MAX_WIDTH || height > MAX_HEIGHT) {
        ret = DD_ERROR_RESOLUTION_NOT_SUPPORTED;
        g_printerr ("Exiting the app with an error: %s\n", error_to_string (ret));
        return ret;
    }
    if (demo_mode && file_dump){
        ret = DD_ERROR_FILE_DUMP_IN_DEMO_NOT_SUPPORTED;
        g_printerr ("Exiting the app with an error: %s\n", error_to_string (ret));
        return ret;
    }

    if (access("/dev/dri/by-path/platform-fd4a0000.display-card", F_OK) != 0) {
        g_printerr("ERROR: Mixer device is not ready.\n%s", msg_firmware);
        return -1;
    } else {
        exec("echo | modetest -M xlnx -D fd4a0000.display -s 43@41:1920x1080-60@BG24 -w 40:\"alpha\":255");
    }

    if (!file_playback && (check_capture_src() != 0)) {
        g_printerr ("Media node not found, please check the connection of camera\n");
        return -1;
    }
    if (!file_playback ) {
        std::string script_caller;
        GST_DEBUG ("Calling default sensor calibration script");
        script_caller = "echo | configure " + dev_node;
        exec(script_caller.c_str());
    }
    ret = create_pipeline (&data);
    if (ret != DD_SUCCESS) {
        g_printerr ("Exiting the app with an error: %s\n", error_to_string (ret));
        return ret;
    }

    ret = link_pipeline (&data);
    if (ret != DD_SUCCESS) {
        g_printerr ("Exiting the app with an error: %s\n", error_to_string (ret));
        return ret;
    }

    ret = set_pipeline_config (&data);
    if (ret != DD_SUCCESS) {
        g_printerr ("Exiting the app with an error: %s\n", error_to_string (ret));
        return ret;
    }

    /* we add a message handler */
    bus = gst_pipeline_get_bus (GST_PIPELINE (data.pipeline));
    bus_watch_id = gst_bus_add_watch (bus, (GstBusFunc)(message_cb), &data);
    gst_object_unref (bus);

    if (!file_playback) {
        g_signal_connect (data.src, "pad-added", G_CALLBACK (pad_added_cb), &data);
    }
    GST_DEBUG ("Triggering play command");
    if (GST_STATE_CHANGE_FAILURE == gst_element_set_state (data.pipeline, GST_STATE_PLAYING)) {
        g_printerr ("state change to Play failed\n");
        goto CLOSE;
    }
    GST_DEBUG ("waiting for the loop");
    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);
CLOSE:
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    if (data.pipeline) {
        gst_object_unref (GST_OBJECT (data.pipeline));
        data.pipeline = NULL;
    }
    GST_DEBUG ("Removing bus");
    g_source_remove (bus_watch_id);

    if (in_file)
        g_free (in_file);
    if (out_file)
        g_free (out_file);
    return ret;
}

/** @brief
 *  This function will be called by the pad-added signal
 *
 *  mediasrcbin has sometimes pad for which pad-added signal
 *  is required to be attached with source element.
 *  This function will be called when pad is created and is
 *  ready to link with peer element.
 *
 *  @param src The GstElement to be connected with peer element.
 *  @param new_pad is the pad of source element which needs to be
 *  linked.
 *  @param data is the application structure pointer.
 *  @return Void.
 */
static void
pad_added_cb (GstElement *src, GstPad *new_pad, AppData *data) {
    GstPadLinkReturn ret;
    GstPad *sink_pad = gst_element_get_static_pad (data->capsfilter, "sink");
    GST_DEBUG ("Received new pad '%s' from '%s':", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

    /* If our capsfilter is already linked, we have nothing to do here */
    if (gst_pad_is_linked (sink_pad)) {
        GST_DEBUG ("Pad is already linked. Ignoring");
        goto exit;
    }

    /* Attempt the link */
    ret = gst_pad_link (new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED (ret)) {
        GST_ERROR ("Linking failed");
    }
exit:
    /* Unreference the sink pad */
    gst_object_unref (sink_pad);
}
