/*
 * Copyright (C) 2016-2017 Canonical, Ltd.
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

#include "wrappedwindowmanagementpolicy.h"

#include <miroil/eventdispatch.h>
#include <miral/toolkit_event.h>

#include "initialsurfacesizes.h"
#include "screensmodel.h"
#include "surfaceobserver.h"

#include "miral/window_manager_tools.h"
#include "miral/window_specification.h"

#include "mirqtconversion.h"
#include "qmirserver.h"
#include "tracepoints.h"

#include <QDebug>

using namespace mir::geometry;

namespace qtmir
{
    std::shared_ptr<ExtraWindowInfo> getExtraInfo(const miral::WindowInfo &windowInfo) {
        return std::static_pointer_cast<ExtraWindowInfo>(windowInfo.userdata());
    }

    struct BasicSetWindowManagementPolicy::Private
    {
        Private(WindowManagmentPolicyBuilder const& builder) :
            builder{builder} {}

        WindowManagmentPolicyBuilder builder;
    };

    BasicSetWindowManagementPolicy::BasicSetWindowManagementPolicy(WindowManagmentPolicyBuilder const& builder)
        : d(new BasicSetWindowManagementPolicy::Private(builder))
    {
    }

    void BasicSetWindowManagementPolicy::operator()(QMirServer &server)
    {
        server.overrideWindowManagementPolicy(d->builder);
    }

    WindowManagmentPolicyBuilder BasicSetWindowManagementPolicy::builder() const
    {
        return d->builder;
    }

    struct WindowManagementPolicyPrivate
    {
        WindowManagementPolicyPrivate(qtmir::WindowModelNotifier &windowModel,
                                      qtmir::AppNotifier &appNotifier,
                                      const std::shared_ptr<QtEventFeeder>& eventFeeder,
                                      const std::shared_ptr<ScreensModel> screensModel)
            : m_windowModel(windowModel)
            , m_appNotifier(appNotifier)
            , m_eventFeeder(eventFeeder)
            , m_screensModel(screensModel)
        {}

        QRect getConfinementRect(const QRect rect) const
        {
            QRect confinementRect;
            for (const QRect r : m_confinementRegions) {
                if (r.intersects(rect)) {
                    confinementRect = r;
                    // TODO: What if there are multiple confinement regions and they intersect??
                    break;
                }
            }
            return confinementRect;
        }

        qtmir::WindowModelNotifier &m_windowModel;
        qtmir::AppNotifier &m_appNotifier;
        const std::shared_ptr<QtEventFeeder> m_eventFeeder;
        const std::shared_ptr<ScreensModel> m_screensModel;

        QVector<QRect> m_confinementRegions;
        QMargins m_windowMargins[mir_window_types];
    };

    WindowManagementPolicy::WindowManagementPolicy(const miral::WindowManagerTools &tools, std::shared_ptr<qtmir::WindowManagementPolicyPrivate> dd)
        : miral::CanonicalWindowManagerPolicy(tools)
        , d(dd)
    {
    }

    miral::WindowSpecification WindowManagementPolicy::place_new_window(
        const miral::ApplicationInfo &appInfo,
        const miral::WindowSpecification &requestParameters)
    {
        auto parameters = CanonicalWindowManagerPolicy::place_new_window(appInfo, requestParameters);

        if (!requestParameters.parent().is_set() || requestParameters.parent().value().lock().get() == nullptr) {

            int surfaceType = requestParameters.type().is_set() ? requestParameters.type().value() : -1;

            QSize initialSize = InitialSurfaceSizes::get(miral::pid_of(appInfo.application()));

            if (initialSize.isValid() && surfaceType == mir_window_type_normal) {
                parameters.size() = toMirSize(initialSize);
            }
        }

        parameters.userdata() = std::make_shared<ExtraWindowInfo>();

        return parameters;
    }

    void WindowManagementPolicy::handle_window_ready(miral::WindowInfo &windowInfo)
    {
        Q_EMIT d->m_windowModel.windowReady(windowInfo);

        auto appInfo = tools.info_for(windowInfo.window().application());
        Q_EMIT d->m_appNotifier.appCreatedWindow(appInfo);
    }

    void WindowManagementPolicy::handle_modify_window(miral::WindowInfo &windowInfo,
                                                      const miral::WindowSpecification &modificationsClient)
    {
        miral::WindowSpecification modifications(modificationsClient);

        if (modifications.size().is_set()) {
            auto extraWindowInfo = qtmir::getExtraInfo(windowInfo);
            QMutexLocker locker(&extraWindowInfo->mutex);
            if (!extraWindowInfo->allowClientResize) {
                modifications.size().consume();
            }
        }

        CanonicalWindowManagerPolicy::handle_modify_window(windowInfo, modifications);

        // TODO Once Qt processes the request we probably don't want to notify from here
        std::shared_ptr<mir::scene::Surface> surface{windowInfo.window()};
        if (SurfaceObserver *observer = SurfaceObserver::observerForSurface(surface.get())) {
            observer->notifySurfaceModifications(modifications);
        }
    }

    void WindowManagementPolicy::handle_raise_window(miral::WindowInfo &windowInfo)
    {
        CanonicalWindowManagerPolicy::handle_raise_window(windowInfo);

        Q_EMIT d->m_windowModel.windowRequestedRaise(windowInfo);
    }

    Rectangle WindowManagementPolicy::confirm_placement_on_display(const miral::WindowInfo &/*window_info*/,
                                                                   MirWindowState /*new_state*/,
                                                                   const Rectangle &new_placement)
    {
        return new_placement;
    }

    bool WindowManagementPolicy::handle_keyboard_event(const MirKeyboardEvent *event)
    {
        d->m_eventFeeder->dispatchKey(event);
        return true;
    }

    bool WindowManagementPolicy::handle_touch_event(const MirTouchEvent *event)
    {
        d->m_eventFeeder->dispatchTouch(event);
        return true;
    }

    bool WindowManagementPolicy::handle_pointer_event(const MirPointerEvent *event)
    {
        d->m_eventFeeder->dispatchPointer(event);
        return true;
    }

    void WindowManagementPolicy::advise_begin()
    {
        Q_EMIT d->m_windowModel.modificationsStarted();
    }

    void WindowManagementPolicy::advise_end()
    {
        Q_EMIT d->m_windowModel.modificationsEnded();
    }

    void WindowManagementPolicy::advise_new_app(miral::ApplicationInfo &application)
    {
        Q_EMIT d->m_appNotifier.appAdded(application);
    }

    void WindowManagementPolicy::advise_delete_app(const miral::ApplicationInfo &application)
    {
        Q_EMIT d->m_appNotifier.appRemoved(application);
    }

    void WindowManagementPolicy::advise_new_window(const miral::WindowInfo &windowInfo)
    {
        // TODO: attach surface observer here

        getExtraInfo(windowInfo)->persistentId = QString::fromStdString(tools.id_for_window(windowInfo.window()));

        // FIXME: remove when possible
        getExtraInfo(windowInfo)->state = toQtState(windowInfo.state());

        Q_EMIT d->m_windowModel.windowAdded(NewWindow{windowInfo});
    }

    void WindowManagementPolicy::advise_focus_lost(const miral::WindowInfo &windowInfo)
    {
        Q_EMIT d->m_windowModel.windowFocusChanged(windowInfo, false);
    }

    void WindowManagementPolicy::advise_focus_gained(const miral::WindowInfo &windowInfo)
    {
        // update Qt model ASAP, before applying Mir policy
        Q_EMIT d->m_windowModel.windowFocusChanged(windowInfo, true);

        CanonicalWindowManagerPolicy::advise_focus_gained(windowInfo);
    }

    void WindowManagementPolicy::advise_state_change(const miral::WindowInfo &windowInfo, MirWindowState state)
    {
        auto extraWinInfo = getExtraInfo(windowInfo);

        // FIXME: Remove this mess once MirWindowState matches Mir::State
        if (state == mir_window_state_restored && extraWinInfo->state != Mir::RestoredState
                && toMirState(extraWinInfo->state) == state) {
            // Ignore. That MirWindowState is just a placeholder for a Mir::State value that has no counterpart
            // in MirWindowState.
        } else {
            extraWinInfo->state = toQtState(state);
        }

        Q_EMIT d->m_windowModel.windowStateChanged(windowInfo, extraWinInfo->state);
    }

    void WindowManagementPolicy::advise_move_to(const miral::WindowInfo &windowInfo, Point topLeft)
    {
        Q_EMIT d->m_windowModel.windowMoved(windowInfo, toQPoint(topLeft));
    }

    void WindowManagementPolicy::advise_resize(const miral::WindowInfo &windowInfo, const Size &newSize)
    {
        Q_EMIT d->m_windowModel.windowResized(windowInfo, toQSize(newSize));
    }

    void WindowManagementPolicy::advise_delete_window(const miral::WindowInfo &windowInfo)
    {
        Q_EMIT d->m_windowModel.windowRemoved(windowInfo);
    }

    void WindowManagementPolicy::advise_raise(const std::vector<miral::Window> &windows)
    {
        Q_EMIT d->m_windowModel.windowsRaised(windows);
    }

    void WindowManagementPolicy::advise_adding_to_workspace(std::shared_ptr<miral::Workspace> const& workspace,
                                                            std::vector<miral::Window> const& windows)
    {
        miral::CanonicalWindowManagerPolicy::advise_adding_to_workspace(workspace, windows);

        Q_EMIT d->m_windowModel.windowsAddedToWorkspace(workspace, windows);
    }

    void WindowManagementPolicy::advise_removing_from_workspace(std::shared_ptr<miral::Workspace> const& workspace,
                                                                std::vector<miral::Window> const& windows)
    {
        miral::CanonicalWindowManagerPolicy::advise_removing_from_workspace(workspace, windows);

        Q_EMIT d->m_windowModel.windowsAboutToBeRemovedFromWorkspace(workspace, windows);
    }

    void WindowManagementPolicy::advise_output_create(miral::Output const& output)
    {
        Q_UNUSED(output);
        Q_EMIT d->m_screensModel->update();
    }

    void WindowManagementPolicy::advise_output_update(miral::Output const& updated, miral::Output const& original)
    {
        Q_UNUSED(updated);
        Q_UNUSED(original);
        Q_EMIT d->m_screensModel->update();
    }

    void WindowManagementPolicy::advise_output_delete(miral::Output const& output)
    {
        Q_UNUSED(output);
        Q_EMIT d->m_screensModel->update();
    }

    Rectangle WindowManagementPolicy::confirm_inherited_move(miral::WindowInfo const& windowInfo, Displacement movement)
    {
        if (d->m_confinementRegions.isEmpty()) {
            return CanonicalWindowManagerPolicy::confirm_inherited_move(windowInfo, movement);
        }

        auto window = windowInfo.window();
        const QMargins windowMargins = d->m_windowMargins[windowInfo.type()];
        QRect windowGeom(toQPoint(window.top_left()), toQSize(window.size()));

        QRect geom = windowGeom.marginsAdded(windowMargins);
        const QRect confinementRect = d->getConfinementRect(geom);

        int x = geom.x();
        int y = geom.y();
        int moveX = movement.dx.as_int();
        int moveY = movement.dy.as_int();

        // If the child window is already partially beyond the available desktop area (most likely because the user
        // explicitly moved it there) we won't pull it back, unless the inherited movement is this direction, but also won't
        // push it even further astray. But if it currently is completely within the available desktop area boundaries
        // we won't let go beyond it.

        if (moveX > 0) {
            if (geom.right() < confinementRect.right()) {
                x = qMin(x + moveX, confinementRect.right() + 1 - geom.width()); // +1 because QRect::right() weird
            }
        } else {
            if (geom.x() > confinementRect.left()) {
                x = qMax(x + moveX, confinementRect.left());
            }
        }

        if (moveY > 0) {
            if (geom.bottom() < confinementRect.bottom()) {
                y = qMin(y + moveY, confinementRect.bottom() + 1 - geom.height()); // +1 because QRect::bottom() weird
            }
        } else {
            if (geom.y() > confinementRect.top()) {
                y = qMax(y + moveY, confinementRect.top());
            }
        }

        geom.moveTo(x, y);
        return toMirRectangle(geom.marginsRemoved(windowMargins));
    }

    void WindowManagementPolicy::handle_request_drag_and_drop(miral::WindowInfo &/*window_info*/)
    {

    }

    void WindowManagementPolicy::handle_request_move(miral::WindowInfo &/*window_info*/,
                                                    const MirInputEvent */*input_event*/)
    {

    }

    void WindowManagementPolicy::handle_request_resize(miral::WindowInfo &/*window_info*/,
                                                    const MirInputEvent */*input_event*/, MirResizeEdge /*edge*/)
    {

    }
}

