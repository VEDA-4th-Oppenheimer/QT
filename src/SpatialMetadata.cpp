#include "SpatialMetadata.h"
#include "SpatialProjector.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QXmlStreamReader>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace spatial_metadata {
namespace {

QString lower(const QString &s) { return s.trimmed().toLower(); }

QString canonicalFrame(const QString &frame, const QString &fallback) {
    if (frame.trimmed().isEmpty()) return fallback;
    const QString f = lower(frame);
    if (f.contains("pixel") || f.contains("image") || f.contains("bbox") || f.contains("normalized"))
        return QStringLiteral("image");
    if (f.contains("lidar") || f.contains("kit") || f.contains("world") || f.contains("top"))
        return QStringLiteral("top_view_m");
    return frame.trimmed();
}

QString firstString(const QJsonObject &o, std::initializer_list<const char *> keys) {
    for (const char *key : keys) {
        const QJsonValue v = o.value(QLatin1String(key));
        if (v.isString() && !v.toString().trimmed().isEmpty()) return v.toString();
        if (v.isDouble()) return QString::number(v.toDouble());
    }
    return {};
}

bool number(const QJsonObject &o, std::initializer_list<const char *> keys, double *out) {
    for (const char *key : keys) {
        const QJsonValue v = o.value(QLatin1String(key));
        if (v.isDouble()) {
            *out = v.toDouble();
            return std::isfinite(*out);
        }
        if (v.isString()) {
            bool ok = false;
            const double n = v.toString().toDouble(&ok);
            if (ok && std::isfinite(n)) {
                *out = n;
                return true;
            }
        }
    }
    return false;
}

bool nestedNumber(const QJsonObject &o, const char *key, double *x, double *y) {
    const QJsonObject n = o.value(QLatin1String(key)).toObject();
    return !n.isEmpty()
        && number(n, {"x_m", "x", "cx", "center_x"}, x)
        && number(n, {"y_m", "y", "cy", "center_y"}, y);
}

QString coordinateFrame(const QJsonObject &root, const QJsonObject &item) {
    QString frame = firstString(item, {"coordinate_frame", "coord_frame", "frame"});
    if (frame.isEmpty()) frame = firstString(root, {"coordinate_frame", "coord_frame", "frame"});
    return canonicalFrame(frame, QStringLiteral("top_view_m"));
}

QString normalizedClass(const QString &cls, bool defaultPerson = false) {
    const QString c = lower(cls);
    if (c.contains("person") || c.contains("human") || c.contains("pedestrian") || c.contains("head"))
        return QStringLiteral("PERSON");
    if (c.contains("vehicle") || c.contains("car") || c.contains("truck") || c.contains("bus"))
        return QStringLiteral("VEHICLE");
    return defaultPerson ? QStringLiteral("PERSON") : cls.trimmed().toUpper();
}

QVector<QJsonObject> objectArray(const QJsonObject &root, bool *defaultPerson) {
    for (const char *key : {"objects", "detections", "targets", "persons", "people"}) {
        const QJsonValue v = root.value(QLatin1String(key));
        if (v.isArray()) {
            const QString name = QString::fromLatin1(key);
            *defaultPerson = name == QStringLiteral("persons") || name == QStringLiteral("people");
            QVector<QJsonObject> out;
            for (const QJsonValue &entry : v.toArray()) {
                if (entry.isObject()) out.push_back(entry.toObject());
            }
            return out;
        }
    }
    if (root.contains("x") || root.contains("x_m") || root.contains("position")) {
        *defaultPerson = true;
        return {root};
    }
    return {};
}

bool objectPosition(const QJsonObject &item, double *x, double *y, bool *imageBox, QRectF *pixelBBox) {
    *imageBox = false;
    if (pixelBBox) *pixelBBox = QRectF();

    if (number(item, {"x_m", "x", "cx", "center_x"}, x) && number(item, {"y_m", "y", "cy", "center_y"}, y)) {
        return true;
    }
    if (nestedNumber(item, "position", x, y)
        || nestedNumber(item, "center", x, y)
        || nestedNumber(item, "location", x, y)
        || nestedNumber(item, "point", x, y)) {
        return true;
    }

    const QJsonObject bbox = item.value(QStringLiteral("bbox")).toObject();
    if (!bbox.isEmpty()) {
        double left = 0.0, top = 0.0, right = 0.0, bottom = 0.0;
        if (number(bbox, {"left", "x1", "x"}, &left)
            && number(bbox, {"top", "y1", "y"}, &top)
            && number(bbox, {"right", "x2", "w", "width"}, &right)
            && number(bbox, {"bottom", "y2", "h", "height"}, &bottom)) {
            if (bbox.contains("w") || bbox.contains("width")) right += left;
            if (bbox.contains("h") || bbox.contains("height")) bottom += top;
            *x = (left + right) * 0.5;
            *y = (top + bottom) * 0.5;
            *imageBox = true;
            if (pixelBBox) *pixelBBox = QRectF(QPointF(left, top), QPointF(right, bottom));
            return true;
        }
    }
    return false;
}

void appendJsonObjects(const QJsonObject &root, int channel, ParseResult *result) {
    bool defaultPerson = false;
    const QVector<QJsonObject> items = objectArray(root, &defaultPerson);
    if (items.isEmpty()) return;

    for (const QJsonObject &item : items) {
        double x = 0.0, y = 0.0;
        bool imageBox = false;
        QRectF pixelBBox;
        if (!objectPosition(item, &x, &y, &imageBox, &pixelBBox)) {
            ++result->ignored;
            continue;
        }

        SpatialObject obj;
        obj.channel = item.value(QStringLiteral("channel")).toInt(channel);
        obj.id = firstString(item, {"id", "track_id", "target_id", "object_id", "name"});
        obj.cls = normalizedClass(firstString(item, {"class", "type", "label", "category"}), defaultPerson);
        obj.confidence = 0.0;
        number(item, {"confidence", "score", "prob"}, &obj.confidence);
        obj.heightM = 0.0;
        number(item, {"height_m", "height", "z_m", "z"}, &obj.heightM);
        obj.pixelBBox = pixelBBox;

        const QString frame = coordinateFrame(root, item);
        obj.coordinateFrame = imageBox ? QStringLiteral("image") : frame;
        obj.posM = QPointF(x, y);

        if (!obj.hasTopViewPosition() && SpatialProjector::instance().hasProfile(obj.channel)) {
            QPointF topViewPos;
            double dist = 0.0;
            bool projected = false;
            if (!obj.pixelBBox.isEmpty()) {
                projected = SpatialProjector::instance().projectBBoxToGround(obj.channel, obj.pixelBBox, obj.heightM, &topViewPos, &dist, obj.id);
            } else {
                projected = SpatialProjector::instance().projectImageToGround(obj.channel, x, y, obj.heightM, &topViewPos, &dist);
            }
            if (projected) {
                obj.posM = topViewPos;
                obj.distM = dist;
                obj.coordinateFrame = QStringLiteral("top_view_m");
                obj.isProjected = true;
            }
        }

        if (!obj.hasTopViewPosition()) {
            ++result->ignored;
            result->objects.push_back(obj);
            continue;
        }

        obj.distM = std::hypot(obj.posM.x(), obj.posM.y());
        result->objects.push_back(obj);
    }
}

QString xmlAttribute(const QXmlStreamAttributes &a, std::initializer_list<const char *> keys) {
    for (const char *key : keys) {
        if (a.hasAttribute(QLatin1String(key))) {
            return a.value(QLatin1String(key)).toString().trimmed();
        }
    }
    return {};
}

QString xmlPositionFrame(const QXmlStreamAttributes &a) {
    const QString f = xmlAttribute(a, {"coordinate_frame", "coord_frame", "frame", "system", "space"});
    return canonicalFrame(f, {});
}

bool xmlNumber(const QXmlStreamAttributes &a, std::initializer_list<const char *> keys, double *out) {
    const QString text = xmlAttribute(a, keys);
    if (text.isEmpty()) return false;
    bool ok = false;
    const double value = text.toDouble(&ok);
    if (!ok || !std::isfinite(value)) return false;
    *out = value;
    return true;
}

void parseXmlRegex(const QString &text, int channel, ParseResult *result) {
    static const QRegularExpression objRe(
        QStringLiteral("<(?:[a-zA-Z0-9_-]+:)?Object\\b([^>]*)>(.*?)</(?:[a-zA-Z0-9_-]+:)?Object>"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);

    static const QRegularExpression idRe(QStringLiteral("ObjectId=\"([^\"]+)\""), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression bboxRe(
        QStringLiteral("<(?:[a-zA-Z0-9_-]+:)?BoundingBox\\b[^>]*left=\"([\\d.-]+)\"[^>]*top=\"([\\d.-]+)\"[^>]*right=\"([\\d.-]+)\"[^>]*bottom=\"([\\d.-]+)\""),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression typeRe(
        QStringLiteral("<(?:[a-zA-Z0-9_-]+:)?Type(?:\\s+Likelihood=\"([\\d.-]+)\")?[^>]*>([^<]+)</(?:[a-zA-Z0-9_-]+:)?Type>"),
        QRegularExpression::CaseInsensitiveOption);

    auto it = objRe.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        QString attrs = m.captured(1);
        QString body = m.captured(2);

        SpatialObject obj;
        obj.channel = channel;

        auto idMatch = idRe.match(attrs);
        if (idMatch.hasMatch()) obj.id = idMatch.captured(1);

        auto bboxMatch = bboxRe.match(body);
        if (bboxMatch.hasMatch()) {
            double left = bboxMatch.captured(1).toDouble();
            double top = bboxMatch.captured(2).toDouble();
            double right = bboxMatch.captured(3).toDouble();
            double bottom = bboxMatch.captured(4).toDouble();
            obj.pixelBBox = QRectF(QPointF(left, top), QPointF(right, bottom));
            obj.posM = QPointF((left + right) * 0.5, (top + bottom) * 0.5);
            obj.coordinateFrame = QStringLiteral("image");
        }

        auto typeMatch = typeRe.match(body);
        if (typeMatch.hasMatch()) {
            if (!typeMatch.captured(1).isEmpty()) obj.confidence = typeMatch.captured(1).toDouble();
            obj.cls = normalizedClass(typeMatch.captured(2), false);
        } else {
            obj.cls = QStringLiteral("PERSON");
        }

        if (obj.pixelBBox.isValid() && SpatialProjector::instance().hasProfile(channel)) {
            QPointF topViewPos;
            double dist = 0.0;
            if (SpatialProjector::instance().projectBBoxToGround(channel, obj.pixelBBox, 0.0, &topViewPos, &dist, obj.id)) {
                obj.posM = topViewPos;
                obj.distM = dist;
                obj.coordinateFrame = QStringLiteral("top_view_m");
                obj.isProjected = true;
            }
        }

        result->objects.push_back(obj);
        result->recognized = true;
    }
}

void parseXml(const QByteArray &payload, int channel, ParseResult *result) {
    QXmlStreamReader xml(payload);
    SpatialObject current;
    bool inObject = false;
    bool explicitObjectFrame = false;
    bool havePosition = false;
    bool sawMetadata = false;
    QString pendingPosition;
    QString pendingPositionFrame;
    bool pendingX = false;
    bool pendingY = false;
    double pendingXValue = 0.0;
    double pendingYValue = 0.0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString().toLower();
            if (name == QStringLiteral("object")) {
                current = SpatialObject();
                current.channel = channel;
                current.id = xmlAttribute(xml.attributes(), {"ObjectId", "object_id", "id"});
                const QString frameAttribute = xmlAttribute(
                    xml.attributes(), {"coordinate_frame", "coord_frame", "frame"});
                explicitObjectFrame = !frameAttribute.isEmpty();
                current.coordinateFrame = canonicalFrame(
                    frameAttribute,
                    QStringLiteral("image"));
                inObject = true;
                havePosition = false;
                pendingPosition.clear();
                pendingPositionFrame.clear();
                pendingX = pendingY = false;
                sawMetadata = true;
                continue;
            }
            if (!inObject) continue;
            if (name == QStringLiteral("type") || name == QStringLiteral("class") || name == QStringLiteral("label")) {
                const QXmlStreamAttributes attrs = xml.attributes();
                const QString value = xml.readElementText(QXmlStreamReader::SkipChildElements);
                const QString normalized = normalizedClass(value, false);
                if (current.cls.isEmpty() || normalized == QStringLiteral("PERSON")) {
                    current.cls = normalized;
                }
                const QString likelihood = xmlAttribute(attrs, {"Likelihood", "likelihood", "confidence"});
                if (!likelihood.isEmpty()) current.confidence = likelihood.toDouble();
                continue;
            }
            if (name == QStringLiteral("boundingbox")) {
                double left = 0.0, top = 0.0, right = 0.0, bottom = 0.0;
                if (xmlNumber(xml.attributes(), {"left", "x1"}, &left)
                    && xmlNumber(xml.attributes(), {"top", "y1"}, &top)
                    && xmlNumber(xml.attributes(), {"right", "x2"}, &right)
                    && xmlNumber(xml.attributes(), {"bottom", "y2"}, &bottom)) {
                    current.pixelBBox = QRectF(QPointF(left, top), QPointF(right, bottom));
                    current.posM = QPointF((left + right) * 0.5, (top + bottom) * 0.5);
                    const QString frame = xmlPositionFrame(xml.attributes());
                    if (!frame.isEmpty()) {
                        current.coordinateFrame = frame;
                    } else if (!explicitObjectFrame) {
                        current.coordinateFrame = QStringLiteral("image");
                    }
                    havePosition = true;
                }
                continue;
            }
            if (name == QStringLiteral("centerofgravity") || name == QStringLiteral("point")
                || name == QStringLiteral("position") || name == QStringLiteral("location")) {
                pendingPosition = name;
                pendingPositionFrame = xmlPositionFrame(xml.attributes());
                if (pendingPositionFrame.isEmpty()) pendingPositionFrame = current.coordinateFrame;
                pendingX = pendingY = false;
                double x = 0.0, y = 0.0;
                if (xmlNumber(xml.attributes(), {"x_m", "world_x_m", "x"}, &x)
                    && xmlNumber(xml.attributes(), {"y_m", "world_y_m", "y"}, &y)) {
                    current.posM = QPointF(x, y);
                    current.coordinateFrame = pendingPositionFrame;
                    havePosition = true;
                    pendingPosition.clear();
                    pendingPositionFrame.clear();
                }
                continue;
            }
            if (!pendingPosition.isEmpty() && (name == QStringLiteral("x") || name == QStringLiteral("y"))) {
                bool ok = false;
                const double value = xml.readElementText(QXmlStreamReader::SkipChildElements).toDouble(&ok);
                if (ok && std::isfinite(value)) {
                    if (name == QStringLiteral("x")) {
                        pendingX = true;
                        pendingXValue = value;
                    } else {
                        pendingY = true;
                        pendingYValue = value;
                    }
                    if (pendingX && pendingY) {
                        current.posM = QPointF(pendingXValue, pendingYValue);
                        current.coordinateFrame = pendingPositionFrame;
                        havePosition = true;
                        pendingPosition.clear();
                        pendingPositionFrame.clear();
                    }
                }
            }
        } else if (xml.isEndElement()
                   && !pendingPosition.isEmpty()
                   && xml.name().toString().compare(pendingPosition, Qt::CaseInsensitive) == 0) {
            pendingPosition.clear();
            pendingPositionFrame.clear();
            pendingX = pendingY = false;
        } else if (xml.isEndElement()
                   && xml.name().toString().compare(QStringLiteral("Object"), Qt::CaseInsensitive) == 0
                   && inObject) {
            if (current.cls.isEmpty()) current.cls = QStringLiteral("PERSON");
            if (havePosition) {
                if (!current.hasTopViewPosition() && SpatialProjector::instance().hasProfile(channel)) {
                    QPointF topViewPos;
                    double dist = 0.0;
                    bool projected = false;
                    if (!current.pixelBBox.isEmpty()) {
                        projected = SpatialProjector::instance().projectBBoxToGround(channel, current.pixelBBox, 0.0, &topViewPos, &dist, current.id);
                    } else {
                        projected = SpatialProjector::instance().projectImageToGround(channel, current.posM.x(), current.posM.y(), 0.0, &topViewPos, &dist);
                    }
                    if (projected) {
                        current.posM = topViewPos;
                        if (current.distM <= 0.0) current.distM = dist;
                        current.coordinateFrame = QStringLiteral("top_view_m");
                        current.isProjected = true;
                    }
                }

                if (!current.hasTopViewPosition()) ++result->ignored;
                result->objects.push_back(current);
            } else {
                ++result->ignored;
            }
            inObject = false;
        }
    }
    if (!xml.hasError() && sawMetadata) result->recognized = true;
    if (xml.hasError()) result->warning = xml.errorString();

    // QXmlStreamReader로 객체를 못 찾은 경우 정규식 Fallback 파서 시도
    if (result->objects.isEmpty() && payload.contains("BoundingBox")) {
        parseXmlRegex(QString::fromUtf8(payload), channel, result);
    }
}

QByteArray jsonSlice(const QByteArray &payload) {
    const int objectStart = payload.indexOf('{');
    const int arrayStart = payload.indexOf('[');
    int start = -1;
    if (objectStart >= 0 && arrayStart >= 0) start = std::min(objectStart, arrayStart);
    else start = std::max(objectStart, arrayStart);
    if (start < 0) return {};
    const int objectEnd = payload.lastIndexOf('}');
    const int arrayEnd = payload.lastIndexOf(']');
    const int end = std::max(objectEnd, arrayEnd);
    return end >= start ? payload.mid(start, end - start + 1) : QByteArray();
}

QByteArray xmlSlice(const QByteArray &payload) {
    const int start = payload.indexOf('<');
    const int end = payload.lastIndexOf('>');
    return start >= 0 && end >= start ? payload.mid(start, end - start + 1) : QByteArray();
}

} // namespace

