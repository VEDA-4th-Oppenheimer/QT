#include "ScanCloud.h"
#include <QByteArray>
#include <QFile>
#include <QStringList>
#include <cmath>
#include <limits>

namespace {

// "# sensor_height_m = 0.4700 (좌표에 미적용 — 메타데이터)" 같은 주석에서 값만 뽑는다.
double commentValue(const QByteArray &line, const char *key, bool *found) {
    const int k = line.indexOf(key);
    if (k < 0) { *found = false; return 0.0; }
    const int eq = line.indexOf('=', k);
    if (eq < 0) { *found = false; return 0.0; }
    bool ok = false;
    // '=' 뒤 첫 토큰만 — 뒤에 괄호 주석이 붙어 있어도 무시된다.
    const QByteArray rest = line.mid(eq + 1).trimmed();
    const QByteArray tok = rest.split(' ').value(0);
    const double v = tok.toDouble(&ok);
    *found = ok;
    return ok ? v : 0.0;
}

}   // namespace

bool parsePcdAscii(const QByteArray &data, const QString &label,
                   ScanCloud *out, QString *err) {
    if (out == nullptr) return false;
    *out = ScanCloud();
    out->sourcePath = label;

    const QList<QByteArray> lines = data.split('\n');
    if (lines.isEmpty()) {
        if (err) *err = QStringLiteral("빈 파일");
        return false;
    }

    int  headerEnd = -1;
    bool sawFields = false, fieldsOk = false;
    bool asciiData = false;
    int  declaredPoints = 0;

    for (int i = 0; i < lines.size() && i < 64; ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith('#')) {
            bool found = false;
            const double h = commentValue(line, "sensor_height_m", &found);
            if (found) out->sensorHeightM = h;
            continue;
        }

        const QList<QByteArray> tok = line.simplified().split(' ');
        const QByteArray key = tok.value(0).toUpper();

        if (key == "FIELDS") {
            sawFields = true;
            // x y z 세 필드로 시작해야 한다. 뒤에 intensity 등이 더 붙는 건 허용하지
            // 않는다 — 데몬은 x y z 만 쓰고, 더 있으면 열 위치 가정이 깨진다.
            fieldsOk = (tok.size() == 4 && tok.at(1) == "x" && tok.at(2) == "y" && tok.at(3) == "z");
        } else if (key == "WIDTH") {
            out->width = tok.value(1).toInt();
        } else if (key == "HEIGHT") {
            out->height_ = tok.value(1).toInt();
        } else if (key == "POINTS") {
            declaredPoints = tok.value(1).toInt();
        } else if (key == "DATA") {
            asciiData = (tok.value(1).toLower() == "ascii");
            headerEnd = i;
            break;
        }
    }

    if (headerEnd < 0) {
        if (err) *err = QStringLiteral("PCD 헤더에 DATA 줄이 없다");
        return false;
    }
    if (!sawFields || !fieldsOk) {
        if (err) *err = QStringLiteral("지원하지 않는 FIELDS — x y z 세 개여야 한다");
        return false;
    }
    if (!asciiData) {
        if (err) *err = QStringLiteral("ASCII PCD 만 지원한다 (binary 는 데몬이 쓰지 않는다)");
        return false;
    }

    out->gridTotal = declaredPoints > 0 ? declaredPoints
                                        : qMax(0, out->width * out->height_);
    out->floor.reserve(out->gridTotal > 0 ? out->gridTotal : 4096);
    out->height.reserve(out->gridTotal > 0 ? out->gridTotal : 4096);

    double xMin = std::numeric_limits<double>::max(), xMax = -xMin;
    double zMin = xMin, zMax = -xMin;
    double hMin = xMin, hMax = -xMin;

    for (int i = headerEnd + 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty()) continue;

        const QList<QByteArray> tok = line.simplified().split(' ');
        if (tok.size() < 3) continue;

        // 미반사 격자점은 "nan nan nan" 으로 들어온다. toDouble 이 0 을 돌려주므로
        // 문자열 단계에서 걸러야 원점에 가짜 점이 쌓이지 않는다.
        bool okX = false, okY = false, okZ = false;
        const double x = tok.at(0).toDouble(&okX);
        const double y = tok.at(1).toDouble(&okY);
        const double z = tok.at(2).toDouble(&okZ);
        if (!okX || !okY || !okZ || std::isnan(x) || std::isnan(y) || std::isnan(z)) {
            ++out->invalid;
            continue;
        }

        const double up = -y;   // PCD 는 +y 아래 — 화면 표시용으로 뒤집는다
        out->floor.push_back(QPointF(x, z));
        out->height.push_back(static_cast<float>(up));

        if (x < xMin) xMin = x;   if (x > xMax) xMax = x;
        if (z < zMin) zMin = z;   if (z > zMax) zMax = z;
        if (up < hMin) hMin = up; if (up > hMax) hMax = up;
    }

    if (out->floor.isEmpty()) {
        if (err) *err = QStringLiteral("유효한 점이 없다 (전부 미반사)");
        return false;
    }

    out->xMin = xMin; out->xMax = xMax;
    out->zMin = zMin; out->zMax = zMax;
    out->hMin = hMin; out->hMax = hMax;
    if (out->gridTotal < out->floor.size()) out->gridTotal = out->floor.size() + out->invalid;
    return true;
}

bool loadPcdAscii(const QString &path, ScanCloud *out, QString *err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("열 수 없음: %1").arg(f.errorString());
        return false;
    }
    return parsePcdAscii(f.readAll(), path, out, err);
}
