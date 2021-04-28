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

#ifndef MOCK_QTMIR_SESSION_H
#define MOCK_QTMIR_SESSION_H

#include <QtMir/Application/session_interface.h>
#include "promptsession.h"
#include <gmock/gmock.h>

namespace qtmir {

class MockSession : public SessionInterface {
public:
    MockSession();
    virtual ~MockSession();

    MOCK_METHOD0(release, void());

    MOCK_CONST_METHOD0(name, QString());
    MOCK_CONST_METHOD0(application, lomiri::shell::application::ApplicationInfoInterface*());
    MOCK_CONST_METHOD0(lastSurface, MirSurfaceInterface*());
    MOCK_METHOD0(surfaceList, MirSurfaceListModel*());
    MOCK_CONST_METHOD0(childSessions, SessionModel*());

    MOCK_CONST_METHOD0(state, State());

    MOCK_CONST_METHOD0(fullscreen, bool());
    MOCK_CONST_METHOD0(live, bool());

    MOCK_CONST_METHOD0(session, std::shared_ptr<mir::scene::Session>());

    MOCK_METHOD1(registerSurface, void(MirSurfaceInterface* surface));
    MOCK_METHOD1(removeSurface, void(MirSurfaceInterface* surface));

    MOCK_METHOD1(setApplication, void(lomiri::shell::application::ApplicationInfoInterface* item));

    MOCK_METHOD0(suspend, void());
    MOCK_METHOD0(resume, void());
    MOCK_METHOD0(close, void());
    MOCK_METHOD0(stop, void());
    MOCK_CONST_METHOD0(hadSurface, bool());
    MOCK_CONST_METHOD0(hasClosingSurfaces, bool());

    MOCK_METHOD1(addChildSession, void(SessionInterface* session));
    MOCK_METHOD2(insertChildSession, void(uint index, SessionInterface* session));
    MOCK_METHOD1(removeChildSession, void(SessionInterface* session));
    MOCK_CONST_METHOD1(foreachChildSession, void(const std::function<void(SessionInterface* session)> &f));

    MOCK_CONST_METHOD0(activePromptSession, qtmir::PromptSession());
    MOCK_CONST_METHOD1(foreachPromptSession, void(const std::function<void(const qtmir::PromptSession&)> &f));

    void setState(State state);

    void doSuspend();
    void doResume();
    void doStop();
    State doState() const;

protected:
    MOCK_METHOD1(setFullscreen, void(bool fullscreen));
    MOCK_METHOD1(setLive, void(const bool));
    MOCK_METHOD1(appendPromptSession, void(const qtmir::PromptSession& session));
    MOCK_METHOD1(removePromptSession, void(const qtmir::PromptSession& session));

private:
    State m_state;
};

} // namespace qtmir

#endif // MOCK_QTMIR_SESSION_H
