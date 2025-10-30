#pragma once

#include <QString>
#include <QTime>
#include <QThread>
#include <QMutex>
#include "opencv2/opencv.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/objdetect.hpp" // Se añade para cv::HOGDescriptor

using namespace std;



class CaptureThread : public QThread
{
    Q_OBJECT

public:
    // Constructors for capturing from a camera or from a video file
    CaptureThread(int camera, QMutex *lock);
    CaptureThread(QString videoPath, QMutex *lock);

    ~CaptureThread() = default;

    // Setters for thread controls and video capture configurations
    void setRunning(bool run);
    void startCalcFPS();

    // Enumeration to handle video saving status
    enum VideoSavingStatus
    {
        STARTING,
        STARTED,
        STOPPING,
        STOPPED
    };

    void setVideoSavingStatus(VideoSavingStatus status);
    void setMotionDetectingStatus(bool status);

protected:
    void run() override; // Main loop for capturing and processing video frames

signals:
    // Signals to notify other Qt components about frame capture, FPS changes, and video saving status
    void frameCaptured(cv::Mat *data);
    void fpsChanged(float fps);
    void videoSaved(QString name);

private:
    // **Añadir esto a la definición de la clase CaptureThread (ej. en capture_thread.h)**
    private:
        // ... otros miembros
        QTime last_human_detection_time;
        const int GRABACION_COOLDOWN_MS = 5000; // 5 segundos de gracia/cooldown
        // ...
    // Internal helper functions for FPS calculation, video saving, and human detection
    void calculateFPS(cv::VideoCapture &cap);
    void startSavingVideo(cv::Mat &firstFrame);
    void stopSavingVideo();
    void humanDetect(cv::Mat &frame); // Reemplaza motionDetect para la detección de humanos

    bool running;
    int cameraID;
    QString videoPath;
    QMutex *data_lock; // Mutex for thread-safe data access
    cv::Mat frame;

    // FPS variables
    bool fps_calculating;
    float fps;

    // Video saving variables
    int frame_width, frame_height;
    VideoSavingStatus video_saving_status;
    QString saved_video_name;
    cv::VideoWriter *video_writer;

    // Human Detection variables
    bool motion_detecting_status;
    bool motion_detected;
    cv::HOGDescriptor hog; // Detector HOG para figuras humanas
};

