/*
 * Copyright (C) 2015-2017 Canonical, Ltd.
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

#include "screensmodel.h"

#include "screenplatformwindow.h"
#include "qtcompositor.h"
#include "logging.h"
#include "platformscreen.h"
#include "mirqtconversion.h"
#include "orientationsensor.h"

// Mir
#include <mir/graphics/display.h>
#include <mir/graphics/display_buffer.h>
#include <mir/graphics/display_configuration.h>
#include <mir/compositor/display_listener.h>

// Qt
#include <QScreen>
#include <QGuiApplication> // for qApp
#include <qpa/qwindowsysteminterface.h>

// std
#include <memory>

namespace mg = mir::graphics;

#define DEBUG_MSG qCDebug(QTMIR_SCREENS).nospace() << "ScreensModel[" << this <<"]::" << __func__

ScreensModel::ScreensModel(QObject *parent)
    : MirDisplayConfigurationObserver(parent)
    , m_compositing(false)
    , m_orientationSensor(std::make_shared<OrientationSensor>(this))
{
    DEBUG_MSG << "()";
}

// init only after MirServer has initialized - runs on MirServerThread!!!
void ScreensModel::init(
    const std::shared_ptr<mir::graphics::Display>& display,
    const std::shared_ptr<QtCompositor>& compositor,
    const std::shared_ptr<mir::compositor::DisplayListener>& displayListener)
{
    m_display = display;
    m_compositor = compositor;
    m_displayListener = displayListener;

    // Use a Blocking Queued Connection to enforce synchronization of Qt GUI thread with Mir thread(s)
    // on compositor shutdown. Compositor startup can be lazy.
    // Queued connections work because the thread affinity of this class is with the Qt GUI thread.
    auto qtCompositor = compositor.get();
    connect(qtCompositor, &QtCompositor::starting,
            this, &ScreensModel::onCompositorStarting);
    connect(qtCompositor, &QtCompositor::stopping,
            this, &ScreensModel::onCompositorStopping, Qt::BlockingQueuedConnection);

    connect(this, &MirDisplayConfigurationObserver::configurationApplied,
            this, &ScreensModel::update, Qt::BlockingQueuedConnection);
}

// terminate before shutting down the Mir server, or else liable to deadlock with the blocking connection above
// Runs on MirServerThread!!!
void ScreensModel::terminate()
{
    m_compositor->disconnect();
}

void ScreensModel::onCompositorStarting()
{
    DEBUG_MSG << "()";
    m_compositing = true;

    m_orientationSensor->start();

    update(); // must handle all hardware changes before starting the renderer

    startRenderer();
}

void ScreensModel::onCompositorStopping()
{
    DEBUG_MSG << "()";
    m_compositing = false;

    m_orientationSensor->stop();

    haltRenderer(); // must stop all rendering before handling any hardware changes

    update();
}

void ScreensModel::update()
{
    DEBUG_MSG << "()";
    auto display = m_display.lock();
    if (!display)
        return;
    auto displayConfig = display->configuration();

    // Mir only tells us something changed, it is up to us to figure out what.
    QList<PlatformScreen*> newScreenList;
    QList<PlatformScreen*> oldScreenList = m_screenList;
    QHash<ScreenPlatformWindow*, PlatformScreen*> windowMoveList;
    m_screenList.clear();

    displayConfig->for_each_output(
        [this, &oldScreenList, &newScreenList, &windowMoveList](const mg::DisplayConfigurationOutput &output) {
            if (output.connected) {
                PlatformScreen *screen = findScreenWithId(oldScreenList, output.id);
                if (screen) { // we've already set up this display before

                    // Can we re-use the existing Screen?
                    if (canUpdateExistingScreen(screen, output)) {
                        qCDebug(QTMIR_SCREENS) << "Can reuse Screen with id" << output.id.as_value();
                        screen->setMirDisplayConfiguration(output);
                        oldScreenList.removeAll(screen);
                        m_screenList.append(screen);
                    } else {
                        // no, need to delete it and re-create with new config
                        DEBUG_MSG << "() - recreating screen for output=" << output.id.as_value();

                        auto newScreen = createScreen(output);
                        newScreenList.append(newScreen);

                        // if Window on this Screen, arrange to move it to the new Screen
                        Q_FOREACH (ScreenPlatformWindow* window, screen->windows()) {
                            windowMoveList.insert(window, newScreen);
                        }
                        m_screenList.append(newScreen);
                        // Announce to mir
                        m_displayListener->add_display(qtmir::toMirRectangle(newScreen->geometry()));
                    }
                } else {
                    DEBUG_MSG << "() - new screen for output=" << output.id.as_value();
                    // new display, so create Screen for it
                    screen = createScreen(output);
                    newScreenList.append(screen);
                    m_screenList.append(screen);
                }

                if (!output.used) {
                    Q_FOREACH (ScreenPlatformWindow* window, screen->windows()) {
                        if (window->window() && window->isExposed()) {
                            window->window()->hide();
                        }
                    }
                }
            } else {
                qCDebug(QTMIR_SCREENS) << "Output with ID" << output.id.as_value() << "is not used and connected.";
            }
        }
    );

    // Match up the new Mir DisplayBuffers with each Screen
    // Do this first so that the buffers are valid before we announce to Qt.
    display->for_each_display_sync_group([&](mg::DisplaySyncGroup &group) {
        group.for_each_display_buffer([&](mg::DisplayBuffer &buffer) {
            // only way to match Screen to a DisplayBuffer is by matching the geometry
            QRect dbGeom(buffer.view_area().top_left.x.as_int(),
                         buffer.view_area().top_left.y.as_int(),
                         buffer.view_area().size.width.as_int(),
                         buffer.view_area().size.height.as_int());

            Q_FOREACH (auto screen, m_screenList) {
                if (dbGeom == screen->geometry()) {
                    screen->setMirDisplayBuffer(&buffer, &group);
                    break;
                }
            }
        });
    });

    // Announce new Screens to Qt
    Q_FOREACH (auto screen, newScreenList) {
        Q_EMIT screenAdded(screen);
    }

    // Move Windows from about-to-be-deleted Screens to new Screen
    auto i = windowMoveList.constBegin();
    while (i != windowMoveList.constEnd()) {
        DEBUG_MSG << "() - moving platform window " << (void*) i.key() <<  " from " << static_cast<PlatformScreen*>(i.key()->screen()) << " to " << i.value();
        i.key()->setScreen(i.value());
        i++;
    }

    // Delete any old & unused Screens
    Q_FOREACH (auto screen, oldScreenList) {
        DEBUG_MSG << "() - removed Screen with id " << screen->displayId().output_id.as_value()
                               << " and geometry " << screen->geometry();

        Q_FOREACH (ScreenPlatformWindow* window, screen->windows()) {
            if (window->window() && window->isExposed()) {
                window->window()->hide();
            }
        }
        bool ok = QMetaObject::invokeMethod(qApp, "screenAboutToBeRemoved", Qt::DirectConnection, Q_ARG(QScreen*, screen->screen()));
        if (!ok) {
            DEBUG_MSG << "() - Failed to invoke QGuiApplication::onScreenAboutToBeRemoved(QScreen*) slot.";
        }
        m_displayListener->remove_display(qtmir::toMirRectangle(screen->geometry()));
        Q_EMIT screenRemoved(screen); // should delete the backing Screen
    }

    qCDebug(QTMIR_SCREENS) << "=======================================";
    Q_FOREACH (auto screen, m_screenList) {
        qCDebug(QTMIR_SCREENS) << screen << "- id:" << screen->displayId().output_id.as_value()
                               << "geometry:" << screen->geometry()
                               << "window:" << (void*) screen->primaryWindow()
                               << "type:" << screen->name()
                               << "scale:" << screen->scale();
    }
    qCDebug(QTMIR_SCREENS) << "=======================================";
}

bool ScreensModel::canUpdateExistingScreen(const PlatformScreen *screen, const mg::DisplayConfigurationOutput &output)
{
    // Compare the properties of the existing Screen with its new configuration. Properties
    // like geometry, refresh rate and dpi can be updated on existing screens. Other property
    // changes cannot be applied to existing screen, so will need to delete existing Screen and
    // create new Screen with new properties.
    bool canUpdateExisting = true;

    if (!qFuzzyCompare(screen->scale(), output.scale)) {
        canUpdateExisting = true; //false; FIXME
    }

    return canUpdateExisting;
}

/*
 * ScreensModel::startRenderer()
 * (Re)Start Qt's render thread by setting all windows with a corresponding screen to exposed.
 * It is asynchronous, it returns before the render thread(s) have started.
 */
