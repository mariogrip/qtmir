/*
 * Copyright (C) 2014-2015 Canonical, Ltd.
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

#ifndef SESSION_INTERFACE_H
#define SESSION_INTERFACE_H

#include <functional>
#include <memory>

// Unity API
#include <lomiri/shell/application/ApplicationInfoInterface.h>

// local
#include "objectlistmodel.h"
#include "sessionmodel.h"
#include <miroil/promptsession.h>

// Qt
#include <QQmlListProperty>

namespace mir {
    namespace scene {
        class Session;
    }
}

namespace qtmir {

class MirSurfaceInterface;
class MirSurfaceListModel;

class SessionInterface : public QObject {
    Q_OBJECT

    Q_PROPERTY(lomiri::shell::application::ApplicationInfoInterface* application READ application NOTIFY applicationChanged DESIGNABLE false)
    Q_PROPERTY(SessionModel* childSessions READ childSessions DESIGNABLE false CONSTANT)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool live READ live NOTIFY liveChanged)
public:
    SessionInterface(QObject *parent = 0) : QObject(parent) {}
    virtual ~SessionInterface() {}

    // Ordered by importance/activity. Used for calculating the combined state of multiple sessions
    enum State {
        Running = 4,
        Starting = 3,
        Suspending = 2,
        Suspended = 1,
        Stopped = 0
    };

    //getters
    virtual QString name() const = 0;
    virtual lomiri::shell::application::ApplicationInfoInterface* application() const = 0;

    // List of surfaces in this session. Surfaces that are being forcibly closed are not present in this list
    virtual MirSurfaceListModel* surfaceList() = 0;

    // List of prompt surfaces under this session (and its children)
    virtual MirSurfaceListModel* promptSurfaceList() = 0;

    virtual SessionModel* childSessions() const = 0;
    virtual State state() const = 0;
    virtual bool fullscreen() const = 0;
    virtual bool live() const = 0;

    virtual std::shared_ptr<mir::scene::Session> session() const = 0;

    // For MirSurface use

    virtual void registerSurface(MirSurfaceInterface* surface) = 0;

    // For Application use

    virtual void setApplication(lomiri::shell::application::ApplicationInfoInterface* item) = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual void close() = 0;
    virtual void stop() = 0;
    virtual bool hadSurface() const = 0; // whether this session ever had any surface (currently or in the past)
    virtual bool hasClosingSurfaces() const = 0; // whether it has surfaces being forcibly closed
    virtual bool focused() const = 0; // whether any surface in its list is focused()

    // Whether any of its MirSurfaces has activeFocus()
    // See qtmir::MirSurfaceInterface::activeFocus
    virtual bool activeFocus() const = 0;

    virtual pid_t pid() const = 0;

    // For TaskController use

    virtual void addChildSession(SessionInterface* session) = 0;
    virtual void insertChildSession(uint index, SessionInterface* session) = 0;
    virtual void removeChildSession(SessionInterface* session) = 0;
    virtual void foreachChildSession(const std::function<void(SessionInterface* session)>& f) const = 0;

    virtual miroil::PromptSession activePromptSession() const = 0;
    virtual void foreachPromptSession(const std::function<void(const miroil::PromptSession&)>& f) const = 0;

    virtual void setFullscreen(bool fullscreen) = 0;
    virtual void setLive(const bool) = 0;
    virtual void appendPromptSession(const miroil::PromptSession& session) = 0;
    virtual void removePromptSession(const miroil::PromptSession& session) = 0;

Q_SIGNALS:
    void applicationChanged(lomiri::shell::application::ApplicationInfoInterface* application);
    void stateChanged(State state);
    void fullscreenChanged(bool fullscreen);
    void liveChanged(bool live);
    void focusedChanged(bool focused);

    // Emitted when any surface in this session emits focusRequested()
    void focusRequested();

    // For Application use
    void hasClosingSurfacesChanged();
};

} // namespace qtmir

Q_DECLARE_METATYPE(qtmir::SessionInterface*)
Q_DECLARE_METATYPE(qtmir::ObjectListModel<qtmir::MirSurfaceInterface>*)

#endif // SESSION_INTERFACE_H
