#include <QTime>
#include <QtConcurrent>
#include <QDebug>

// IMPORTANTE: Necesario para cv::HOGDescriptor y cv::detectMultiScale
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include "utilities.h"
#include "capture_thread.h" // Asumo que este archivo define la clase CaptureThread

// Asumo:
// - cv::HOGDescriptor hog;
// - bool motion_detected = false;
// - QTime last_human_detection_time;
// - const int GRABACION_COOLDOWN_MS = 5000; // 5 segundos

CaptureThread::CaptureThread(int camera, QMutex *lock) :
    running(false), cameraID(camera), videoPath(""), data_lock(lock), motion_detected(false)
{
    fps_calculating = false;
    fps = 0.0;

    frame_width = frame_height = 0;
    video_saving_status = STOPPED;
    saved_video_name = "";
    video_writer = nullptr;

    motion_detecting_status = false;

    // Inicialización del Cooldown (Para evitar falsos inicios al arrancar)
    last_human_detection_time.start();

    // 1. Inicialización del Detector HOG/SVM para personas
    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
}

CaptureThread::CaptureThread(QString videoPath, QMutex *lock) :
    running(false), cameraID(-1), videoPath(videoPath), data_lock(lock), motion_detected(false)
{
    fps_calculating = false;
    fps = 0.0;

    frame_width = frame_height = 0;
    video_saving_status = STOPPED;
    saved_video_name = "";
    video_writer = nullptr;

    motion_detecting_status = false;

    // Inicialización del Cooldown (Para evitar falsos inicios al arrancar)
    last_human_detection_time.start();

    // 1. Inicialización del Detector HOG/SVM para personas
    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
}

// Main loop for capturing and processing video frames
void CaptureThread::run()
{
    running = true;

    cv::VideoCapture cap;

    QString current=Utilities::getParam("current");
    QByteArray source="";
    if(Utilities::getParam(current+".tipo")==QString("webcam")){
        source.append("/dev/video");
        source.append(Utilities::getParam(current+".num"));
        cv::VideoCapture cap0(source.constData(), cv::CAP_V4L2);
        cap=cap0;
        qDebug()<<"Capturando desde WebCam: "<<source;
    }else{
        QByteArray source="";
        source.append(Utilities::getParam("cam1.urlmin"));
        qDebug()<<"Capturando desde: "<<source;
        cv::VideoCapture cap0(source.constData());
        cap=cap0;
        qDebug()<<"Capturando desde DVR: "<<source;
    }

    cv::Mat tmp_frame;

    // Update video frame dimensions
    frame_width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    frame_height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);

    // Verificar si la cámara se abrió correctamente
    if (!cap.isOpened()) {
        qDebug() << "ERROR: No se pudo abrir la fuente de video.";
        running = false;
        return;
    }

    while (running)
    {
        cap >> tmp_frame;
        if (tmp_frame.empty())
        {
            break;
        }

        // Llama a la función de detección de humanos
        if (motion_detecting_status)
        {
            humanDetect(tmp_frame);
        }

        // El bucle principal maneja la transición de estados de grabación
        if (video_saving_status == STARTING)
        {
            startSavingVideo(tmp_frame);
        }
        if (video_saving_status == STARTED)
        {
            video_writer->write(tmp_frame);
        }
        if (video_saving_status == STOPPING)
        {
            stopSavingVideo();
        }

        // Convert frame color from BGR to RGB
        cvtColor(tmp_frame, tmp_frame, cv::COLOR_BGR2RGB);

        // Thead-safe update
        data_lock->lock();
        frame = tmp_frame;
        data_lock->unlock();

        // Emit a signal indicating a new frame has been captured
        emit frameCaptured(&frame);
        if (fps_calculating)
        {
            calculateFPS(cap);
        }
    }

    // Cleanup
    cap.release();
    running = false;
}

/*
 * Calculate the FPS by reading 100 frames from the video capture device, then
 * divide the number of frames by the elapsed time in seconds.
 */
void CaptureThread::calculateFPS(cv::VideoCapture &cap)
{
    const int count_to_read = 100;
    cv::Mat tmp_frame;
    QTime timer;
    timer.start();
    for (int i = 0; i < count_to_read; i++)
    {
        cap >> tmp_frame;
    }
    int elapsed_ms = timer.elapsed();
    fps = count_to_read / (elapsed_ms / 1000.0);
    fps_calculating = false;

    // Emit a signal to inform about the updated FPS
    emit fpsChanged(fps);
}