void ScreensModel::startRenderer()
{
    Q_FOREACH (const auto screen, m_screenList) {
        // Only set windows exposed on displays which are used and turned on, as the GL context Mir provided
        // is invalid in that situation
        if (screen->used() && screen->powerMode() == mir_power_mode_on) {
            Q_FOREACH (ScreenPlatformWindow* window, screen->windows()) {
                window->setExposed(true);
            }
        }
    }
}

/*
 * ScreensModel::haltRenderer()
 * Stop Qt's render thread(s) by setting all windows with a corresponding screen to not exposed.
 * It is blocking, it returns after the render thread(s) have all stopped.
 */
void ScreensModel::haltRenderer()
{
    Q_FOREACH (const auto screen, m_screenList) {

        Q_FOREACH (ScreenPlatformWindow* window, screen->windows()) {
            window->setExposed(false);
        }
    }
}

PlatformScreen* ScreensModel::createScreen(const mg::DisplayConfigurationOutput &output) const
{
    return new PlatformScreen(output, m_orientationSensor);
}

PlatformScreen* ScreensModel::findScreenWithId(const QList<PlatformScreen *> &list, const mg::DisplayConfigurationOutputId id)
{
    Q_FOREACH (auto screen, list) {
        if (screen->displayId().output_id == id) {
            return screen;
        }
    }
    return nullptr;
}
