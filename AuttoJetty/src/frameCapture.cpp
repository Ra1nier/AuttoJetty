#include "../include/frameCapture.h"

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <mutex>
#include <vector>

#include <libportal/portal.h>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <pipewire/pipewire.h>
#include <spa/param/buffers.h>
#include <spa/param/format-utils.h>
#include <spa/param/video/raw-utils.h>
#include <spa/pod/builder.h>

namespace
{
int promptInt(const std::string& label, int currentValue)
{
    // Empty input keeps the current coordinate so small setup tweaks are quick.
    cout << label << " [" << currentValue << "]: ";
    std::string input;
    std::getline(std::cin >> std::ws, input);
    if (input.empty())
    {
        return currentValue;
    }

    return std::stoi(input);
}

cv::Mat fitPreview(const cv::Mat& image, int maxWidth, int maxHeight, double& scale)
{
    // Scale large desktop frames down to fit the setup window without changing aspect ratio.
    scale = std::min(
        static_cast<double>(maxWidth) / image.cols,
        static_cast<double>(maxHeight) / image.rows);
    scale = std::min(scale, 1.0);

    cv::Mat preview;
    if (scale < 1.0)
    {
        cv::resize(image, preview, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    else
    {
        preview = image.clone();
    }

    return preview;
}

uint32_t firstStreamNodeId(XdpSession* session)
{
    // The desktop portal can return multiple streams; AuttoJetty uses the first selected stream.
    GVariant* streams = xdp_session_get_streams(session);
    if (!streams)
    {
        return SPA_ID_INVALID;
    }

    GVariantIter iter;
    guint32 nodeId = SPA_ID_INVALID;
    GVariant* properties = nullptr;
    g_variant_iter_init(&iter, streams);
    if (g_variant_iter_next(&iter, "(u@a{sv})", &nodeId, &properties))
    {
        if (properties)
        {
            g_variant_unref(properties);
        }
    }
    g_variant_unref(streams);
    return nodeId;
}
}

struct PipeWireCapture
{
    XdpPortal* portal = nullptr;
    XdpSession* session = nullptr;
    GMainLoop* portalLoop = nullptr;
    GError* portalError = nullptr;
    uint32_t nodeId = SPA_ID_INVALID;

    pw_thread_loop* loop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    pw_stream* stream = nullptr;
    spa_hook streamListener{};
    spa_video_info_raw videoInfo{};

    std::mutex frameMutex;
    std::condition_variable frameReady;
    cv::Mat latestFrame;

    bool initialize()
    {
        if (!startPortalSession())
        {
            return false;
        }

        int pipewireFd = xdp_session_open_pipewire_remote(session);
        if (pipewireFd < 0)
        {
            std::cerr << "Portal did not provide a PipeWire remote." << std::endl;
            return false;
        }

        return startPipeWire(pipewireFd);
    }

    ~PipeWireCapture()
    {
        cv::destroyAllWindows();

        if (loop)
        {
            pw_thread_loop_stop(loop);
        }
        if (stream)
        {
            if (loop)
            {
                pw_thread_loop_lock(loop);
            }
            pw_stream_disconnect(stream);
            pw_stream_destroy(stream);
            if (loop)
            {
                pw_thread_loop_unlock(loop);
            }
            stream = nullptr;
        }
        if (core)
        {
            pw_core_disconnect(core);
            core = nullptr;
        }
        if (context)
        {
            pw_context_destroy(context);
            context = nullptr;
        }
        if (loop)
        {
            pw_thread_loop_destroy(loop);
            loop = nullptr;
        }
        if (session)
        {
            xdp_session_close(session);
            g_object_unref(session);
        }
        if (portal)
        {
            g_object_unref(portal);
            portal = nullptr;
        }
        pw_deinit();
    }

    bool getFrame(cv::Mat& frame)
    {
        std::unique_lock<std::mutex> lock(frameMutex);
        if (!frameReady.wait_for(lock, std::chrono::seconds(3), [&] { return !latestFrame.empty(); }))
        {
            return false;
        }

        frame = latestFrame.clone();
        return true;
    }

    static void onCreateSession(GObject* source, GAsyncResult* result, gpointer data)
    {
        PipeWireCapture* self = static_cast<PipeWireCapture*>(data);
        self->session = xdp_portal_create_screencast_session_finish(
            XDP_PORTAL(source),
            result,
            &self->portalError);

        if (!self->session)
        {
            g_main_loop_quit(self->portalLoop);
            return;
        }

        xdp_session_start(self->session, nullptr, nullptr, onStartSession, self);
    }

    static void onStartSession(GObject* source, GAsyncResult* result, gpointer data)
    {
        PipeWireCapture* self = static_cast<PipeWireCapture*>(data);
        if (!xdp_session_start_finish(XDP_SESSION(source), result, &self->portalError))
        {
            g_main_loop_quit(self->portalLoop);
            return;
        }

        self->nodeId = firstStreamNodeId(self->session);
        g_main_loop_quit(self->portalLoop);
    }

    bool startPortalSession()
    {
        portal = xdp_portal_new();
        portalLoop = g_main_loop_new(nullptr, FALSE);

        cout << "Requesting screen capture permission from the desktop portal." << std::endl;
        xdp_portal_create_screencast_session(
            portal,
            static_cast<XdpOutputType>(XDP_OUTPUT_MONITOR | XDP_OUTPUT_WINDOW),
            XDP_SCREENCAST_FLAG_NONE,
            XDP_CURSOR_MODE_HIDDEN,
            XDP_PERSIST_MODE_TRANSIENT,
            nullptr,
            nullptr,
            onCreateSession,
            this);

        g_main_loop_run(portalLoop);
        g_main_loop_unref(portalLoop);
        portalLoop = nullptr;

        if (portalError)
        {
            std::cerr << "Portal screen capture failed: " << portalError->message << std::endl;
            g_clear_error(&portalError);
            return false;
        }

        if (!session || nodeId == SPA_ID_INVALID)
        {
            std::cerr << "No PipeWire stream was selected from the portal dialog." << std::endl;
            return false;
        }

        cout << "Portal stream selected. PipeWire node id: " << nodeId << std::endl;
        return true;
    }

    bool startPipeWire(int pipewireFd)
    {
        pw_init(nullptr, nullptr);

        loop = pw_thread_loop_new("auttojetty-pipewire", nullptr);
        if (!loop)
        {
            std::cerr << "Failed to create PipeWire thread loop." << std::endl;
            return false;
        }

        context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
        if (!context)
        {
            std::cerr << "Failed to create PipeWire context." << std::endl;
            return false;
        }

        core = pw_context_connect_fd(context, pipewireFd, nullptr, 0);
        if (!core)
        {
            std::cerr << "Failed to connect to the portal PipeWire remote." << std::endl;
            return false;
        }

        stream = pw_stream_new(core, "AuttoJetty capture", pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Video",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Screen",
            nullptr));
        if (!stream)
        {
            std::cerr << "Failed to create PipeWire stream." << std::endl;
            return false;
        }

        static const pw_stream_events streamEvents = {
            PW_VERSION_STREAM_EVENTS,
            nullptr,
            onStreamStateChanged,
            nullptr,
            nullptr,
            onParamChanged,
            nullptr,
            nullptr,
            onProcess,
            nullptr,
            nullptr,
            nullptr
        };
        pw_stream_add_listener(stream, &streamListener, &streamEvents, this);

        uint8_t buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        spa_video_info_raw format{};
        format.format = SPA_VIDEO_FORMAT_BGRx;
        const spa_pod* params[1];
        params[0] = spa_format_video_raw_build(&builder, SPA_PARAM_EnumFormat, &format);

        int result = pw_stream_connect(
            stream,
            PW_DIRECTION_INPUT,
            nodeId,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
            params,
            1);
        if (result < 0)
        {
            std::cerr << "Failed to connect PipeWire stream." << std::endl;
            return false;
        }

        if (pw_thread_loop_start(loop) < 0)
        {
            std::cerr << "Failed to start PipeWire thread loop." << std::endl;
            return false;
        }

        return true;
    }

    static void onStreamStateChanged(void*, pw_stream_state, pw_stream_state state, const char* error)
    {
        cout << "PipeWire stream state: " << pw_stream_state_as_string(state);
        if (error)
        {
            cout << " (" << error << ")";
        }
        cout << std::endl;
    }

    static void onParamChanged(void* data, uint32_t id, const spa_pod* param)
    {
        PipeWireCapture* self = static_cast<PipeWireCapture*>(data);
        if (!param || id != SPA_PARAM_Format)
        {
            return;
        }

        if (spa_format_video_raw_parse(param, &self->videoInfo) < 0)
        {
            return;
        }

        cout << "PipeWire video format: " << self->videoInfo.size.width << "x" << self->videoInfo.size.height << std::endl;

        uint8_t buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        const int stride = SPA_ROUND_UP_N(static_cast<int>(self->videoInfo.size.width) * 4, 4);
        const spa_pod* params[1];
        params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&builder,
            SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
            SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(8, 2, 16),
            SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
            SPA_PARAM_BUFFERS_size, SPA_POD_Int(stride * static_cast<int>(self->videoInfo.size.height)),
            SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
            SPA_PARAM_BUFFERS_align, SPA_POD_Int(16)));
        pw_stream_update_params(self->stream, params, 1);
    }

    static void onProcess(void* data)
    {
        PipeWireCapture* self = static_cast<PipeWireCapture*>(data);
        pw_buffer* pipewireBuffer = pw_stream_dequeue_buffer(self->stream);
        if (!pipewireBuffer)
        {
            return;
        }

        spa_buffer* buffer = pipewireBuffer->buffer;
        if (!buffer || buffer->n_datas == 0 || !buffer->datas[0].data)
        {
            pw_stream_queue_buffer(self->stream, pipewireBuffer);
            return;
        }

        spa_data& spaData = buffer->datas[0];
        const uint8_t* source = static_cast<const uint8_t*>(spaData.data);
        uint32_t offset = spaData.chunk ? spaData.chunk->offset : 0;
        int stride = spaData.chunk && spaData.chunk->stride > 0
            ? spaData.chunk->stride
            : static_cast<int>(self->videoInfo.size.width) * 4;

        int width = static_cast<int>(self->videoInfo.size.width);
        int height = static_cast<int>(self->videoInfo.size.height);
        if (width <= 0 || height <= 0)
        {
            pw_stream_queue_buffer(self->stream, pipewireBuffer);
            return;
        }

        cv::Mat sourceFrame(height, width, CV_8UC4, const_cast<uint8_t*>(source + offset), stride);
        cv::Mat bgrFrame;
        switch (self->videoInfo.format)
        {
            case SPA_VIDEO_FORMAT_BGRA:
            case SPA_VIDEO_FORMAT_BGRx:
                cv::cvtColor(sourceFrame, bgrFrame, cv::COLOR_BGRA2BGR);
                break;
            case SPA_VIDEO_FORMAT_RGBA:
            case SPA_VIDEO_FORMAT_RGBx:
                cv::cvtColor(sourceFrame, bgrFrame, cv::COLOR_RGBA2BGR);
                break;
            default:
                cv::cvtColor(sourceFrame, bgrFrame, cv::COLOR_BGRA2BGR);
                break;
        }

        {
            std::lock_guard<std::mutex> lock(self->frameMutex);
            self->latestFrame = bgrFrame.clone();
        }
        self->frameReady.notify_one();

        pw_stream_queue_buffer(self->stream, pipewireBuffer);
    }
};