WrappedWindowManagementPolicy::WrappedWindowManagementPolicy(const miral::WindowManagerTools &tools,
                                               qtmir::WindowModelNotifier &windowModel,
                                               qtmir::WindowController &windowController,
                                               qtmir::WorkspaceController &workspaceController,
                                               qtmir::AppNotifier &appNotifier,
                                               const std::shared_ptr<ScreensModel> screensModel,
                                               const std::shared_ptr<QtEventFeeder> &eventFeeder,
                                               const qtmir::WindowManagmentPolicyBuilder &wmBuilder)
    : qtmir::WindowManagementPolicy(tools, std::make_shared<qtmir::WindowManagementPolicyPrivate>(windowModel,
                                                                                                  appNotifier,
                                                                                                  eventFeeder,
                                                                                                  screensModel))
    , m_wrapper(wmBuilder(tools, d))
{
    qRegisterMetaType<qtmir::NewWindow>();
    qRegisterMetaType<std::vector<miral::Window>>();
    qRegisterMetaType<miral::ApplicationInfo>();
    windowController.setPolicy(this);
    workspaceController.setPolicy(this);
}

/* Following are hooks to allow custom policy be imposed */
miral::WindowSpecification WrappedWindowManagementPolicy::place_new_window(
    const miral::ApplicationInfo &appInfo,
    const miral::WindowSpecification &requestParameters)
{
    return m_wrapper->place_new_window(appInfo, requestParameters);
}

