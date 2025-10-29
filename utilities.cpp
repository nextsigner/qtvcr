#include <QFile>
#include <QObject>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostInfo>
#include <QDebug>

#include "utilities.h"
#include "json_parser.h"

QString Utilities::getDataPath()
{
    //QString user_movie_path = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation)[0];
    QString user_movie_path = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0];
    QString folder=fileToString(qApp->applicationDirPath()+"/folder").replace("\n", "");
    if(folder!=QString("")){
        qDebug()<<"Carpeta de videos y fotos: "<<folder;
        user_movie_path=folder;
    }else{
        qDebug()<<"No hay carpeta de videos y fotos definida. Todo se guardará en: "<<user_movie_path;

    }

    //QString user_movie_path ="/media/ns/686F8CD568B41F5D/recs";

    QDir movie_dir(user_movie_path);
    movie_dir.mkpath("qtvcr");
    return movie_dir.absoluteFilePath("qtvcr");
}

QString Utilities::newSavedVideoName()
{
    QDateTime time = QDateTime::currentDateTime();
    return time.toString("yyyy-MM-dd+HH:mm:ss");
}

QString Utilities::getSavedVideoPath(QString name, QString postfix)
{
    return QString("%1/%2.%3").arg(Utilities::getDataPath(), name, postfix);
}


QString Utilities::fileToString(const QString &rutaArchivo)
{
    QFile archivo(rutaArchivo);
    QString contenido = "";

    if (archivo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Opción 1 (Recomendada y más rápida): Leer todo el contenido como QByteArray
        QByteArray datos = archivo.readAll();
        // Convertir QByteArray a QString, asumiendo codificación UTF-8
        contenido = QString::fromUtf8(datos);

        // Opción 2 (Alternativa para archivos de texto puros): Usar QTextStream
        /*
        QTextStream flujo(&archivo);
        contenido = flujo.readAll();
        */

        archivo.close();
    } else {
        qWarning() << "No se pudo abrir el archivo para lectura:" << rutaArchivo;
    }

    return contenido;
}

// Ejemplo de uso:
void Utilities::ejemploUso() {
    // Reemplaza "ruta/a/tu/archivo.txt" con la ruta real
    QString ruta = "datos.txt";
    QString datosLeidos = fileToString(ruta);

    qDebug() << "Contenido del archivo:\n" << datosLeidos;
}

QString Utilities::getParam(const QString param)
{
    JsonParser parser;

    // La cadena JSON que has proporcionado
    QString jsonString = fileToString("config.cfg").replace("\n", "");//"{\"params\":{\"dato1\":\"aaa\", \"dato2\": 222, \"activo\": true, \"vacio\": null, \"profundo\": {\"config\": 1.23}}}";

    qDebug() << "--- Analizador de Parámetros JSON ---";
    qDebug() << "JSON de Entrada: " << jsonString;
    return parser.getParam(jsonString, param);
}
