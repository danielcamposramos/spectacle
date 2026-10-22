/* SPDX-License-Identifier: LGPL-2.0-or-later */

#pragma once

#include <QImage>
#include <QList>

class MonitorCapture
{
public:
    enum Mode {
        AllMonitors2D,
        AllMonitors3D,
        ThisMonitor2D,
        ThisMonitor3D
    };

    QList<QImage> monitors;
    QImage currentMonitor;

    bool supports(Mode mode) const;
    bool hasStereo() const;
    QImage image(Mode mode, bool rightEye) const;
};