void WrappedWindowManagementPolicy::handle_window_ready(miral::WindowInfo &windowInfo)
{
    m_wrapper->handle_window_ready(windowInfo);
}

void WrappedWindowManagementPolicy::handle_modify_window(
    miral::WindowInfo &windowInfo,
    const miral::WindowSpecification &modificationsClient)
{
    m_wrapper->handle_modify_window(windowInfo, modificationsClient);
}

void WrappedWindowManagementPolicy::handle_raise_window(miral::WindowInfo &windowInfo)
{
    m_wrapper->handle_raise_window(windowInfo);
}

Rectangle WrappedWindowManagementPolicy::confirm_placement_on_display(const miral::WindowInfo &window_info,
                                                                MirWindowState new_state,
                                                                const Rectangle &new_placement)
{
    return m_wrapper->confirm_placement_on_display(window_info, new_state, new_placement);
}

/* Handle input events - here just inject them into Qt event loop for later processing */
bool WrappedWindowManagementPolicy::handle_keyboard_event(const MirKeyboardEvent *event)
{
    return m_wrapper->handle_keyboard_event(event);
}

bool WrappedWindowManagementPolicy::handle_touch_event(const MirTouchEvent *event)
{
    return m_wrapper->handle_touch_event(event);
}

bool WrappedWindowManagementPolicy::handle_pointer_event(const MirPointerEvent *event)
{
    return m_wrapper->handle_pointer_event(event);
}

