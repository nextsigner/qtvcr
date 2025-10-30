#include "json_parser.h"

// Implementación de la función getParam
QString JsonParser::getParam(const QString &jsonString, const QString &paramPath)
{
    // 1. Convertir QString a QJsonDocument
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());

    if (doc.isNull()) {
        qWarning() << "Error: La cadena no es un documento JSON válido.";
        return QString();
    }
    
    // Asegurarse de que el documento es un objeto (lo más común para parámetros raíz)
    if (!doc.isObject()) {
        qWarning() << "Error: El documento JSON no es un objeto raíz.";
        return QString();
    }

    QJsonObject currentObject = doc.object();
    QStringList pathSegments = paramPath.split('.');
    
    // Iterar sobre los segmentos del path
    for (int i = 0; i < pathSegments.size(); ++i) {
        const QString &key = pathSegments.at(i);
        QJsonValue value = currentObject.value(key);

        // Si es el último segmento del camino, hemos encontrado el valor
        if (i == pathSegments.size() - 1) {
            if (value.isUndefined()) {
                qWarning() << "Advertencia: El parámetro '" << paramPath << "' no se encontró (clave faltante:" << key << ").";
                return QString();
            }
            
            // Convertir QJsonValue a QString de la manera más adecuada
            if (value.isString()) {
                return value.toString();
            } else if (value.isDouble()) {
                // Para números (double), devolver como string. Usamos 15 decimales de precisión.
                return QString::number(value.toDouble(), 'g', 15);
            } else if (value.isBool()) {
                return value.toBool() ? "true" : "false";
            } else if (value.isNull()) {
                return "null";
            } else {
                qWarning() << "Advertencia: El valor del parámetro no es un tipo primitivo simple (string, number, bool).";
                return QString();
            }
        } 
        // Si no es el último segmento, debe ser un objeto para continuar la búsqueda
        else {
            if (value.isObject()) {
                currentObject = value.toObject();
            } else {
                qWarning() << "Advertencia: La clave intermedia '" << key << "' no es un objeto. No se puede continuar la ruta.";
                return QString();
            }
        }
    }

    return QString(); // Debería ser inaccesible si la lógica es correcta, pero se añade por seguridad.
}
int JsonParser::getParamCount(const QString &jsonString)
{
    // 1. Convertir QString a QJsonDocument
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());

    if (doc.isNull()) {
        qWarning() << "Error: La cadena no es un documento JSON válido.";
        return 0;
    }

    // Asegurarse de que el documento es un objeto (lo más común para parámetros raíz)
    if (!doc.isObject()) {
        qWarning() << "Error: El documento JSON no es un objeto raíz.";
        return 0;
    }

    //QJsonObject currentObject = doc.object();
    return doc.object().count();
}
