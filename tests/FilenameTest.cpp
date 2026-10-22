/*
 *  SPDX-License-Identifier: GPL-2.0-only OR LGPL-2.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <KSystemClipboard>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QLocale>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPainter>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "ExportManager.h"
#include "ImageMetaData.h"

using namespace Qt::StringLiterals;

class FilenameTest : public QObject
{
    Q_OBJECT

private:
    ExportManager *mExportManager;
    QDateTime timestamp;
    QString defaultTitle;

private Q_SLOTS:

    void initTestCase();
    void testStrings();
    void testDateTokens();
    void testWindowTitle();
    void testNumbering();
    void testCombined();
    void testStereoMimeTypes();
    void testStereoFormatMapping();
    void testExplicitMonoFormat();
    void testMonoWriter_data();
    void testMonoWriter();
    void testExplicitSavePath_data();
    void testExplicitSavePath();
    void testTempSaveEyeChange();
    void testClipboardEye_data();
    void testClipboardEye();
    void testWriterError();
    void testJpsPluginWriter();
    void testMpoPluginWriter();
    void testFlatWriter();
};

void FilenameTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    mExportManager = ExportManager::instance();
    timestamp = QDateTime::fromString(u"2019-03-22T20:43:25Z"_s, Qt::ISODate);
    mExportManager->setTimestamp(timestamp);
    defaultTitle = u"Spectacle"_s;
}

void FilenameTest::testStrings()
{
    QCOMPARE(mExportManager->formattedFilename(u"Screenshot"_s), u"Screenshot"_s);
    // empty string produces Screenshot per default
    QCOMPARE(mExportManager->formattedFilename({}), u"Screenshot"_s);
    // not a placeholder
    QCOMPARE(mExportManager->formattedFilename(u"<"_s), u"<"_s);
    QCOMPARE(mExportManager->formattedFilename(u">"_s), u">"_s);
    QCOMPARE(mExportManager->formattedFilename(u"<>"_s), u"<>"_s);
    QCOMPARE(mExportManager->formattedFilename(u"<notaplaceholder>"_s), u"<notaplaceholder>"_s);
}

void FilenameTest::testDateTokens()
{
    const auto &placeholders = mExportManager->filenamePlaceholders;
    const auto &locale = QLocale::system();
    for (auto it = placeholders.cbegin(); it != placeholders.cend(); ++it) {
        using Flag = ExportManager::Placeholder::Flag;
        if (it->flags.testFlag(Flag::IsQDateTime)) {
            QCOMPARE(mExportManager->formattedFilename(it->token, timestamp), locale.toString(timestamp, it->base));
        }
    }
    QCOMPARE(mExportManager->formattedFilename(u"<h>"_s, timestamp), u"8"_s);
    QCOMPARE(mExportManager->formattedFilename(u"<hh>"_s, timestamp), u"08"_s);
    QCOMPARE(mExportManager->formattedFilename(u"<UnixTime>"_s, timestamp), QString::number(timestamp.toSecsSinceEpoch()));
}

void FilenameTest::testWindowTitle()
{
    enum : std::size_t {
        WithTitle,
        WithoutTitle,
        ArraySize,
    };
    static const QMap<QString, std::array<QString, ArraySize>> comparisons{
        {u"<title>"_s, {u"Spectacle"_s, u"Screenshot"_s}}, // Empty String produces Screenshot
        {u"_<title>_"_s, {u"_Spectacle_"_s, u"_Screenshot_"_s}},
        {u"Before<title>After"_s, {u"BeforeSpectacleAfter"_s, u"BeforeAfter"_s}},
        {u"Before_<title>_After"_s, {u"Before_Spectacle_After"_s, u"Before_After"_s}},
        {u"Before_<title>"_s, {u"Before_Spectacle"_s, u"Before"_s}},
        {u"<title>_After"_s, {u"Spectacle_After"_s, u"After"_s}},
        {u"<mm><title><ss>"_s, {u"43Spectacle25"_s, u"4325"_s}},
        {u"<mm>_<title>_<ss>"_s, {u"43_Spectacle_25"_s, u"43_25"_s}},
        {u"<mm>/<title>/<title>/<ss>"_s, {u"43/Spectacle/Spectacle/25"_s, u"43/25"_s}},
        {u"<mm>_<title>/<title>_<ss>"_s, {u"43_Spectacle/Spectacle_25"_s, u"43/25"_s}},
        {u"<mm>_/<title>/_<ss>"_s, {u"43_/Spectacle/_25"_s, u"43_/_25"_s}},
        {u"<mm>_/_<title>_/_<ss>"_s, {u"43_/_Spectacle_/_25"_s, u"43_/_25"_s}},
        {u"<mm>_/_<title>/_<ss>"_s, {u"43_/_Spectacle/_25"_s, u"43_/_25"_s}},
        {u"<mm>_/<title>_/_<ss>"_s, {u"43_/Spectacle_/_25"_s, u"43_/_25"_s}},
        {u"<mm>_<title>_/_<title>_/_<title>_<ss>"_s, {u"43_Spectacle_/_Spectacle_/_Spectacle_25"_s, u"43_/_25"_s}},
    };
    for (auto it = comparisons.cbegin(); it != comparisons.cend(); ++it) {
        QCOMPARE(mExportManager->formattedFilename(it.key(), timestamp, defaultTitle), it.value()[WithTitle]);
    }
    for (auto it = comparisons.cbegin(); it != comparisons.cend(); ++it) {
        QCOMPARE(mExportManager->formattedFilename(it.key(), timestamp), it.value()[WithoutTitle]);
    }
}

void FilenameTest::testNumbering()
{
    QString BaseName = u"spectacle_test_" + QUuid::createUuid().toString();
    QCOMPARE(mExportManager->formattedFilename(BaseName + u"_<#>"_s), BaseName + u"_1"_s);
    QCOMPARE(mExportManager->formattedFilename(BaseName + u"_<##>"_s), BaseName + u"_01"_s);
    QCOMPARE(mExportManager->formattedFilename(BaseName + u"_<###>"_s), BaseName + u"_001"_s);
    QCOMPARE(mExportManager->formattedFilename(BaseName + u"_<####>"_s), BaseName + u"_0001"_s);
    QCOMPARE(mExportManager->formattedFilename(BaseName + u"_<#>_<##>_<###>"_s), BaseName + u"_1_01_001"_s);

    const auto saveLocation = Settings::imageSaveLocation();
    QFile file(QDir(mExportManager->defaultSaveLocation()).filePath(BaseName + u"_3.png"_s));
    file.open(QIODevice::WriteOnly);
    file.close();
    QCOMPARE(mExportManager->formattedFilename(BaseName + u"_<#>"_s, {}, {}, saveLocation), BaseName + u"_4"_s);
    file.remove();
    file.setFileName(QDir(mExportManager->defaultSaveLocation()).filePath(BaseName + u"_0008"_s));
    file.open(QIODevice::WriteOnly);
    file.close();
    QCOMPARE(mExportManager->formattedFilename(BaseName + u"_<####>"_s, {}, {}, saveLocation), BaseName + u"_0009"_s);
    file.remove();
    file.setFileName(QDir(mExportManager->defaultSaveLocation()).filePath(BaseName + u"_7_07_007"_s));
    file.open(QIODevice::WriteOnly);
    file.close();
    QCOMPARE(mExportManager->formattedFilename(BaseName + u"_<#>_<##>_<###>"_s, {}, {}, saveLocation), BaseName + u"_8_08_008"_s);
    file.remove();
}

void FilenameTest::testCombined()
{
    static const auto filenameTemplate = u"App_<title>/Date_<yyyy><MM><dd>_Time_<hh>:<mm>:<ss><AP><notaplaceholder>"_s;
    QCOMPARE(mExportManager->formattedFilename(filenameTemplate, timestamp, defaultTitle),
             u"App_Spectacle/Date_20190322_Time_08:43:25PM<notaplaceholder>"_s);
    QCOMPARE(mExportManager->formattedFilename(filenameTemplate, timestamp),
             u"App/Date_20190322_Time_08:43:25PM<notaplaceholder>"_s);
}

static void verifyEyeColor(const QImage &image, const QColor &expectedColor)
{
    QCOMPARE(image.width(), 4);
    QCOMPARE(image.height(), 2);
    const QColor color = image.pixelColor(2, 1);
    if (expectedColor == Qt::red) {
        QVERIFY2(color.red() > color.blue(), qPrintable(color.name()));
    } else {
        QVERIFY2(color.blue() > color.red(), qPrintable(color.name()));
    }
}

// The JPS and MPO plugins are in KImageFormats as we carry it, not in the one KDE ships
static bool hasStereoPlugins()
{
    const auto mimeTypes = QImageWriter::supportedMimeTypes();
    return mimeTypes.contains("image/x-jps") && mimeTypes.contains("image/x-mpo");
}

void FilenameTest::testStereoMimeTypes()
{
    if (!hasStereoPlugins()) {
        QSKIP("KImageFormats has no JPS and MPO plugins");
    }

    const QMimeDatabase mimeDatabase;
    QCOMPARE(mimeDatabase.mimeTypeForFile(u"x.jps"_s, QMimeDatabase::MatchExtension).name(), u"image/x-jps"_s);
    QCOMPARE(mimeDatabase.mimeTypeForFile(u"x.mpo"_s, QMimeDatabase::MatchExtension).name(), u"image/x-mpo"_s);
}

static QImage stereoTestImage()
{
    QImage image(8, 2, QImage::Format_RGB32);
    image.fill(Qt::red);
    QPainter painter(&image);
    painter.fillRect(4, 0, 4, 2, Qt::blue);
    painter.end();
    ImageMetaData::setStereo(image, true);
    return image;
}

void FilenameTest::testStereoFormatMapping()
{
    const auto previousFormat = Settings::preferredImageFormat();
    mExportManager->m_saveImage = stereoTestImage();

    Settings::setPreferredImageFormat(u"JPG"_s);
    QCOMPARE(mExportManager->imageFileSuffix({}), u"jpg"_s);
    Settings::setPreferredImageFormat(u"PNG"_s);
    QCOMPARE(mExportManager->imageFileSuffix({}), u"png"_s);

    ImageMetaData::setStereo(mExportManager->m_saveImage, false);
    Settings::setPreferredImageFormat(u"JPG"_s);
    QCOMPARE(mExportManager->imageFileSuffix({}), u"jpg"_s);
    Settings::setPreferredImageFormat(previousFormat);
}

void FilenameTest::testExplicitMonoFormat()
{
    mExportManager->setImage(stereoTestImage());
    for (const auto &suffix : {u"bmp"_s, u"png"_s, u"jpg"_s, u"webp"_s}) {
        QCOMPARE(mExportManager->imageFileSuffix(QUrl(u"file:///tmp/explicit."_s + suffix)), suffix);
    }
}

void FilenameTest::testMonoWriter_data()
{
    QTest::addColumn<QByteArray>("suffix");
    QTest::addColumn<bool>("rightEye");
    QTest::addColumn<qreal>("dpr");
    for (const auto &suffix : {QByteArray("png"), QByteArray("bmp"), QByteArray("jpg"), QByteArray("webp")}) {
        if (!QImageWriter::supportedImageFormats().contains(suffix)) {
            continue;
        }
        for (bool right : {false, true}) {
            for (qreal dpr : {1.0, 1.5, 2.0}) {
                QTest::newRow(qPrintable(QString::fromLatin1(suffix) + QString::number(right) + u'-' + QString::number(dpr))) << suffix << right << dpr;
            }
        }
    }
}

void FilenameTest::testMonoWriter()
{
    QFETCH(QByteArray, suffix);
    QFETCH(bool, rightEye);
    QFETCH(qreal, dpr);
    Settings::setStereoExportEye(rightEye ? Settings::RightEye : Settings::LeftEye);
    auto image = stereoTestImage();
    image.setDevicePixelRatio(dpr);
    image.setText(u"Stereo3DLayout"_s, u"sideBySideLeftFirst"_s);
    image.setText(u"X-KDE::Stereo3DLayout"_s, u"sideBySideLeftFirst"_s);
    mExportManager->setImage(image);
    QBuffer output;
    QVERIFY(output.open(QIODevice::WriteOnly));
    QVERIFY(mExportManager->writeImage(&output, suffix));
    const auto result = QImage::fromData(output.buffer(), suffix.constData());
    QCOMPARE(result.size(), QSize(4, 2));
    verifyEyeColor(result, rightEye ? Qt::blue : Qt::red);
    for (const auto &key : {ImageMetaData::Keys::stereo, u"Stereo3DLayout"_s, u"X-KDE::Stereo3DLayout"_s}) {
        QVERIFY(!result.textKeys().contains(key));
        QVERIFY(!mExportManager->imageForExport(suffix).textKeys().contains(key));
    }
    if (suffix == "png" || suffix == "bmp") {
        auto expected = image.copy(rightEye ? 4 : 0, 0, 4, 2);
        expected.setDevicePixelRatio(1);
        QCOMPARE(result, expected);
    }
    QCOMPARE(mExportManager->imageForExport(suffix).devicePixelRatio(), dpr);
    QCOMPARE(mExportManager->image(), image);
    Settings::setStereoExportEye(Settings::LeftEye);
}

void FilenameTest::testExplicitSavePath_data()
{
    QTest::addColumn<QString>("suffix");
    for (const auto &suffix : {"png", "bmp", "jpg", "webp", "mpo", "jps"}) {
        if (QImageWriter::supportedImageFormats().contains(suffix)) {
            QTest::newRow(suffix) << QString::fromLatin1(suffix);
        }
    }
}

void FilenameTest::testExplicitSavePath()
{
    QFETCH(QString, suffix);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    mExportManager->setImage(stereoTestImage());
    const auto url = QUrl::fromLocalFile(dir.filePath(u"explicit."_s + suffix));
    QSignalSpy exported(mExportManager, &ExportManager::imageExported);
    mExportManager->exportImage(ExportManager::Save, url);
    QCOMPARE(exported.count(), 1);
    QCOMPARE(exported.constFirst().at(1).toUrl(), url);
    QCOMPARE(Settings::lastImageSaveLocation(), url);
    QVERIFY(QFile::exists(url.toLocalFile()));
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1);
    if (suffix != u"mpo" && suffix != u"jps") {
        QCOMPARE(QImage(url.toLocalFile()).size(), QSize(4, 2));
    }
}

void FilenameTest::testTempSaveEyeChange()
{
    const auto previousFormat = Settings::preferredImageFormat();
    Settings::setPreferredImageFormat(u"PNG"_s);
    Settings::setStereoExportEye(Settings::LeftEye);
    mExportManager->setImage(stereoTestImage());
    const auto left = mExportManager->tempSave();
    QVERIFY(left.isLocalFile());
    QCOMPARE(QImage(left.toLocalFile()).pixelColor(2, 1), QColor(Qt::red));
    Settings::setStereoExportEye(Settings::RightEye);
    QVERIFY(!QFile::exists(left.toLocalFile()));
    const auto right = mExportManager->tempSave();
    QVERIFY(right != left);
    QCOMPARE(QImage(right.toLocalFile()).pixelColor(2, 1), QColor(Qt::blue));
    QCOMPARE(QImage(right.toLocalFile()).size(), QSize(4, 2));
    Settings::setStereoExportEye(Settings::LeftEye);
    Settings::setPreferredImageFormat(previousFormat);
}

void FilenameTest::testClipboardEye_data()
{
    QTest::addColumn<bool>("rightEye");
    QTest::newRow("left") << false;
    QTest::newRow("right") << true;
}

void FilenameTest::testClipboardEye()
{
    QFETCH(bool, rightEye);
    const auto previousFormat = Settings::preferredImageFormat();
    Settings::setPreferredImageFormat(u"PNG"_s);
    Settings::setStereoExportEye(rightEye ? Settings::RightEye : Settings::LeftEye);
    mExportManager->setImage(stereoTestImage());
    mExportManager->exportImage(ExportManager::CopyImage);
    const auto data = KSystemClipboard::instance()->mimeData(QClipboard::Clipboard);
    QVERIFY(data);
    const auto encoded = QImage::fromData(data->data(u"image/png"_s), "png");
    QCOMPARE(encoded.size(), QSize(4, 2));
    QCOMPARE(encoded.pixelColor(2, 1), QColor(rightEye ? Qt::blue : Qt::red));
    QCOMPARE(qvariant_cast<QImage>(data->imageData()), encoded);
    QVERIFY(!encoded.textKeys().contains(ImageMetaData::Keys::stereo));
    Settings::setStereoExportEye(Settings::LeftEye);
    Settings::setPreferredImageFormat(previousFormat);
}

void FilenameTest::testWriterError()
{
    mExportManager->setImage(stereoTestImage());
    QBuffer output;
    QVERIFY(output.open(QIODevice::WriteOnly));
    QSignalSpy errors(mExportManager, &ExportManager::errorMessage);
    QVERIFY(!mExportManager->writeImage(&output, "unsupported-format"));
    QCOMPARE(errors.count(), 1);
}

void FilenameTest::testJpsPluginWriter()
{
    if (!hasStereoPlugins()) {
        QSKIP("KImageFormats has no JPS and MPO plugins");
    }
    mExportManager->m_saveImage = stereoTestImage();

    QBuffer output;
    QVERIFY(output.open(QIODevice::WriteOnly));
    QVERIFY(mExportManager->writeImage(&output, "jps"));
    output.close();

    QByteArray data = output.buffer();
    QBuffer input(&data);
    QVERIFY(input.open(QIODevice::ReadOnly));
    QImageReader reader(&input, "jps");
    const QImage result = reader.read();
    QVERIFY2(!result.isNull(), qPrintable(reader.errorString()));
    QCOMPARE(result.size(), QSize(8, 2));
    verifyEyeColor(result.copy(0, 0, 4, 2), Qt::blue);
    verifyEyeColor(result.copy(4, 0, 4, 2), Qt::red);
}

void FilenameTest::testMpoPluginWriter()
{
    if (!hasStereoPlugins()) {
        QSKIP("KImageFormats has no JPS and MPO plugins");
    }
    mExportManager->m_saveImage = stereoTestImage();

    QBuffer output;
    QVERIFY(output.open(QIODevice::WriteOnly));
    QVERIFY(mExportManager->writeImage(&output, "mpo"));
    output.close();

    QByteArray data = output.buffer();
    QBuffer input(&data);
    QVERIFY(input.open(QIODevice::ReadOnly));
    QImageReader reader(&input, "mpo");
    QCOMPARE(reader.imageCount(), 2);
    const QImage left = reader.read();
    QVERIFY(reader.jumpToImage(1));
    const QImage right = reader.read();
    QVERIFY2(!left.isNull(), qPrintable(reader.errorString()));
    QVERIFY2(!right.isNull(), qPrintable(reader.errorString()));
    verifyEyeColor(left, Qt::red);
    verifyEyeColor(right, Qt::blue);
}

void FilenameTest::testFlatWriter()
{
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(Qt::magenta);
    ImageMetaData::setStereo(image, false);
    mExportManager->m_saveImage = image;

    QBuffer output;
    QVERIFY(output.open(QIODevice::WriteOnly));
    QVERIFY(mExportManager->writeImage(&output, "png"));
    output.close();

    QCOMPARE(QImage::fromData(output.buffer(), "png"), image);
}

QTEST_MAIN(FilenameTest)

#include "FilenameTest.moc"
