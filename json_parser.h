#pragma once

#include <QObject>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

class JsonParser : public QObject
{
    Q_OBJECT
public:
    explicit JsonParser(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief Extrae un parámetro de una cadena JSON, soportando anidamiento.
     *
     * Ejemplo de JSON de entrada: "{"params":{"dato1":"aaa", "dato2": 222}}"
     * Llamada: getParam("params.dato1") -> "aaa"
     * Llamada: getParam("params.dato2") -> "222" (como string)
     *
     * @param jsonString La cadena JSON de entrada.
     * @param paramPath El camino del parámetro, separado por puntos (e.g., "root.child.key").
     * @return El valor del parámetro como QString, o un QString vacío si no se encuentra o hay error.
     */
    QString getParam(const QString &jsonString, const QString &paramPath);
};
