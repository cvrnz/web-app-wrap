#include "json5.h"

#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
#include <limits>

namespace {

class Parser {
public:
    explicit Parser(const QString &text) : m_text(text) {}

    QJsonValue run(QString *error) {
        try {
            skipSpace();
            QJsonValue value = parseValue();
            skipSpace();
            if (m_pos < m_text.size())
                fail("Unexpected text after the end of the data");
            return value;
        } catch (const QString &message) {
            if (error)
                *error = message;
            return QJsonValue(QJsonValue::Undefined);
        }
    }

private:
    [[noreturn]] void fail(const QString &message) const {
        int line = 1, column = 1;
        for (qsizetype i = 0; i < m_pos && i < m_text.size(); ++i) {
            if (m_text[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        throw QString("%1 at line %2, column %3.").arg(message).arg(line).arg(column);
    }

    QChar peek(qsizetype offset = 0) const {
        return m_pos + offset < m_text.size() ? m_text[m_pos + offset] : QChar();
    }

    void skipSpace() {
        while (m_pos < m_text.size()) {
            const QChar c = m_text[m_pos];
            if (c.isSpace() || c == QChar(0xFEFF)) {
                ++m_pos;
            } else if (c == '/' && peek(1) == '/') {
                while (m_pos < m_text.size() && m_text[m_pos] != '\n')
                    ++m_pos;
            } else if (c == '/' && peek(1) == '*') {
                const qsizetype end = m_text.indexOf("*/", m_pos + 2);
                if (end < 0)
                    fail("Unclosed comment");
                m_pos = end + 2;
            } else {
                break;
            }
        }
    }

    QJsonValue parseValue() {
        const QChar c = peek();
        if (c == '{')
            return parseObject();
        if (c == '[')
            return parseArray();
        if (c == '"' || c == '\'')
            return parseString();
        if (c.isDigit() || c == '-' || c == '+' || c == '.')
            return parseNumber();
        if (c.isLetter()) {
            const QString word = parseIdentifier();
            if (word == "true")
                return true;
            if (word == "false")
                return false;
            if (word == "null")
                return QJsonValue(QJsonValue::Null);
            if (word == "Infinity")
                return std::numeric_limits<double>::infinity();
            if (word == "NaN")
                return std::numeric_limits<double>::quiet_NaN();
            fail(QString("Unexpected word “%1”").arg(word));
        }
        if (c.isNull())
            fail("Unexpected end of the data");
        fail(QString("Unexpected character “%1”").arg(c));
    }

    QJsonObject parseObject() {
        QJsonObject object;
        ++m_pos; // {
        skipSpace();
        while (peek() != '}') {
            QString key;
            if (peek() == '"' || peek() == '\'')
                key = parseString();
            else if (peek().isLetter() || peek() == '_' || peek() == '$')
                key = parseIdentifier();
            else
                fail("Expected a key");
            skipSpace();
            if (peek() != ':')
                fail("Expected “:”");
            ++m_pos;
            skipSpace();
            object.insert(key, parseValue());
            skipSpace();
            if (peek() == ',') {
                ++m_pos;
                skipSpace();
            } else if (peek() != '}') {
                fail("Expected “,” or “}”");
            }
        }
        ++m_pos; // }
        return object;
    }

    QJsonArray parseArray() {
        QJsonArray array;
        ++m_pos; // [
        skipSpace();
        while (peek() != ']') {
            array.append(parseValue());
            skipSpace();
            if (peek() == ',') {
                ++m_pos;
                skipSpace();
            } else if (peek() != ']') {
                fail("Expected “,” or “]”");
            }
        }
        ++m_pos; // ]
        return array;
    }

    QString parseIdentifier() {
        const qsizetype start = m_pos;
        while (m_pos < m_text.size() && (m_text[m_pos].isLetterOrNumber() || m_text[m_pos] == '_' || m_text[m_pos] == '$'))
            ++m_pos;
        return m_text.mid(start, m_pos - start);
    }

    QString parseString() {
        const QChar quote = m_text[m_pos++];
        QString result;
        while (true) {
            if (m_pos >= m_text.size())
                fail("Unclosed string");
            const QChar c = m_text[m_pos++];
            if (c == quote)
                return result;
            if (c == '\n')
                fail("Line break inside a string");
            if (c != '\\') {
                result += c;
                continue;
            }
            if (m_pos >= m_text.size())
                fail("Unclosed string");
            const QChar e = m_text[m_pos++];
            switch (e.unicode()) {
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            case 'v': result += '\v'; break;
            case '0': result += QChar(0); break;
            case '\n': break; // An escaped line break continues the string.
            case '\r':
                if (peek() == '\n')
                    ++m_pos;
                break;
            case 'x': result += QChar(hexDigits(2)); break;
            case 'u': result += QChar(hexDigits(4)); break;
            default: result += e; break;
            }
        }
    }

    ushort hexDigits(int count) {
        bool ok = false;
        const ushort value = m_text.mid(m_pos, count).toUShort(&ok, 16);
        if (!ok || m_pos + count > m_text.size())
            fail("Bad escape in a string");
        m_pos += count;
        return value;
    }

    double parseNumber() {
        double sign = 1;
        if (peek() == '-' || peek() == '+') {
            sign = peek() == '-' ? -1 : 1;
            ++m_pos;
        }
        if (peek().isLetter()) {
            const QString word = parseIdentifier();
            if (word == "Infinity")
                return sign * std::numeric_limits<double>::infinity();
            if (word == "NaN")
                return std::numeric_limits<double>::quiet_NaN();
            fail("Bad number");
        }
        if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
            m_pos += 2;
            const QString digits = parseIdentifier();
            bool ok = false;
            const qulonglong value = digits.toULongLong(&ok, 16);
            if (!ok)
                fail("Bad hexadecimal number");
            return sign * double(value);
        }
        const qsizetype start = m_pos;
        while (m_pos < m_text.size()) {
            const QChar c = m_text[m_pos];
            const bool exponentSign = (c == '+' || c == '-') && m_pos > start
                                      && (m_text[m_pos - 1] == 'e' || m_text[m_pos - 1] == 'E');
            if (c.isDigit() || c == '.' || c == 'e' || c == 'E' || exponentSign)
                ++m_pos;
            else
                break;
        }
        bool ok = false;
        const double value = m_text.mid(start, m_pos - start).toDouble(&ok);
        if (!ok)
            fail("Bad number");
        return sign * value;
    }

    const QString m_text;
    qsizetype m_pos = 0;
};

}

namespace Json5 {

QJsonValue parse(const QByteArray &data, QString *error) {
    return Parser(QString::fromUtf8(data)).run(error);
}

}
