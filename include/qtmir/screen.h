/*
 * Copyright (C) 2017 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QTMIR_SCREEN_H
#define QTMIR_SCREEN_H

#include <qtmir/types.h>
#include <miroil/display_id.h>

#include <QObject>
#include <QPointer>

#include <QQmlListProperty>
#include <QSize>
#include <QPoint>

#include <mir_toolkit/common.h>

class QScreen;

namespace qtmir {

class ScreenMode;
class ScreenConfiguration;

class Screen : public QObject
{
    Q_OBJECT
public:
    virtual miroil::DisplayId displayId() const = 0;
    virtual bool used() const = 0;
    virtual QString name() const = 0;
    virtual float scale() const = 0;
    virtual QSizeF physicalSize() const = 0;
    virtual qtmir::FormFactor formFactor() const = 0;
    virtual qtmir::OutputTypes outputType() const = 0;
    virtual MirPowerMode powerMode() const = 0;
    virtual Qt::ScreenOrientation orientation() const = 0;
    virtual QPoint position() const = 0;
    virtual QQmlListProperty<qtmir::ScreenMode> availableModes() = 0;
    virtual uint currentModeIndex() const = 0;
    virtual bool isActive() const = 0;
    virtual void setActive(bool active) = 0;

    virtual QScreen* qscreen() const = 0;

    virtual ScreenConfiguration *beginConfiguration() const = 0;
    virtual bool applyConfiguration(ScreenConfiguration *configuration) = 0;

Q_SIGNALS:
    void usedChanged();
    void nameChanged();
    void outputTypeChanged();
    void scaleChanged();
    void formFactorChanged();
    void powerModeChanged();
    void orientationChanged();
    void positionChanged();
    void currentModeIndexChanged();
    void physicalSizeChanged();
    void availableModesChanged();
    void activeChanged(bool active);

protected:
    Screen(QObject* parent = 0): QObject(parent) {}
};

class ScreenMode : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal refreshRate MEMBER refreshRate CONSTANT)
    Q_PROPERTY(QSize size MEMBER size CONSTANT)
public:
    ScreenMode():refreshRate(-1) {}
    ScreenMode(qreal refreshRate, const QSize& size)
          : refreshRate{refreshRate},size{size}
    {}

    qreal refreshRate;
    QSize size;
};

struct ScreenConfiguration
{
    bool valid{false};
    miroil::OutputId id;

    bool used;
    QPoint topLeft;
    uint32_t currentModeIndex;
    MirPowerMode powerMode;
    MirOrientation orientation;
    float scale;
    qtmir::FormFactor formFactor;
};

typedef QVector<ScreenConfiguration> ScreenConfigurationList;

}

Q_DECLARE_METATYPE(qtmir::Screen*)
Q_DECLARE_METATYPE(qtmir::ScreenMode*)
Q_DECLARE_METATYPE(qtmir::ScreenConfiguration*)

Q_DECLARE_METATYPE(MirPowerMode)

#endif // SCREEN_H
