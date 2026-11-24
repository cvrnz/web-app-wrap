#pragma once

#include <QByteArray>
#include <QJsonValue>
#include <QString>

// Reads JSON5, which also covers JSONC and plain JSON: comments, trailing commas, unquoted
// keys, single-quoted strings, hexadecimal numbers, Infinity and NaN, and escaped line breaks.
// Batch files on every platform accept all three.
namespace Json5 {
// Returns the parsed value, or an undefined QJsonValue with `error` set to a message naming
// the line and column.
QJsonValue parse(const QByteArray &data, QString *error = nullptr);
}
