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

// binary DATA 의 x/y/z 필드 하나의 바이트 레이아웃. TYPE F(float) 만 지원 —
// 좌표 필드가 정수/문자 타입인 PCD 는 실질적으로 없다.
struct FieldLayout {
    int size = 4;   // 4(float32) 또는 8(float64)
};

double readField(const char *p, const FieldLayout &f) {
    return f.size == 8 ? *reinterpret_cast<const double *>(p)
                        : static_cast<double>(*reinterpret_cast<const float *>(p));
}

}   // namespace

bool parsePcd(const QByteArray &data, const QString &label,
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
    int  declaredPoints = 0;
    QByteArray dataMode;
    QList<QByteArray> sizeTok, typeTok, countTok;

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
        } else if (key == "SIZE") {
            sizeTok = tok;
        } else if (key == "TYPE") {
            typeTok = tok;
        } else if (key == "COUNT") {
            countTok = tok;
        } else if (key == "WIDTH") {
            out->width = tok.value(1).toInt();
        } else if (key == "HEIGHT") {
            out->height_ = tok.value(1).toInt();
        } else if (key == "POINTS") {
            declaredPoints = tok.value(1).toInt();
        } else if (key == "DATA") {
            dataMode = tok.value(1).toLower();
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

    out->gridTotal = declaredPoints > 0 ? declaredPoints
                                        : qMax(0, out->width * out->height_);
    out->floor.reserve(out->gridTotal > 0 ? out->gridTotal : 4096);
    out->height.reserve(out->gridTotal > 0 ? out->gridTotal : 4096);

    double xMin = std::numeric_limits<double>::max(), xMax = -xMin;
    double zMin = xMin, zMax = -xMin;
    double hMin = xMin, hMax = -xMin;

    if (dataMode == "ascii") {
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
    } else if (dataMode == "binary") {
        // COUNT 는 필드당 1개만 지원한다(배열 필드는 데몬/일반 스캐너 출력에 없음).
        if (!countTok.isEmpty()) {
            const bool countOk = countTok.size() == 4 && countTok.at(1) == "1"
                                 && countTok.at(2) == "1" && countTok.at(3) == "1";
            if (!countOk) {
                if (err) *err = QStringLiteral("지원하지 않는 COUNT — x y z 필드는 각각 1개여야 한다");
                return false;
            }
        }
        if (sizeTok.size() != 4 || typeTok.size() != 4) {
            if (err) *err = QStringLiteral("binary PCD 헤더에 SIZE/TYPE 줄이 없다");
            return false;
        }

        FieldLayout layout[3];
        int stride = 0;
        for (int f = 0; f < 3; ++f) {
            bool okSize = false;
            const int sz = sizeTok.at(f + 1).toInt(&okSize);
            const QByteArray ty = typeTok.at(f + 1).toUpper();
            if (!okSize || ty != "F" || (sz != 4 && sz != 8)) {
                if (err) *err = QStringLiteral("지원하지 않는 필드 타입 — x y z 는 TYPE F(float32/float64) 만 지원한다");
                return false;
            }
            layout[f].size = sz;
            stride += sz;
        }

        // 헤더가 끝난 바로 다음 바이트부터 binary 페이로드가 시작된다.
        // (헤더 검색에 쓴 data.split('\n') 의 라인 길이를 되짚어 오프셋을 구한다 —
        // binary 페이로드 안의 '\n' 바이트가 헤더 라인으로 잘못 쪼개지지 않도록,
        // 오프셋 계산은 DATA 줄까지의 헤더 구간에서만 한다.)
        qint64 offset = 0;
        for (int i = 0; i <= headerEnd; ++i) offset += lines.at(i).size() + 1;

        const int numPoints = out->gridTotal;
        if (numPoints <= 0) {
            if (err) *err = QStringLiteral("binary PCD 에 POINTS 또는 WIDTH/HEIGHT 선언이 없다");
            return false;
        }
        const qint64 needed = static_cast<qint64>(numPoints) * stride;
        if (offset + needed > data.size()) {
            if (err) *err = QStringLiteral("binary 페이로드가 선언된 점 수보다 짧다");
            return false;
        }

        const char *base = data.constData() + offset;
        for (int p = 0; p < numPoints; ++p) {
            const char *rec = base + static_cast<qint64>(p) * stride;
            const double x = readField(rec, layout[0]);
            const double y = readField(rec + layout[0].size, layout[1]);
            const double z = readField(rec + layout[0].size + layout[1].size, layout[2]);
            if (std::isnan(x) || std::isnan(y) || std::isnan(z)) {
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
    } else if (dataMode == "binary_compressed") {
        if (err) *err = QStringLiteral("binary_compressed PCD 는 지원하지 않는다 (LZF 압축 해제 필요)");
        return false;
    } else {
        if (err) *err = QStringLiteral("알 수 없는 DATA 형식: %1").arg(QString::fromUtf8(dataMode));
        return false;
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

bool loadPcd(const QString &path, ScanCloud *out, QString *err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("열 수 없음: %1").arg(f.errorString());
        return false;
    }
    return parsePcd(f.readAll(), path, out, err);
}
