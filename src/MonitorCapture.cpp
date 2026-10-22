/* SPDX-License-Identifier: LGPL-2.0-or-later */

#include "MonitorCapture.h"
#include "ImageMetaData.h"

#include <QPainter>
#include <algorithm>

using namespace Qt::StringLiterals;

static QImage selectedEye(const QImage &source, bool rightEye)
{
    if (!ImageMetaData::isStereo(source)) {
        auto image = source;
        image.setText(u"Stereo3DLayout"_s, u"mono"_s);
        return image;
    }
    const int width = source.width() / 2;
    auto image = source.copy(rightEye ? width : 0, 0, width, source.height());
    ImageMetaData::setStereo(image, false);
    image.setText(u"Stereo3DLayout"_s, u"mono"_s);
    return image;
}

bool MonitorCapture::hasStereo() const
{
    return std::any_of(monitors.cbegin(), monitors.cend(), ImageMetaData::isStereo);
}

bool MonitorCapture::supports(Mode mode) const
{
    switch (mode) {
    case AllMonitors2D:
        return !monitors.empty();
    case AllMonitors3D:
        return hasStereo();
    case ThisMonitor2D:
        return !currentMonitor.isNull();
    case ThisMonitor3D:
        return ImageMetaData::isStereo(currentMonitor);
    }
    return false;
}

QImage MonitorCapture::image(Mode mode, bool rightEye) const
{
    if (!supports(mode)) {
        return {};
    }
    if (mode == ThisMonitor3D) {
        return currentMonitor;
    }
    if (mode == ThisMonitor2D) {
        return selectedEye(currentMonitor, rightEye);
    }

    auto sources = monitors;
    std::stable_sort(sources.begin(), sources.end(), [](const QImage &a, const QImage &b) {
        const auto pa = ImageMetaData::logicalXY(a);
        const auto pb = ImageMetaData::logicalXY(b);
        if (pa.x() != pb.x()) {
            return pa.x() < pb.x();
        }
        if (pa.y() != pb.y()) {
            return pa.y() < pb.y();
        }
        return ImageMetaData::screen(a) < ImageMetaData::screen(b);
    });
    QList<QImage> images;
    QSize size(0, 0);
    for (const auto &source : sources) {
        if (mode == AllMonitors3D && !ImageMetaData::isStereo(source)) {
            continue;
        }
        auto image = mode == AllMonitors2D ? selectedEye(source, rightEye) : source;
        // Each monitor keeps its captured pixels, including when its DPR differs from its neighbours.
        image.setDevicePixelRatio(1);
        size.rwidth() += image.width();
        size.setHeight(std::max(size.height(), image.height()));
        images.append(image);
    }
    QImage result(size, QImage::Format_RGBA8888_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    int x = 0;
    for (const auto &image : images) {
        painter.drawImage(x, 0, image);
        x += image.width();
    }
    painter.end();
    ImageMetaData::setMonitorPanorama(result);
    return result;
}
