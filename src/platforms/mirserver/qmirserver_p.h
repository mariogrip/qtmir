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

#ifndef QMIRSERVER_P_H
#define QMIRSERVER_P_H

// Qt
#include <QThread>
#include <QSharedPointer>

// std
#include <condition_variable>
#include <mutex>

// local
#include "appnotifier.h"
#include "screensmodel.h"
#include "windowcontroller.h"
#include "windowmodelnotifier.h"
#include "workspacecontroller.h"
#include "screenscontroller.h"
#include "wrappedsessionauthorizer.h"
#include "qteventfeeder.h"
#include "qtmir/displayconfigurationpolicy.h"
#include "qtmir/windowmanagementpolicy.h"
#include "qtmir/displayconfigurationstorage.h"

//miral
#include <miral/runner.h>
#include <QDebug>

// miroil
#include <miroil/mirserverhooks.h>
#include <miroil/promptsessionmanager.h>
#include <miroil/openglcontextfactory.h>

class MirServerThread;
class QOpenGLContext;
class QMirServer;

namespace qtmir
{
class SessionAuthorizer;
class PromptSessionListener;
}

class QMirServerPrivate
{
public:
    QMirServerPrivate();
    const std::shared_ptr<ScreensModel> screensModel{new ScreensModel()};
    QSharedPointer<ScreensController> screensController;
    std::shared_ptr<QtEventFeeder> eventFeeder{new QtEventFeeder()};
    MirServerThread *serverThread;

    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const;

    void run(const std::function<void()> &startCallback);
    void stop();

    qtmir::PromptSessionListener *promptSessionListener() const;
    std::shared_ptr<miroil::PromptSessionManager> promptSessionManager() const;

    std::shared_ptr<qtmir::SessionAuthorizer> theApplicationAuthorizer() const;

    qtmir::WindowModelNotifier *windowModelNotifier() const
        { return &m_windowModelNotifier; }

    qtmir::AppNotifier *appNotifier() const
        { return &m_appNotifier; }

    qtmir::WindowControllerInterface *windowController() const
        { return &m_windowController; }

    qtmir::WorkspaceControllerInterface *workspaceController() const
        { return &m_workspaceController; }

    qtmir::DisplayConfigurationPolicyWrapper m_displayConfigurationPolicy;
    qtmir::WindowManagmentPolicyBuilder m_windowManagementPolicy;
    qtmir::BasicSetDisplayConfigurationStorage m_displayConfigurationStorage;
    miral::SetApplicationAuthorizer<WrappedSessionAuthorizer> m_wrappedSessionAuthorizer;
private:
    miroil::MirServerHooks       m_mirServerHooks;
    miroil::OpenGLContextFactory m_openGLContextFactory;  

    miral::MirRunner runner;

    mutable qtmir::AppNotifier m_appNotifier;
    mutable qtmir::WindowModelNotifier m_windowModelNotifier;
    mutable qtmir::WindowController m_windowController;
    mutable qtmir::WorkspaceController m_workspaceController;
};

class MirServerThread : public QThread
{
    Q_OBJECT

public:
    MirServerThread(QMirServerPrivate* server)
        : server(server)
    {}

    bool waitForMirStartup();

Q_SIGNALS:
    void stopped();

public Q_SLOTS:
    void run() override;

private:
    std::mutex mutex;
    std::condition_variable started_cv;
    bool mir_running{false};

    QMirServerPrivate* const server;
};

#endif // QMIRSERVER_P_H