void WrappedWindowManagementPolicy::advise_begin()
{
    m_wrapper->advise_begin();
}

void WrappedWindowManagementPolicy::advise_end()
{
    m_wrapper->advise_end();
}

void WrappedWindowManagementPolicy::advise_new_window(const miral::WindowInfo &windowInfo)
{
    m_wrapper->advise_new_window(windowInfo);
}

void WrappedWindowManagementPolicy::advise_delete_window(const miral::WindowInfo &windowInfo)
{
    m_wrapper->advise_delete_window(windowInfo);
}

void WrappedWindowManagementPolicy::advise_raise(const std::vector<miral::Window> &windows)
{
    m_wrapper->advise_raise(windows);
}

Rectangle WrappedWindowManagementPolicy::confirm_inherited_move(const miral::WindowInfo &windowInfo, Displacement movement)
{
    return m_wrapper->confirm_inherited_move(windowInfo, movement);
}

void WrappedWindowManagementPolicy::advise_new_app(miral::ApplicationInfo &application)
{
    m_wrapper->advise_new_app(application);
}

void WrappedWindowManagementPolicy::advise_delete_app(const miral::ApplicationInfo &application)
{
    m_wrapper->advise_delete_app(application);
}

void WrappedWindowManagementPolicy::advise_focus_lost(const miral::WindowInfo &info)
{
    m_wrapper->advise_focus_lost(info);
}

void WrappedWindowManagementPolicy::advise_focus_gained(const miral::WindowInfo &info)
{
    m_wrapper->advise_focus_gained(info);
}

