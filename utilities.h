#pragma once

#include <QString>

class Utilities
{
 public:
    static QString getDataPath();
    static QString newSavedVideoName();
    static QString getSavedVideoPath(QString name, QString postfix);
    static QString fileToString(const QString &rutaArchivo);
    static void ejemploUso();
    static QString getParam(const QString param);
    static int getParamCount();
};