//void CaptureThread::startSavingVideo(cv::Mat &firstFrame)
//{
//    saved_video_name = Utilities::newSavedVideoName();

//    // Generar la imagen de cover
//    QString cover = Utilities::getSavedVideoPath(saved_video_name, "jpg");
//    cv::imwrite(cover.toStdString(), firstFrame);

//    video_writer = new cv::VideoWriter(
//        Utilities::getSavedVideoPath(saved_video_name, "avi").toStdString(),
//        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
//        fps ? fps : 30,
//        cv::Size(frame_width, frame_height));

//    // Cambiar a STARTED para comenzar a escribir frames en el bucle run()
//    video_saving_status = STARTED;
//}
void CaptureThread::startSavingVideo(cv::Mat &firstFrame)
{
    saved_video_name = Utilities::newSavedVideoName();

    // Generar la imagen de cover
    QString cover = Utilities::getSavedVideoPath(saved_video_name, "jpg");
    cv::imwrite(cover.toStdString(), firstFrame);

    // -----------------------------------------------------------
    // ✨ CAMBIO A MP4:
    // 1. Cambiar la extensión del archivo a "mp4".
    // 2. Usar un códec compatible con MP4: 'MP4V' (MPEG-4) o 'X264' (H.264).
    // NOTA: Para 'X264', podrías necesitar librerías FFmpeg configuradas en OpenCV.
    // 'MP4V' suele funcionar con una configuración base de OpenCV.

    // Opción recomendada si tienes FFmpeg configurado:
    // int fourcc = cv::VideoWriter::fourcc('X', '2', '6', '4');
    // Opción alternativa (MPEG-4):
    //int fourcc = cv::VideoWriter::fourcc('M', 'P', '4', 'V');
    int fourcc = cv::VideoWriter::fourcc('X', '2', '6', '4');

    QString video_path_mp4 = Utilities::getSavedVideoPath(saved_video_name, "mp4");

    video_writer = new cv::VideoWriter(
        video_path_mp4.toStdString(), // Usar la extensión "mp4"
        fourcc,                       // Usar códec MP4V (o X264)
        fps ? fps : 30,
        cv::Size(frame_width, frame_height));
    // -----------------------------------------------------------

    // Cambiar a STARTED para comenzar a escribir frames en el bucle run()
    video_saving_status = STARTED;
}

void CaptureThread::stopSavingVideo()
{
    video_saving_status = STOPPED;
    if (video_writer) {
        video_writer->release();
        delete video_writer;
        video_writer = nullptr;
        qDebug()<<"saved_video_name: "<<saved_video_name;
        emit videoSaved(saved_video_name);
    }
}