void WrappedWindowManagementPolicy::advise_state_change(const miral::WindowInfo &info, MirWindowState state)
{
    m_wrapper->advise_state_change(info, state);
}

void WrappedWindowManagementPolicy::advise_move_to(const miral::WindowInfo &windowInfo, Point topLeft)
{
    m_wrapper->advise_move_to(windowInfo, topLeft);
}

void WrappedWindowManagementPolicy::advise_resize(const miral::WindowInfo &info, const Size &newSize)
{
    m_wrapper->advise_resize(info, newSize);
}

void WrappedWindowManagementPolicy::deliver_keyboard_event(const MirKeyboardEvent *event, const miral::Window &window)
{
    if (miral::toolkit::mir_keyboard_event_action(event) == mir_keyboard_action_down) {
        tools.invoke_under_lock([&]() {
            if (tools.active_window() != window) {
                tools.select_active_window(window);
            }
        });
    }

    miroil::dispatch_input_event(window, miral::toolkit::mir_keyboard_event_input_event(event));
}

void WrappedWindowManagementPolicy::deliver_touch_event(const MirTouchEvent *event, const miral::Window &window)
{
    tools.invoke_under_lock([&]() {
        if (tools.active_window() != window) {
            tools.select_active_window(window);
        }
    });

    miroil::dispatch_input_event(window, miral::toolkit::mir_touch_event_input_event(event));
}

void WrappedWindowManagementPolicy::deliver_pointer_event(const MirPointerEvent *event, const miral::Window &window)
{
    // Prevent mouse hover events causing window focus to change
    if (miral::toolkit::mir_pointer_event_action(event) == mir_pointer_action_button_down) {
        tools.invoke_under_lock([&]() {
            if (tools.active_window() != window) {
                tools.select_active_window(window);
            }
        });
    }

    miroil::dispatch_input_event(window, miral::toolkit::mir_pointer_event_input_event(event));
}

void WrappedWindowManagementPolicy::advise_adding_to_workspace(const std::shared_ptr<miral::Workspace> &workspace,
                                                               const std::vector<miral::Window> &windows)
{
    m_wrapper->advise_adding_to_workspace(workspace, windows);
}

void WrappedWindowManagementPolicy::advise_removing_from_workspace(const std::shared_ptr<miral::Workspace> &workspace,
                                                                   const std::vector<miral::Window> &windows)
{
    m_wrapper->advise_removing_from_workspace(workspace, windows);
}


void WrappedWindowManagementPolicy::advise_output_create(miral::Output const& output)
{
    m_wrapper->advise_output_create(output);
}

void WrappedWindowManagementPolicy::advise_output_update(miral::Output const& updated, miral::Output const& original)
{
    m_wrapper->advise_output_update(updated, original);
}

void WrappedWindowManagementPolicy::advise_output_delete(miral::Output const& output)
{
    m_wrapper->advise_output_delete(output);
}

void WrappedWindowManagementPolicy::activate(const miral::Window &window)
{
    try {
        if (window) {
            auto &windowInfo = tools.info_for(window);

            // restore from minimized if needed
            if (windowInfo.state() == mir_window_state_minimized) {
                auto extraInfo = qtmir::getExtraInfo(windowInfo);
                Q_ASSERT(extraInfo->previousState != Mir::MinimizedState);
                requestState(window, extraInfo->previousState);
            }

            tools.invoke_under_lock([&]() {
                tools.select_active_window(window);
            });
        }
    } catch (const std::out_of_range&) {
        // usually shell trying to operate on a window which already closed, just ignore
        // (throws from tools.info_for(...) ususally)
        // TODO: Same issue as resize below
    }
}

void WrappedWindowManagementPolicy::resize(const miral::Window &window, const Size size)
{
    miral::WindowSpecification modifications;
    modifications.size() = size;
    tools.invoke_under_lock([&window, &modifications, this]() {
        try {
            tools.modify_window(tools.info_for(window), modifications);
        } catch (const std::out_of_range&) {
            // usually shell trying to operate on a window which already closed, just ignore
            // TODO: MirSurface extends the miral::Window lifetime by holding a shared pointer to
            // the mir::scene::Surface, meaning it cannot detect when the window has been closed
            // and thus avoid calling this method.
        }
    });
}

