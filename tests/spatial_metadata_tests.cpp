#include "SpatialMetadata.h"

#include <QCoreApplication>
#include <QDebug>

#include <cmath>

namespace {
void require(bool condition, const char *message) {
    if (!condition) qFatal("spatial_metadata_tests: %s", message);
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    const auto json = spatial_metadata::parse(
        R"({"coordinate_frame":"top_view_m","objects":[{"id":"17","class":"person","x":1.25,"y":-2.5,"confidence":0.91}]})",
        3);
    require(json.recognized && json.objects.size() == 1, "JSON object was not parsed");
    require(json.objects.first().channel == 3, "JSON channel was not attached");
    require(json.objects.first().hasTopViewPosition(), "top_view_m was rejected");
    require(json.objects.first().cls == "PERSON", "person class was not normalized");
    require(std::abs(json.objects.first().posM.x() - 1.25) < 1e-9, "JSON x mismatch");

    const auto xml = spatial_metadata::parse(
        QByteArrayLiteral("<tt:Frame xmlns:tt=\"urn:schemas-onvif-org:ver10/schema\">"
                          "<tt:Object ObjectId=\"A\" coordinate_frame=\"top_view_m\"><tt:Type Likelihood=\"0.8\">Human</tt:Type>"
                          "<tt:CenterOfGravity x=\"-0.5\" y=\"2.0\"/></tt:Object></tt:Frame>"),
        1);
    require(xml.recognized && xml.objects.size() == 1, "ONVIF XML object was not parsed");
    require(xml.objects.first().cls == "PERSON", "ONVIF Human class was not normalized");
    require(xml.objects.first().hasTopViewPosition(), "ONVIF x/y was not treated as top-view");

    const auto standardXml = spatial_metadata::parse(
        QByteArrayLiteral("<tt:Frame xmlns:tt=\"urn:schemas-onvif-org:ver10/schema\">"
                          "<tt:Object ObjectId=\"B\"><tt:Type>Human</tt:Type>"
                          "<tt:CenterOfGravity><tt:Point><tt:x>0.5</tt:x><tt:y>0.7</tt:y>"
                          "</tt:Point></tt:CenterOfGravity></tt:Object></tt:Frame>"),
        4);
    require(standardXml.recognized && standardXml.objects.size() == 1,
            "standard ONVIF XML object was not parsed");
    require(!standardXml.objects.first().hasTopViewPosition(),
            "standard ONVIF image coordinates were incorrectly plotted as Top-View");

    const auto nestedStandardXml = spatial_metadata::parse(
        QByteArrayLiteral("<tt:MetadataStream xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
                          "<tt:VideoAnalytics><tt:Frame UtcTime=\"2026-08-18T08:00:00Z\">"
                          "<tt:Object ObjectId=\"22\"><tt:Appearance><tt:Shape>"
                          "<tt:BoundingBox left=\"20\" top=\"80\" right=\"100\" bottom=\"30\"/>"
                          "<tt:CenterOfGravity x=\"60\" y=\"50\"/>"
                          "</tt:Shape><tt:Class><tt:ClassCandidate><tt:Type>Human</tt:Type>"
                          "<tt:Likelihood>0.93</tt:Likelihood></tt:ClassCandidate></tt:Class>"
                          "</tt:Appearance></tt:Object></tt:Frame></tt:VideoAnalytics>"
                          "</tt:MetadataStream>"),
        4);
    require(nestedStandardXml.recognized && nestedStandardXml.objects.size() == 1,
            "nested standard ONVIF XML object was not parsed");
    require(nestedStandardXml.objects.first().cls == "PERSON",
            "nested ONVIF Human class was not normalized");
    require(!nestedStandardXml.objects.first().hasTopViewPosition()
                && nestedStandardXml.ignored == 1,
            "nested standard ONVIF image coordinates were incorrectly plotted");

    const auto nestedTopViewXml = spatial_metadata::parse(
        QByteArrayLiteral("<tt:MetadataStream xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
                          "<tt:VideoAnalytics><tt:Frame><tt:Object ObjectId=\"kit-22\" "
                          "coordinate_frame=\"top_view_m\"><tt:Appearance><tt:Shape>"
                          "<tt:BoundingBox left=\"0\" top=\"0\" right=\"1\" bottom=\"1\"/>"
                          "<tt:CenterOfGravity x=\"1.2\" y=\"-0.4\"/>"
                          "</tt:Shape></tt:Appearance><tt:Type>Human</tt:Type>"
                          "</tt:Object></tt:Frame></tt:VideoAnalytics></tt:MetadataStream>"),
        2);
    require(nestedTopViewXml.recognized && nestedTopViewXml.objects.size() == 1
                && nestedTopViewXml.objects.first().hasTopViewPosition(),
            "explicit top_view_m was lost when ONVIF BoundingBox preceded CenterOfGravity");

    const auto emptyWiseAi = spatial_metadata::parse(
        QByteArrayLiteral("<tt:MetadataStream xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
                          "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\">"
                          "<tt:Event><wsnt:NotificationMessage><wsnt:Topic>"
                          "tns1:OpenApp/WiseAI/ObjectDetection</wsnt:Topic>"
                          "<wsnt:Message><tt:Message><tt:Data>"
                          "<tt:SimpleItem Name=\"State\" Value=\"false\"/>"
                          "</tt:Data></tt:Message></wsnt:Message>"
                          "</wsnt:NotificationMessage></tt:Event></tt:MetadataStream>"),
        1);
    require(emptyWiseAi.recognized && emptyWiseAi.objects.isEmpty(),
            "empty WiseAI ObjectDetection frame was not recognized");

    const QByteArray framedXml = QByteArrayLiteral("\x01\x02")
        + QByteArrayLiteral("<tt:Frame xmlns:tt=\"urn:schemas-onvif-org:ver10/schema\">"
                            "<tt:Object coordinate_frame=\"top_view_m\"><tt:Point x_m=\"2.0\" y_m=\"3.0\"/>"
                            "</tt:Object></tt:Frame>");
    const auto prefixedXml = spatial_metadata::parse(framedXml, 2);
    require(prefixedXml.recognized && prefixedXml.objects.size() == 1
                && prefixedXml.objects.first().hasTopViewPosition(),
            "prefixed XML metadata was not recovered");

    const QByteArray framedXmlWithSuffix = QByteArrayLiteral("\x01\x02")
        + QByteArrayLiteral("<tt:Frame xmlns:tt=\"urn:schemas-onvif-org:ver10/schema\">")
        + QByteArrayLiteral("<tt:Object ObjectId=\"suffix-1\" coordinate_frame=\"top_view_m\">")
        + QByteArrayLiteral("<tt:Type>Human</tt:Type><tt:CenterOfGravity x=\"0.25\" y=\"-0.75\"/>")
        + QByteArrayLiteral("</tt:Object></tt:Frame>")
        + QByteArray::fromHex("00ff");
    const auto suffixedXml = spatial_metadata::parse(framedXmlWithSuffix, 2);
    require(suffixedXml.recognized && suffixedXml.objects.size() == 1
                && suffixedXml.objects.first().hasTopViewPosition(),
            "binary-suffixed XML metadata was not recovered");

    const auto bbox = spatial_metadata::parse(
        R"({"objects":[{"class":"person","bbox":{"left":0.1,"top":0.2,"right":0.3,"bottom":0.6}}]})",
        2);
    require(bbox.recognized && bbox.objects.size() == 1, "bbox metadata was not recognized");
    require(!bbox.objects.first().hasTopViewPosition(), "image bbox was incorrectly treated as world position");

    qInfo() << "spatial_metadata_tests: PASS";
    return 0;
}