FrameCapture::FrameCapture(int x, int y, int width, int height)
    : x(x), y(y), width(width), height(height), capture(std::make_unique<PipeWireCapture>())
{
    if (!capture->initialize())
    {
        std::cerr << "PipeWire portal capture initialization failed." << std::endl;
    }
}

FrameCapture::~FrameCapture() = default;

tuple<Mat, Mat> FrameCapture::captureFrame()
{
    // Read one full desktop frame and crop the two regions the player expects.
    Mat screen;
    if (!captureScreen(screen))
    {
        std::cerr << "Failed to read a frame from the PipeWire stream." << std::endl;
        return {{}, {}};
    }

    Mat gameFrame = cropRegion(screen, x, y, width, height);
    Mat stateFrame = cropRegion(screen, stateX, stateY, stateWidth, stateHeight);
    
    return {gameFrame, stateFrame};
}

bool FrameCapture::captureScreen(Mat& screen)
{
    return capture && capture->getFrame(screen);
}

Mat FrameCapture::cropRegion(const Mat& screen, int captureX, int captureY, int captureWidth, int captureHeight)
{
    // Clip requested coordinates so slightly off-screen state regions do not crash OpenCV.
    cv::Rect screenBounds(0, 0, screen.cols, screen.rows);
    cv::Rect requested(captureX, captureY, captureWidth, captureHeight);
    cv::Rect clipped = requested & screenBounds;

    if (clipped.empty())
    {
        return {};
    }

    return screen(clipped).clone();
}