// **FUNCIÓN HUMAN DETECT CORREGIDA (Versión Final: Control de Estados Mejorado)**
// Detecta figuras humanas utilizando HOG + SVM y controla la grabación.
void CaptureThread::humanDetect(cv::Mat &frame)
{
    std::vector<cv::Rect> found;

    // 2. Aplicar el detector HOG/SVM.
    hog.detectMultiScale(
        frame,
        found,
        0,
        cv::Size(8, 8),
        cv::Size(32, 32),
        1.05,
        2
    );

    // 3. Filtrado de rectángulos solapados
    std::vector<cv::Rect> found_filtered;
    for (size_t i = 0; i < found.size(); i++) {
        cv::Rect r = found[i];
        size_t j;
        for (j = 0; j < found.size(); j++)
            if (j != i && (r & found[j]) == r)
                break;

        if (j == found.size())
            found_filtered.push_back(r);
    }

    // Determinar si hay figuras humanas después del filtrado
    bool human_present = !found_filtered.empty();


    // 4. Control de la lógica de grabación de video (basada en presencia humana y Cooldown)
    if (human_present)
    {
        // Caso 1: Humano presente.
        // 1. Reiniciamos el contador de tiempo de la última detección para extender el cooldown.
        last_human_detection_time.restart();

        // 2. Si la grabación está detenida o deteniéndose, la iniciamos (estado STARTING).
        if (video_saving_status == STOPPED || video_saving_status == STOPPING)
        {
            // Cambiamos el estado para que run() inicie la grabación en el siguiente bucle.
            setVideoSavingStatus(STARTING);
            qDebug() << "Figura humana detectada. Iniciando grabación.";
        }

        // Establecer el flag de detección.
        motion_detected = true;
    }
    else // !human_present
    {
        // Caso 2: Humano NO presente (en este frame).

        // Solo detener si la grabación está activa (STARTING o STARTED) Y ha expirado el tiempo de cooldown.
        if ((video_saving_status == STARTED || video_saving_status == STARTING) &&
            (last_human_detection_time.elapsed() > GRABACION_COOLDOWN_MS))
        {
            // Ha pasado el tiempo de gracia (cooldown). Detener la grabación.
            setVideoSavingStatus(STOPPING);
            qDebug() << "Fin del Cooldown. Deteniendo grabación.";
        }

        // Establecer el flag de detección.
        motion_detected = false;
    }

    // 5. Dibujar los rectángulos de detección en el frame
    cv::Scalar color = cv::Scalar(0, 255, 0); // Rectángulos verdes
    for (size_t i = 0; i < found_filtered.size(); i++)
    {
        cv::Rect r = found_filtered[i];

        // Ajuste de los límites del rectángulo
        r.x += cvRound(r.width * 0.1);
        r.width = cvRound(r.width * 0.8);
        r.y += cvRound(r.height * 0.07);
        r.height = cvRound(r.height * 0.8);

        cv::rectangle(frame, r.tl(), r.br(), color, 3);
        cv::putText(frame, "HUMANO", cv::Point(r.x, r.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
    }
}


// Setters for thread controls and video capture configurations
void CaptureThread::setRunning(bool run)
{
    running = run;
}

void CaptureThread::startCalcFPS()
{
    fps_calculating = true;
}

void CaptureThread::setVideoSavingStatus(VideoSavingStatus status)
{
    video_saving_status = status;
}

void CaptureThread::setMotionDetectingStatus(bool status)
{
    motion_detecting_status = status;
}
//#include <QTime>
//#include <QtConcurrent>
//#include <QDebug>

//// IMPORTANTE: Necesario para cv::HOGDescriptor y cv::detectMultiScale
//#include <opencv2/objdetect.hpp>
//#include <opencv2/imgproc.hpp>
//#include <opencv2/highgui.hpp>

//#include "utilities.h"
//#include "capture_thread.h" // Asumo que este archivo define la clase CaptureThread

//// Asumo que HOGDescriptor es un miembro de la clase (cv::HOGDescriptor hog;)
//// Asumo que motion_detected es un miembro de la clase (bool motion_detected = false;)


//CaptureThread::CaptureThread(int camera, QMutex *lock) :
//    running(false), cameraID(camera), videoPath(""), data_lock(lock), motion_detected(false)
//{
//    fps_calculating = false;
//    fps = 0.0;

//    frame_width = frame_height = 0;
//    video_saving_status = STOPPED;
//    saved_video_name = "";
//    video_writer = nullptr;

//    motion_detecting_status = false;

//    // Inicialización del Cooldown
//    last_human_detection_time.start();

//    // 1. Inicialización del Detector HOG/SVM para personas
//    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
//}

//CaptureThread::CaptureThread(QString videoPath, QMutex *lock) :
//    running(false), cameraID(-1), videoPath(videoPath), data_lock(lock), motion_detected(false)
//{
//    fps_calculating = false;
//    fps = 0.0;

//    frame_width = frame_height = 0;
//    video_saving_status = STOPPED;
//    saved_video_name = "";
//    video_writer = nullptr;

//    motion_detecting_status = false;

//    // Inicialización del Cooldown
//    last_human_detection_time.start();

//    // 1. Inicialización del Detector HOG/SVM para personas
//    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
//}

//// Main loop for capturing and processing video frames
//void CaptureThread::run()
//{
//    running = true;

//    cv::VideoCapture cap;

//    QString current=Utilities::getParam("current");
//    QByteArray source="";
//    if(Utilities::getParam(current+".tipo")==QString("webcam")){
//        source.append("/dev/video");
//        source.append(Utilities::getParam(current+".num"));
//        cv::VideoCapture cap0(source.constData(), cv::CAP_V4L2);
//        cap=cap0;
//        qDebug()<<"Capturando desde WebCam: "<<source;
//    }else{
//        QByteArray source="";
//        source.append(Utilities::getParam("cam1.urlmin"));
//        qDebug()<<"Capturando desde: "<<source;
//        cv::VideoCapture cap0(source.constData());
//        cap=cap0;
//        qDebug()<<"Capturando desde DVR: "<<source;
//    }

//    cv::Mat tmp_frame;

//    // Update video frame dimensions
//    frame_width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
//    frame_height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);

//    // Verificar si la cámara se abrió correctamente
//    if (!cap.isOpened()) {
//        qDebug() << "ERROR: No se pudo abrir la fuente de video.";
//        running = false;
//        return;
//    }

//    while (running)
//    {
//        cap >> tmp_frame;
//        if (tmp_frame.empty())
//        {
//            break;
//        }

//        // Llama a la función de detección de humanos
//        if (motion_detecting_status)
//        {
//            humanDetect(tmp_frame);
//        }

//        if (video_saving_status == STARTING)
//        {
//            startSavingVideo(tmp_frame);
//        }
//        if (video_saving_status == STARTED)
//        {
//            video_writer->write(tmp_frame);
//        }
//        if (video_saving_status == STOPPING)
//        {
//            stopSavingVideo();
//        }

//        // Convert frame color from BGR to RGB
//        cvtColor(tmp_frame, tmp_frame, cv::COLOR_BGR2RGB);

//        // Thead-safe update
//        data_lock->lock();
//        frame = tmp_frame;
//        data_lock->unlock();

//        // Emit a signal indicating a new frame has been captured
//        emit frameCaptured(&frame);
//        if (fps_calculating)
//        {
//            calculateFPS(cap);
//        }
//    }

//    // Cleanup
//    cap.release();
//    running = false;
//}

///*
// * Calculate the FPS by reading 100 frames from the video capture device, then
// * divide the number of frames by the elapsed time in seconds.
// */
//void CaptureThread::calculateFPS(cv::VideoCapture &cap)
//{
//    const int count_to_read = 100;
//    cv::Mat tmp_frame;
//    QTime timer;
//    timer.start();
//    for (int i = 0; i < count_to_read; i++)
//    {
//        cap >> tmp_frame;
//    }
//    int elapsed_ms = timer.elapsed();
//    fps = count_to_read / (elapsed_ms / 1000.0);
//    fps_calculating = false;

//    // Emit a signal to inform about the updated FPS
//    emit fpsChanged(fps);
//}

//void CaptureThread::startSavingVideo(cv::Mat &firstFrame)
//{
//    saved_video_name = Utilities::newSavedVideoName();

//    QString cover = Utilities::getSavedVideoPath(saved_video_name, "jpg");
//    cv::imwrite(cover.toStdString(), firstFrame);

//    video_writer = new cv::VideoWriter(
//        Utilities::getSavedVideoPath(saved_video_name, "avi").toStdString(),
//        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
//        fps ? fps : 30,
//        cv::Size(frame_width, frame_height));
//    video_saving_status = STARTED;
//}

//void CaptureThread::stopSavingVideo()
//{
//    video_saving_status = STOPPED;
//    video_writer->release();
//    delete video_writer;
//    video_writer = nullptr;
//    qDebug()<<"saved_video_name: "<<saved_video_name;
//    emit videoSaved(saved_video_name);
//}


//// **FUNCIÓN HUMAN DETECT CORREGIDA (Implementando el Cooldown)**
//// Detecta figuras humanas utilizando HOG + SVM y controla la grabación.
//void CaptureThread::humanDetect(cv::Mat &frame)
//{
//    std::vector<cv::Rect> found;

//    // 2. Aplicar el detector HOG/SVM.
//    hog.detectMultiScale(
//        frame,          // Imagen de entrada (BGR)
//        found,          // Vector de rectángulos donde se detectó la persona
//        0,              // Umbral de acierto (hit threshold)
//        cv::Size(8, 8), // Paso de la ventana (win stride)
//        cv::Size(32, 32), // Relleno (padding)
//        1.05,           // Factor de escala
//        2               // Mínimo de detecciones por grupo
//    );

//    // 3. Filtrado de rectángulos solapados
//    std::vector<cv::Rect> found_filtered;
//    for (size_t i = 0; i < found.size(); i++) {
//        cv::Rect r = found[i];
//        size_t j;
//        for (j = 0; j < found.size(); j++)
//            if (j != i && (r & found[j]) == r)
//                break;

//        if (j == found.size())
//            found_filtered.push_back(r);
//    }

//    // Determinar si hay figuras humanas después del filtrado
//    bool human_present = !found_filtered.empty();

//    // 4. Control de la lógica de grabación de video (basada en presencia humana y Cooldown)

//    if (human_present)
//    {
//        // Caso 1: Humano presente.
//        // Se reinicia el contador de tiempo de la última detección.
//        last_human_detection_time.restart();

//        if (!motion_detected)
//        {
//            // Si la grabación estaba parada, la iniciamos.
//            motion_detected = true;
//            setVideoSavingStatus(STARTING);
//            qDebug() << "Figura humana detectada. Iniciando grabación.";
//        }
//        // Si ya estaba grabando, continúa.
//    }
//    else // !human_present
//    {
//        // Caso 2: Humano NO presente (en este frame).

//        // Solo detener si la grabación está activa Y ha expirado el tiempo de cooldown.
//        if (motion_detected && (last_human_detection_time.elapsed() > GRABACION_COOLDOWN_MS))
//        {
//            // Ha pasado el tiempo de gracia (cooldown). Detener.
//            motion_detected = false;
//            setVideoSavingStatus(STOPPING);
//            qDebug() << "Fin del Cooldown. Deteniendo grabación.";
//        }
//        // Si el cooldown NO ha expirado, la grabación continúa.
//    }


//    // 5. Dibujar los rectángulos de detección en el frame
//    cv::Scalar color = cv::Scalar(0, 255, 0); // Rectángulos verdes
//    for (size_t i = 0; i < found_filtered.size(); i++)
//    {
//        cv::Rect r = found_filtered[i];

//        // Ajuste de los límites del rectángulo
//        r.x += cvRound(r.width * 0.1);
//        r.width = cvRound(r.width * 0.8);
//        r.y += cvRound(r.height * 0.07);
//        r.height = cvRound(r.height * 0.8);

//        cv::rectangle(frame, r.tl(), r.br(), color, 3);
//        cv::putText(frame, "HUMANO", cv::Point(r.x, r.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
//    }
//}


//// Setters for thread controls and video capture configurations
//void CaptureThread::setRunning(bool run)
//{
//    running = run;
//}

//void CaptureThread::startCalcFPS()
//{
//    fps_calculating = true;
//}

//void CaptureThread::setVideoSavingStatus(VideoSavingStatus status)
//{
//    video_saving_status = status;
//}

//void CaptureThread::setMotionDetectingStatus(bool status)
//{
//    motion_detecting_status = status;
//}
////#include <QTime>
////#include <QtConcurrent>
////#include <QDebug>

////// IMPORTANTE: Necesario para cv::HOGDescriptor y cv::detectMultiScale
////#include <opencv2/objdetect.hpp>
////#include <opencv2/imgproc.hpp>
////#include <opencv2/highgui.hpp>

////#include "utilities.h"
////#include "capture_thread.h" // Asumo que este archivo define la clase CaptureThread

////// Asumo que HOGDescriptor es un miembro de la clase (cv::HOGDescriptor hog;)
////// Asumo que motion_detected es un miembro de la clase (bool motion_detected = false;)
////// Asumo que ya no necesitas segmentor (MOG2), por lo que se elimina su uso.

////CaptureThread::CaptureThread(int camera, QMutex *lock) :
////    running(false), cameraID(camera), videoPath(""), data_lock(lock), motion_detected(false)
////{
////    fps_calculating = false;
////    fps = 0.0;

////    frame_width = frame_height = 0;
////    video_saving_status = STOPPED;
////    saved_video_name = "";
////    video_writer = nullptr;

////    motion_detecting_status = false;

////    // 1. Inicialización del Detector HOG/SVM para personas
////    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
////}

////CaptureThread::CaptureThread(QString videoPath, QMutex *lock) :
////    running(false), cameraID(-1), videoPath(videoPath), data_lock(lock), motion_detected(false)
////{
////    fps_calculating = false;
////    fps = 0.0;

////    frame_width = frame_height = 0;
////    video_saving_status = STOPPED;
////    saved_video_name = "";
////    video_writer = nullptr;

////    motion_detecting_status = false;

////    // 1. Inicialización del Detector HOG/SVM para personas
////    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
////}

////// Main loop for capturing and processing video frames
////void CaptureThread::run()
////{
////    running = true;

////    cv::VideoCapture cap;

////    QString current=Utilities::getParam("current");
////    QByteArray source="";
////    if(Utilities::getParam(current+".tipo")==QString("webcam")){
////        source.append("/dev/video");
////        source.append(Utilities::getParam(current+".num"));
////        cv::VideoCapture cap0(source.constData(), cv::CAP_V4L2);
////        cap=cap0;
////        qDebug()<<"Capturando desde WebCam: "<<source;
////    }else{
////        QByteArray source="";
////        source.append(Utilities::getParam("cam1.urlmin"));
////        qDebug()<<"Capturando desde: "<<source;
////        cv::VideoCapture cap0(source.constData());
////        cap=cap0;
////        qDebug()<<"Capturando desde DVR: "<<source;
////    }


////    //cv::VideoCapture cap(Utilities::getParam("cam1.url").toUtf8());


////    // NOTA: Se ha eliminado la inicialización del segmentor MOG2.
////    // segmentor = cv::createBackgroundSubtractorMOG2(500, 16, true);

////    cv::Mat tmp_frame;

////    // Update video frame dimensions
////    frame_width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
////    frame_height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);

////    // Verificar si la cámara se abrió correctamente
////    if (!cap.isOpened()) {
////        qDebug() << "ERROR: No se pudo abrir la fuente de video.";
////        running = false;
////        return;
////    }

////    while (running)
////    {
////        cap >> tmp_frame;
////        if (tmp_frame.empty())
////        {
////            break;
////        }

////        // **ACTUALIZACIÓN**: Llama a la nueva función de detección de humanos
////        if (motion_detecting_status)
////        {
////            // humanDetect ahora se encarga de verificar la presencia humana,
////            // dibujar los rectángulos y actualizar 'motion_detected' (el estado de grabación).
////            humanDetect(tmp_frame);
////        }

////        if (video_saving_status == STARTING)
////        {
////            startSavingVideo(tmp_frame);
////        }
////        if (video_saving_status == STARTED)
////        {
////            video_writer->write(tmp_frame);
////        }
////        if (video_saving_status == STOPPING)
////        {
////            stopSavingVideo();
////        }

////        // Convert frame color from BGR to RGB
////        cvtColor(tmp_frame, tmp_frame, cv::COLOR_BGR2RGB);

////        // Thead-safe update
////        data_lock->lock();
////        frame = tmp_frame;
////        data_lock->unlock();

////        // Emit a signal indicating a new frame has been captured
////        emit frameCaptured(&frame);
////        if (fps_calculating)
////        {
////            calculateFPS(cap);
////        }
////    }

////    // Cleanup
////    cap.release();
////    running = false;
////}

////// [ ... Las funciones calculateFPS, startSavingVideo, stopSavingVideo se mantienen igual ... ]

/////*
//// * Calculate the FPS by reading 100 frames from the video capture device, then
//// * divide the number of frames by the elapsed time in seconds.
//// */
////void CaptureThread::calculateFPS(cv::VideoCapture &cap)
////{
////    const int count_to_read = 100;
////    cv::Mat tmp_frame;
////    QTime timer;
////    timer.start();
////    for (int i = 0; i < count_to_read; i++)
////    {
////        cap >> tmp_frame;
////    }
////    int elapsed_ms = timer.elapsed();
////    fps = count_to_read / (elapsed_ms / 1000.0);
////    fps_calculating = false;

////    // Emit a signal to inform about the updated FPS
////    emit fpsChanged(fps);
////}

////void CaptureThread::startSavingVideo(cv::Mat &firstFrame)
////{
////    saved_video_name = Utilities::newSavedVideoName();

////    QString cover = Utilities::getSavedVideoPath(saved_video_name, "jpg");
////    cv::imwrite(cover.toStdString(), firstFrame);

////    video_writer = new cv::VideoWriter(
////        Utilities::getSavedVideoPath(saved_video_name, "avi").toStdString(),
////        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
////        fps ? fps : 30,
////        cv::Size(frame_width, frame_height));
////    video_saving_status = STARTED;
////}

////void CaptureThread::stopSavingVideo()
////{
////    video_saving_status = STOPPED;
////    video_writer->release();
////    delete video_writer;
////    video_writer = nullptr;
////    qDebug()<<"saved_video_name: "<<saved_video_name;
////    emit videoSaved(saved_video_name);
////}


////// **FUNCIÓN HUMAN DETECT (Reemplazo de motionDetect)**
////// Detecta figuras humanas utilizando HOG + SVM.
////void CaptureThread::humanDetect(cv::Mat &frame)
////{
////    std::vector<cv::Rect> found;

////    // 2. Aplicar el detector HOG/SVM. Los parámetros son clave para el rendimiento.
////    // Se buscan peatones/cuerpos completos.
////    hog.detectMultiScale(
////        frame,          // Imagen de entrada (BGR)
////        found,          // Vector de rectángulos donde se detectó la persona
////        //Por defecto el Umbral era 0.
////        0,              // Umbral de acierto (hit threshold)
////        cv::Size(8, 8), // Paso de la ventana (win stride)
////        cv::Size(32, 32), // Relleno (padding)
////        1.05,           // Factor de escala (1.05 es el valor recomendado)
////        2               // Mínimo de detecciones por grupo para considerarse una detección válida
////    );

////    // 3. Filtrado de rectángulos solapados (Non-Maximum Suppression simplificado)
////    std::vector<cv::Rect> found_filtered;
////    for (size_t i = 0; i < found.size(); i++) {
////        cv::Rect r = found[i];
////        size_t j;
////        // Comprobar si este rectángulo está contenido completamente en otro más grande
////        for (j = 0; j < found.size(); j++)
////            if (j != i && (r & found[j]) == r)
////                break;

////        if (j == found.size())
////            found_filtered.push_back(r);
////    }

////    // Determinar si hay figuras humanas después del filtrado
////    bool human_present = !found_filtered.empty();

////    // 4. Control de la lógica de grabación de video (basada en presencia humana)
////    if (!motion_detected && human_present)
////    {
////        // El estado 'motion_detected' pasa a ser true cuando se detecta un humano
////        motion_detected = true;
////        setVideoSavingStatus(STARTING);
////        qDebug() << "Figura humana detectada. Iniciando grabación.";
////    }
////    else if (motion_detected && !human_present)
////    {
////        // El estado 'motion_detected' pasa a ser false cuando el humano desaparece
////        motion_detected = false;
////        setVideoSavingStatus(STOPPING);
////        qDebug() << "Figura humana desapareció. Deteniendo grabación.";
////    }

////    // 5. Dibujar los rectángulos de detección en el frame
////    cv::Scalar color = cv::Scalar(0, 255, 0); // Rectángulos verdes
////    for (size_t i = 0; i < found_filtered.size(); i++)
////    {
////        cv::Rect r = found_filtered[i];

////        // Ajuste de los límites del rectángulo para evitar bordes ruidosos
////        r.x += cvRound(r.width * 0.1);
////        r.width = cvRound(r.width * 0.8);
////        r.y += cvRound(r.height * 0.07);
////        r.height = cvRound(r.height * 0.8);

////        cv::rectangle(frame, r.tl(), r.br(), color, 3);
////        cv::putText(frame, "HUMANO", cv::Point(r.x, r.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
////    }
////}


////// Setters for thread controls and video capture configurations
////void CaptureThread::setRunning(bool run)
////{
////    running = run;
////}

////void CaptureThread::startCalcFPS()
////{
////    fps_calculating = true;
////}

////void CaptureThread::setVideoSavingStatus(VideoSavingStatus status)
////{
////    video_saving_status = status;
////}

////void CaptureThread::setMotionDetectingStatus(bool status)
////{
////    motion_detecting_status = status;
////}
