#include "SpatialMetadata.h"
#include "TopViewPanel.h"
#include "Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

void addCloudPoint(ScanCloud *cloud, double x, double z, double height) {
    cloud->floor.push_back(QPointF(x, z));
    cloud->height.push_back(static_cast<float>(height));
    if (cloud->floor.size() == 1) {
        cloud->xMin = cloud->xMax = x;
        cloud->zMin = cloud->zMax = z;
        cloud->hMin = cloud->hMax = height;
        return;
    }
    cloud->xMin = std::min(cloud->xMin, x);
    cloud->xMax = std::max(cloud->xMax, x);
    cloud->zMin = std::min(cloud->zMin, z);
    cloud->zMax = std::max(cloud->zMax, z);
    cloud->hMin = std::min(cloud->hMin, height);
    cloud->hMax = std::max(cloud->hMax, height);
}

QVector<SpatialObject> demoObjects() {
    const QByteArray ch1 = R"({
        "coordinate_frame":"top_view_m",
        "objects":[
          {"id":"ch1-person-01","class":"person","x_m":1.20,"y_m":1.05,"height_m":1.70,"confidence":0.94},
          {"id":"ch1-person-02","class":"human","x_m":0.35,"y_m":-0.80,"height_m":1.62,"confidence":0.88}
        ]
    })";
    const QByteArray ch2 = R"({
        "coordinate_frame":"lidar_xy_m",
        "objects":[
          {"id":"ch2-person-01","class":"pedestrian","x_m":-1.15,"y_m":1.60,"height_m":1.76,"confidence":0.91}
        ]
    })";
    const QByteArray imageOnly = R"({
        "coordinate_frame":"image",
        "objects":[{"id":"pixel-only","class":"person","bbox":{"left":0.1,"top":0.2,"right":0.2,"bottom":0.6}}]
    })";

    QVector<SpatialObject> objects;
    for (const auto &input : {
             std::pair<QByteArray, int>{ch1, 1},
             std::pair<QByteArray, int>{ch2, 2},
             std::pair<QByteArray, int>{imageOnly, 3}}) {
        const auto parsed = spatial_metadata::parse(input.first, input.second);
        if (!parsed.recognized) return {};
        objects += parsed.objects;
    }
    return objects;
}

bool loadMetadataFile(const QString &path, int channel, QVector<SpatialObject> *objects) {
    if (objects == nullptr) return false;
    objects->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("cannot open metadata file: %s", qPrintable(file.errorString()));
        return false;
    }
    const auto parsed = spatial_metadata::parse(file.readAll(), channel);
    if (!parsed.recognized) {
        qWarning("metadata file was not recognized: %s", qPrintable(parsed.warning));
        return false;
    }
    qInfo("metadata file parsed: objects=%d ignored=%d",
          static_cast<int>(parsed.objects.size()), parsed.ignored);
    *objects = parsed.objects;
    return true;
}

ScanCloud demoCloud() {
    ScanCloud cloud;
    cloud.sourcePath = QStringLiteral("metadata-replay-cloud");
    for (int xi = -30; xi <= 30; ++xi) {
        for (int zi = -30; zi <= 30; ++zi) {
            const double x = xi / 10.0;
            const double z = zi / 10.0;
            const double height = 0.02 * std::sin(x * 2.0) + 0.02 * std::cos(z * 1.5);
            addCloudPoint(&cloud, x, z, height);
        }
    }
    cloud.gridTotal = cloud.count();
    return cloud;
}

bool savePanel(TopViewPanel &panel, const QString &path) {
    const QImage image = panel.grab().toImage();
    return !image.isNull() && image.save(path);
}

} // namespace

int main(int argc, char **argv) {
    QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
    QApplication app(argc, argv);
    Theme::setMode(Theme::Mode::Developer);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Offline CH1~CH4 RTSP metadata 2D/3D replay"));
    parser.addHelpOption();
    QCommandLineOption outOption({"o", "out-dir"},
                                  QStringLiteral("Directory for 2D/3D screenshots"),
                                  QStringLiteral("directory"),
                                  QDir::currentPath());
    QCommandLineOption metadataOption({"i", "metadata-file"},
                                       QStringLiteral("JSON/XML metadata frame to replay (optional)"),
                                       QStringLiteral("file"));
    QCommandLineOption channelOption({"c", "channel"},
                                      QStringLiteral("CH number for --metadata-file"),
                                      QStringLiteral("channel"), QStringLiteral("1"));
    parser.addOption(outOption);
    parser.addOption(metadataOption);
    parser.addOption(channelOption);
    parser.process(app);

    const QString outDir = parser.value(outOption);
    if (!QDir().mkpath(outDir)) return 2;

    bool channelOk = false;
    const int channel = parser.value(channelOption).toInt(&channelOk);
    if (!channelOk || channel < 1 || channel > 4) {
        qWarning("channel must be in the range 1..4");
        return 3;
    }

    const QString metadataPath = parser.value(metadataOption);
    QVector<SpatialObject> objects;
    if (metadataPath.isEmpty()) {
        objects = demoObjects();
    } else if (!loadMetadataFile(metadataPath, channel, &objects)) {
        return 4;
    }
    if (metadataPath.isEmpty() && objects.size() != 4)
        return 5; // 3 top_view objects + 1 filtered image object

    TopViewPanel panel;
    panel.resize(1000, 760);
    panel.setRoomSize(6.0, 6.0);
    panel.setScanCloud(demoCloud());
    panel.setObjects(objects);
    panel.showMap2D();
    panel.show();

    QTimer::singleShot(350, &panel, [&panel, outDir] {
        if (!savePanel(panel, QDir(outDir).filePath(QStringLiteral("top_view_2d.png")))) {
            qApp->exit(6);
            return;
        }
        panel.showCloud3D();
        QTimer::singleShot(700, &panel, [&panel, outDir] {
            if (!savePanel(panel, QDir(outDir).filePath(QStringLiteral("top_view_3d.png")))) {
                qApp->exit(7);
                return;
            }
            qInfo("metadata replay screenshots written to %s", qPrintable(outDir));
            qApp->exit(0);
        });
    });
    return app.exec();
}