ParseResult parse(const QByteArray &payload, int channel) {
    ParseResult result;
    const QByteArray text = payload.trimmed();
    if (text.isEmpty()) return result;

    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(text, &error);
    if (document.isNull()) {
        const QByteArray slice = jsonSlice(text);
        if (!slice.isEmpty()) document = QJsonDocument::fromJson(slice, &error);
    }
    if (!document.isNull()) {
        result.recognized = true;
        if (document.isObject()) appendJsonObjects(document.object(), channel, &result);
        else if (document.isArray()) {
            QJsonObject root;
            root.insert(QStringLiteral("objects"), document.array());
            appendJsonObjects(root, channel, &result);
        }
        return result;
    }

    const QByteArray lowerText = text.toLower();
    const bool wiseAiObjectFrame = lowerText.contains("objectdetection") || lowerText.contains("boundingbox");
    const bool looksLikeXml = text.contains('<')
        && (text.contains("<Object") || text.contains(":Object")
            || text.contains("<Frame") || text.contains(":Frame")
            || text.contains("BoundingBox") || text.contains("boundingbox")
            || (lowerText.contains("metadatastream") && wiseAiObjectFrame));
    if (looksLikeXml) {
        const QByteArray slice = xmlSlice(text);
        parseXml(slice.isEmpty() ? text : slice, channel, &result);
        if (!result.recognized && result.warning.isEmpty() && wiseAiObjectFrame)
            result.recognized = true;
        return result;
    }
    result.warning = error.errorString();
    return result;
}

} // namespace spatial_metadata