void FrameCapture::showCaptureRegionPreview(const Mat& screen)
{
    if (screen.empty())
    {
        return;
    }

    double scale = 1.0;
    cv::Mat preview = fitPreview(screen, 1400, 900, scale);

    // Draw the two rectangles in scaled preview coordinates.
    cv::Rect gameRect(
        static_cast<int>(x * scale),
        static_cast<int>(y * scale),
        static_cast<int>(width * scale),
        static_cast<int>(height * scale));
    cv::Rect stateRect(
        static_cast<int>(stateX * scale),
        static_cast<int>(stateY * scale),
        static_cast<int>(stateWidth * scale),
        static_cast<int>(stateHeight * scale));

    cv::rectangle(preview, gameRect, cv::Scalar(0, 128, 255), 3);
    cv::rectangle(preview, stateRect, cv::Scalar(255, 0, 255), 3);
    cv::putText(preview, "Game capture", cv::Point(gameRect.x + 8, std::max(gameRect.y + 28, 28)),
        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 128, 255), 2);
    cv::putText(preview, "State capture", cv::Point(stateRect.x + 8, std::max(stateRect.y + 28, 28)),
        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 0, 255), 2);
    cv::putText(preview, "s=start  e=edit coords  r=refresh  q=quit",
        cv::Point(20, preview.rows - 24),
        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);

    cv::imshow("Capture Region Setup", preview);
}

