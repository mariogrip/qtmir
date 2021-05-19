/*
 * Copyright (C) 2015,2017 Canonical, Ltd.
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

#include "qmirserver_p.h"
#include "qtmir/miral/display_configuration_storage.h"

// local
#include "logging.h"
#include "wrappedwindowmanagementpolicy.h"
#include "inputdeviceobserver.h"
#include "qteventfeeder.h"
#include "qtmir/sessionauthorizer.h"
#include "miropenglcontext.h"
#include "screenscontroller.h"
#include "mirglconfig.h"
#include "qtcompositor.h"

#include <miroil/prompt_session_manager.h>
#include <miroil/persist_display_config.h>
#include <miroil/setcompositor.h>
#include <miroil/display_listener_wrapper.h>

// miral
#include <miral/add_init_callback.h>
#include <miral/set_terminator.h>

// Qt
#include <QCoreApplication>
#include <QOpenGLContext>

#ifdef WITH_VALGRIND
#include <valgrind.h>
#endif

namespace
{
static int qtmirArgc{1};
static const char *qtmirArgv[]{"qtmir"};

class DefaultWindowManagementPolicy : public qtmir::WindowManagementPolicy
{
public:
    DefaultWindowManagementPolicy(const miral::WindowManagerTools &tools, std::shared_ptr<qtmir::WindowManagementPolicyPrivate> dd)
        : qtmir::WindowManagementPolicy(tools, dd)
    {}
};

struct DefaultDisplayConfigurationStorage : miral::DisplayConfigurationStorage
{
    void save(const miral::DisplayId&, const miral::DisplayConfigurationOptions&) override {}

    bool load(const miral::DisplayId&, miral::DisplayConfigurationOptions&) const override { return false; }
};

std::shared_ptr<miral::DisplayConfigurationPolicy> buildDisplayConfigurationPolicy()
{
    return std::make_shared<qtmir::DisplayConfigurationPolicy>();
}

std::shared_ptr<miral::DisplayConfigurationStorage> buildDisplayConfigurationStorage()
{
    return std::make_shared<DefaultDisplayConfigurationStorage>();
}

std::shared_ptr<qtmir::WindowManagementPolicy> buildWindowManagementPolicy(const miral::WindowManagerTools &tools,
                                                                           std::shared_ptr<qtmir::WindowManagementPolicyPrivate> dd)
{
    return std::make_shared<DefaultWindowManagementPolicy>(tools, dd);
}

std::shared_ptr<qtmir::SessionAuthorizer> buildSessionAuthorizer()
{
    return std::make_shared<qtmir::SessionAuthorizer>();
}

} // namespace

void MirServerThread::run()
{
    auto start_callback = [this]
    {
        std::lock_guard<std::mutex> lock(mutex);
        mir_running = true;
        started_cv.notify_one();
    };

    server->run(start_callback);

    Q_EMIT stopped();
}

bool MirServerThread::waitForMirStartup()
{
#ifdef WITH_VALGRIND
    const int timeout = RUNNING_ON_VALGRIND ? 100 : 10; // else timeout triggers before Mir ready
#else
    const int timeout = 10;
#endif

    std::unique_lock<decltype(mutex)> lock(mutex);
    started_cv.wait_for(lock, std::chrono::seconds{timeout}, [&]{ return mir_running; });
    return mir_running;
}

QPlatformOpenGLContext *QMirServerPrivate::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    QSurfaceFormat           format     = context->format();
    mir::graphics::Display * mirDisplay = m_mirServerHooks.theMirDisplay().get();
    
    return m_openGLContextFactory.createPlatformOpenGLContext(
        [format, mirDisplay](mir::graphics::GLConfig &gl_config)
        -> QPlatformOpenGLContext *
        {
            return new MirOpenGLContext(*mirDisplay, gl_config, format);
        }
    );
    
}

std::shared_ptr<miroil::PromptSessionManager> QMirServerPrivate::promptSessionManager() const
{
    return std::make_shared<miroil::PromptSessionManager>(m_mirServerHooks.thePromptSessionManager());
}

std::shared_ptr<qtmir::SessionAuthorizer> QMirServerPrivate::theApplicationAuthorizer() const
{
    auto wrapped = m_wrappedSessionAuthorizer.the_custom_application_authorizer();
    return wrapped ? wrapped->wrapper() : nullptr;
}

QMirServerPrivate::QMirServerPrivate()
    : m_displayConfigurationPolicy(buildDisplayConfigurationPolicy)
    , m_windowManagementPolicy(buildWindowManagementPolicy)
    , m_displayConfigurationStorage(buildDisplayConfigurationStorage)
    , m_wrappedSessionAuthorizer(buildSessionAuthorizer)
    , m_openGLContextFactory(new MirGLConfig())
    , runner(qtmirArgc, qtmirArgv)
{
}

qtmir::PromptSessionListener *QMirServerPrivate::promptSessionListener() const
{
    return dynamic_cast<qtmir::PromptSessionListener*>(m_mirServerHooks.promptSessionListener());
}

void QMirServerPrivate::run(const std::function<void()> &startCallback)
{
    m_mirServerHooks.createPromptSessionListener(std::dynamic_pointer_cast<miroil::PromptSessionListener>(std::make_shared<qtmir::PromptSessionListener>()));

    miral::AddInitCallback addInitCallback{[&, this]
    {
        qCDebug(QTMIR_MIR_MESSAGES) << "MirServer created";
        qCDebug(QTMIR_MIR_MESSAGES) << "Command line arguments passed to Qt:" << QCoreApplication::arguments();
    }};

    miral::SetTerminator setTerminator{[](int)
    {
        qDebug() << "Signal caught by Mir, stopping Mir server..";
        QCoreApplication::quit();
    }};

    runner.set_exception_handler([this]
    {
        try {
            throw;
        } catch (const std::exception &ex) {
            qCritical() << ex.what();
            exit(1);
        }
    });

    runner.add_start_callback([&]
    {
        screensController = QSharedPointer<ScreensController>(new ScreensController(screensModel, m_mirServerHooks.theMirDisplay(), m_mirServerHooks.theDisplayConfigurationController()));
        std::shared_ptr<miroil::InputDeviceObserver> ptr = std::make_shared<qtmir::MirInputDeviceObserver>();
        m_mirServerHooks.createInputDeviceObserver(ptr);
    });

    runner.add_start_callback(startCallback);

    runner.add_stop_callback([&]
    {
        screensModel->terminate();
        screensController.clear();
    });

    auto displayStorageBuilder = m_displayConfigurationStorage.builder();

    runner.run_with(
        {
            m_wrappedSessionAuthorizer,
            m_openGLContextFactory,
            m_mirServerHooks,
            miral::set_window_management_policy<WrappedWindowManagementPolicy>(m_windowModelNotifier,
                                                                               m_windowController,
                                                                               m_workspaceController,
                                                                               m_appNotifier,
                                                                               screensModel,
                                                                               eventFeeder,
                                                                               m_windowManagementPolicy),
            addInitCallback,
            miroil::SetCompositor(
                // Create the the QtCompositor 
                    [this]()
                    -> std::shared_ptr<miroil::Compositor>
                {
                    std::shared_ptr<miroil::Compositor> result = std::make_shared<QtCompositor>();
                    return result;
                }
                ,
                // Initialization called by mir when the new compositor is setup up              
                    [this](const std::shared_ptr<mir::graphics::Display>& display,
                           const std::shared_ptr<miroil::Compositor> & compositor,
                           const std::shared_ptr<mir::compositor::DisplayListener>& displayListener)
                {
                    
                    std::shared_ptr<QtCompositor> qtCompsitor = std::dynamic_pointer_cast<QtCompositor>(compositor);
                    
                    this->screensModel->init(display, qtCompsitor, std::make_shared<miroil::DisplayListenerWrapper>(displayListener));
                }
            ),
            setTerminator,
            miroil::PersistDisplayConfig{displayStorageBuilder(),
                                        m_displayConfigurationPolicy}
        });
}

void QMirServerPrivate::stop()
{
    runner.stop();
}
