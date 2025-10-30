#!/bin/bash

# Este script encuentra y elimina archivos .mp4 que tengan un tamaño menor al especificado.
# La búsqueda se realiza recursivamente a partir del directorio actual.

# --- Configuración ---
# Define el tamaño límite en kilobytes (ej: 100k para 100 Kilobytes).
# 'k' es para kilobytes. 'M' para megabytes, 'G' para gigabytes, etc.
MIN_SIZE="$3"

# Define la extensión de archivo
FILE_EXTENSION="$2"

# --- Ejecución ---

echo "🔎 Buscando archivos *.$FILE_EXTENSION con tamaño menor a $MIN_SIZE..."
echo "Los archivos encontrados serán ELIMINADOS permanentemente."
echo "---------------------------------------------------------"

# El comando 'find' busca archivos:
# 1. En el directorio actual (.)
# 2. De tipo archivo (-type f)
# 3. Con el nombre que termina en la extensión especificada (-name "*.$FILE_EXTENSION")
# 4. Cuyo tamaño es menor que el valor de MIN_SIZE (-size -"$MIN_SIZE")
# 5. Y los elimina directamente (-delete)

# Para **VER** los archivos que se eliminarán antes de ejecutar la eliminación:
# find . -type f -name "*.$FILE_EXTENSION" -size -"$MIN_SIZE" -print

# Para **ELIMINAR** los archivos:
find $1 -type f -name "*.$FILE_EXTENSION" -size -"$MIN_SIZE" -delete

# Comprobación de éxito o fracaso
if [ $? -eq 0 ]; then
    echo "✅ La operación de eliminación ha finalizado."
else
    echo "❌ Ocurrió un error durante la ejecución del comando 'find'."
fi