void FrameCapture::setUpCaptureFrame()
{
    // Default state region sits above the game capture where the lives UI is expected.
    stateWidth = width / 4;
    stateHeight = height / 9;
    stateX = x + 100;
    stateY = y - 50;

    configureCaptureRegions();
}

void FrameCapture::configureCaptureRegions()
{
    cout << "Wayland capture setup uses the desktop portal and PipeWire." << std::endl;
    cout << "Default game region: x=" << x << ", y=" << y << ", width=" << width << ", height=" << height << std::endl;
    cout << "Default state region: x=" << stateX << ", y=" << stateY << ", width=" << stateWidth << ", height=" << stateHeight << std::endl;
    cout << "Use the preview window keys: s=start, e=edit coordinates, r=refresh, q=quit." << std::endl;

    bool setupComplete = false;
    cv::namedWindow("Capture Region Setup", cv::WINDOW_NORMAL);
    while (!setupComplete)
    {
        // Refresh the preview until the user starts, edits, refreshes, or quits.
        Mat screen;
        if (captureScreen(screen))
        {
            showCaptureRegionPreview(screen);
            cv::waitKey(1);
        }
        else
        {
            std::cerr << "Could not read a setup frame from PipeWire yet." << std::endl;
        }

        int key = cv::waitKey(30);
        if (key < 0)
        {
            continue;
        }
        char action = static_cast<char>(std::tolower(key & 0xff));

        if (action == 'e')
        {
            // Console editing is easier after closing the HighGUI setup window.
            try
            {
                cv::destroyWindow("Capture Region Setup");
            }
            catch (const cv::Exception&)
            {
            }

            x = promptInt("Game x", x);
            y = promptInt("Game y", y);
            width = promptInt("Game width", width);
            height = promptInt("Game height", height);
            stateX = promptInt("State x", stateX);
            stateY = promptInt("State y", stateY);
            stateWidth = promptInt("State width", stateWidth);
            stateHeight = promptInt("State height", stateHeight);
            cv::namedWindow("Capture Region Setup", cv::WINDOW_NORMAL);
        }
        else if (action == 'r')
        {
            continue;
        }
        else if (action == 'q')
        {
            std::exit(0);
        }
        else
        {
            setupComplete = true;
        }
    }

    try
    {
        cv::destroyWindow("Capture Region Setup");
    }
    catch (const cv::Exception&)
    {
        // Some HighGUI backends throw if the named window was never created.
    }
    cout << "Starting capture loop. Raw captures will display in the 'Raw Captures' window." << std::endl;
}