void WrappedWindowManagementPolicy::move(const miral::Window &window, const Point topLeft)
{
    miral::WindowSpecification modifications;
    modifications.top_left() = topLeft;
    tools.invoke_under_lock([&window, &modifications, this]() {
        try {
            tools.modify_window(tools.info_for(window), modifications);
        } catch (const std::out_of_range&) {
            // usually shell trying to operate on a window which already closed, just ignore
            // TODO: see above comment in resize, same issue
        }
    });
}

void WrappedWindowManagementPolicy::raise(const miral::Window &window)
{
    tools.invoke_under_lock([&window, this]() {
        tools.raise_tree(window);
    });
}

void WrappedWindowManagementPolicy::requestState(const miral::Window &window, const Mir::State state)
{
    auto &windowInfo = tools.info_for(window);
    auto extraWinInfo = qtmir::getExtraInfo(windowInfo);

    if (extraWinInfo->state == state)
        return;

    miral::WindowSpecification modifications;
    modifications.state() = qtmir::toMirState(state);

    // TODO: What if the window modification fails? Is that possible?
    //       Assuming here that the state will indeed change
    extraWinInfo->previousState = extraWinInfo->state;
    extraWinInfo->state = state;

    if (modifications.state() == windowInfo.state()) {
        Q_EMIT d->m_windowModel.windowStateChanged(windowInfo, state);
    } else {
        tools.invoke_under_lock([&]() {
            tools.modify_window(windowInfo, modifications);
        });
    }
}

void WrappedWindowManagementPolicy::ask_client_to_close(const miral::Window &window)
{
    tools.invoke_under_lock([&window, this]() {
        tools.ask_client_to_close(window);
    });
}

void WrappedWindowManagementPolicy::forceClose(const miral::Window &window)
{
    tools.invoke_under_lock([&window, this]() {
        tools.ask_client_to_close(window);
    });
}

void WrappedWindowManagementPolicy::set_window_confinement_regions(const QVector<QRect> &regions)
{
    d->m_confinementRegions = regions;

    // TODO: update window positions to respect new boundary.
}

void WrappedWindowManagementPolicy::set_window_margins(MirWindowType windowType, const QMargins &margins)
{
    d->m_windowMargins[windowType] = margins;

    // TODO: update window positions/sizes to respect new margins.
}

void WrappedWindowManagementPolicy::handle_request_drag_and_drop(miral::WindowInfo &window_info)
{
    m_wrapper->handle_request_drag_and_drop(window_info);
}

void WrappedWindowManagementPolicy::handle_request_move(miral::WindowInfo &window_info,
                                                const MirInputEvent *input_event)
{
    m_wrapper->handle_request_move(window_info, input_event);
}

void WrappedWindowManagementPolicy::handle_request_resize(miral::WindowInfo &window_info,
                                                const MirInputEvent *input_event, MirResizeEdge edge)
{
    m_wrapper->handle_request_resize(window_info, input_event, edge);
}
void WrappedWindowManagementPolicy::for_each_window_in_workspace(const std::shared_ptr<miral::Workspace> &workspace,
                                                                 std::function<void(miral::Window const&)> const& callback)
{
    tools.invoke_under_lock([&]() {
        tools.for_each_window_in_workspace(workspace, callback);
    });
}

void WrappedWindowManagementPolicy::move_worspace_content_to_workspace(const std::shared_ptr<miral::Workspace> &to,
                                                                       const std::shared_ptr<miral::Workspace> &from)
{
    tools.invoke_under_lock([&]() {
        tools.move_workspace_content_to_workspace(to, from);
    });
}

void WrappedWindowManagementPolicy::move_window_to_workspace(const miral::Window &window,
                                                             const std::shared_ptr<miral::Workspace> &workspace)
{
    tools.invoke_under_lock([&]() {
        auto root = window;
        auto const* info = &tools.info_for(root);

        while (auto const& parent = info->parent())
        {
            root = parent;
            info = &tools.info_for(root);
        }

        std::vector<std::shared_ptr<miral::Workspace>> workspaces;
        tools.for_each_workspace_containing(root, [&](std::shared_ptr<miral::Workspace> const& from) {
            workspaces.push_back(from);
        });
        for (auto wks : workspaces) {
            tools.remove_tree_from_workspace(root, wks);
        }
        tools.add_tree_to_workspace(root, workspace);
    });
}
