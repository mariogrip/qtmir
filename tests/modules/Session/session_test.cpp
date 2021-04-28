/*
 * Copyright (C) 2014-2017 Canonical, Ltd.
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

#include <qtmir_test.h>
#include <fake_mirsurface.h>

#include "promptsession.h"
#include <QtMir/Application/application.h>
#include <QtMir/Application/session.h>

#include <QSignalSpy>

using namespace qtmir;
using mir::scene::MockSession;

namespace ms = mir::scene;

class SessionTests : public ::testing::QtMirTest
{
public:
    SessionTests()
    {}

    QList<SessionInterface*> listChildSessions(Session* session) {
        QList<SessionInterface*> sessions;
        session->foreachChildSession([&sessions](SessionInterface* session) {
            sessions << session;
        });
        return sessions;
    }
};

TEST_F(SessionTests, FromStartingToRunningOnceSurfaceDrawsFirstFrame)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    auto mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    auto session = std::make_shared<qtmir::Session>(mirSession, promptSessionManager);

    // On Starting as it has no surface.
    EXPECT_EQ(Session::Starting, session->state());

    FakeMirSurface *surface = new FakeMirSurface;
    session->registerSurface(surface);

    // Still on Starting as the surface hasn't drawn its first frame yet
    EXPECT_EQ(Session::Starting, session->state());

    surface->setReady();
    EXPECT_EQ(Session::Running, session->state());

    delete surface;
}

TEST_F(SessionTests, AddChildSession)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    std::shared_ptr<ms::Session> mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    Session session(mirSession, promptSessionManager);
    Session session1(mirSession, promptSessionManager);
    Session session2(mirSession, promptSessionManager);
    Session session3(mirSession, promptSessionManager);

    // add surfaces
    session.addChildSession(&session1);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1));

    session.addChildSession(&session2);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1, &session2));

    session.addChildSession(&session3);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1, &session2, &session3));
}

TEST_F(SessionTests, InsertChildSession)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    std::shared_ptr<ms::Session> mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    Session session(mirSession, promptSessionManager);
    Session session1(mirSession, promptSessionManager);
    Session session2(mirSession, promptSessionManager);
    Session session3(mirSession, promptSessionManager);

    // add surfaces
    session.insertChildSession(100, &session1); // test overflow
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1));

    session.insertChildSession(0, &session2); // test insert before
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session2, &session1));

    session.insertChildSession(1, &session3); // test before end
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session2, &session3, &session1));
}

TEST_F(SessionTests, RemoveChildSession)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    std::shared_ptr<ms::Session> mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    Session session(mirSession, promptSessionManager);
    Session session1(mirSession, promptSessionManager);
    Session session2(mirSession, promptSessionManager);
    Session session3(mirSession, promptSessionManager);

    // add surfaces
    session.addChildSession(&session1);
    session.addChildSession(&session2);
    session.addChildSession(&session3);

    // remove surfaces
    session.removeChildSession(&session2);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1, &session3));

    session.removeChildSession(&session3);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1));

    session.removeChildSession(&session1);
    EXPECT_THAT(listChildSessions(&session), IsEmpty());
}

TEST_F(SessionTests, DeleteChildSessionRemovesFromApplication)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    std::shared_ptr<ms::Session> mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    Session session(mirSession, promptSessionManager);
    Session* session1 = new Session(mirSession, promptSessionManager);
    Session* session2 = new Session(mirSession, promptSessionManager);
    Session* session3 = new Session(mirSession, promptSessionManager);

    // add surfaces
    session.addChildSession(session1);
    session.addChildSession(session2);
    session.addChildSession(session3);

    // delete surfaces
    delete session2;
    EXPECT_THAT(listChildSessions(&session), ElementsAre(session1, session3));

    // delete surfaces
    delete session3;
    EXPECT_THAT(listChildSessions(&session), ElementsAre(session1));

    // delete surfaces
    delete session1;
    EXPECT_THAT(listChildSessions(&session), IsEmpty());
}

TEST_F(SessionTests, DeleteSessionDeletesChildSessions)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    std::shared_ptr<ms::Session> mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    Session* session = new Session(mirSession, promptSessionManager);
    Session* session1 = new Session(mirSession, promptSessionManager);
    Session* session2 = new Session(mirSession, promptSessionManager);
    Session* session3 = new Session(mirSession, promptSessionManager);

    // add children
    session->addChildSession(session1);
    session->addChildSession(session2);
    session->addChildSession(session3);

    QList<QObject*> destroyed;
    QObject::connect(session1, &QObject::destroyed, [&](QObject*) { destroyed << session1; });
    QObject::connect(session2, &QObject::destroyed, [&](QObject*) { destroyed << session2; });
    QObject::connect(session3, &QObject::destroyed, [&](QObject*) { destroyed << session3; });

    delete session;
    EXPECT_TRUE(destroyed.contains(session1));
    EXPECT_TRUE(destroyed.contains(session2));
    EXPECT_TRUE(destroyed.contains(session3));
}

TEST_F(SessionTests, SuspendPromptSessionWhenSessionSuspends)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    auto mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    auto session = std::make_shared<qtmir::Session>(mirSession, promptSessionManager);
    {
        FakeMirSurface *surface = new FakeMirSurface;
        session->registerSurface(surface);
        surface->setReady();
        delete surface;
    }
    EXPECT_EQ(Session::Running, session->state());

    qtmir::PromptSession promptSession{std::make_shared<ms::MockPromptSession>()};
    session->appendPromptSession(promptSession);

    EXPECT_CALL(*stubPromptSessionManager, suspend_prompt_session(_)).Times(1);

    EXPECT_CALL(*mirSession, set_lifecycle_state(mir_lifecycle_state_will_suspend));
    session->suspend();
    EXPECT_EQ(Session::Suspending, session->state());
    session->doSuspend();
    EXPECT_EQ(Session::Suspended, session->state());

    Mock::VerifyAndClear(promptSessionManager.get());
}

TEST_F(SessionTests, ResumePromptSessionWhenSessionResumes)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    auto mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    auto session = std::make_shared<qtmir::Session>(mirSession, promptSessionManager);
    {
        FakeMirSurface *surface = new FakeMirSurface;
        session->registerSurface(surface);
        surface->setReady();
        delete surface;
    }
    EXPECT_EQ(Session::Running, session->state());

    EXPECT_CALL(*mirSession, set_lifecycle_state(mir_lifecycle_state_will_suspend));
    session->suspend();
    EXPECT_EQ(Session::Suspending, session->state());
    session->doSuspend();
    EXPECT_EQ(Session::Suspended, session->state());

    qtmir::PromptSession promptSession{std::make_shared<ms::MockPromptSession>()};
    session->appendPromptSession(promptSession);

    EXPECT_CALL(*stubPromptSessionManager, resume_prompt_session(_)).Times(1);

    EXPECT_CALL(*mirSession, set_lifecycle_state(mir_lifecycle_state_resumed));
    session->resume();
    EXPECT_EQ(Session::Running, session->state());

    Mock::VerifyAndClear(promptSessionManager.get());
}

TEST_F(SessionTests, SessionStopsWhileSuspendingDoesntSuspend)
{
    using namespace testing;

    class SessionTestClass : public qtmir::Session
    {
    public:
        SessionTestClass(const std::shared_ptr<mir::scene::Session>& session,
                         const std::shared_ptr<qtmir::PromptSessionManager>& promptSessionManager)
            : Session(session, promptSessionManager) {}

        using Session::m_suspendTimer;
    };

    const QString appId("test-app");
    const pid_t procId = 5551;

    auto mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    auto session = std::make_shared<SessionTestClass>(mirSession, promptSessionManager);
    {
        FakeMirSurface *surface = new FakeMirSurface;
        session->registerSurface(surface);
        surface->setReady();
        delete surface;
    }
    EXPECT_EQ(Session::Running, session->state());

    EXPECT_CALL(*mirSession, set_lifecycle_state(mir_lifecycle_state_will_suspend));
    session->suspend();
    EXPECT_EQ(Session::Suspending, session->state());
    EXPECT_TRUE(session->m_suspendTimer->isRunning());

    session->setLive(false);
    EXPECT_EQ(Session::Stopped, session->state());
    EXPECT_FALSE(session->m_suspendTimer->isRunning());
}

TEST_F(SessionTests, CloseAllSurfaceOnSessionClose)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    auto mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    auto session = std::make_shared<qtmir::Session>(mirSession, promptSessionManager);

    FakeMirSurface *surface1 = new FakeMirSurface;
    session->registerSurface(surface1);
    surface1->setReady();

    FakeMirSurface *surface2 = new FakeMirSurface;
    session->registerSurface(surface2);
    surface2->setReady();

    EXPECT_EQ(session->surfaceList()->count(), 2);
    session->close();
    EXPECT_EQ(session->surfaceList()->count(), 0);

    delete surface2;
    delete surface1;
}

TEST_F(SessionTests, EmitFocusedChangedWhenAddingAlreadyFocusedSurface)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    auto mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    auto session = std::make_shared<qtmir::Session>(mirSession, promptSessionManager);

    FakeMirSurface *surface = new FakeMirSurface;
    surface->setFocused(true);
    surface->setReady();

    QSignalSpy spy(session.get(), SIGNAL(focusedChanged(bool)));

    session->registerSurface(surface);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toBool(), true);

    delete surface;
}

TEST_F(SessionTests, AlsoKillClosingSurfacesWhenKilled)
{
    using namespace testing;

    const QString appId("test-app");
    const pid_t procId = 5551;

    auto mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    auto session = std::make_shared<qtmir::Session>(mirSession, promptSessionManager);

    FakeMirSurface *surface = new FakeMirSurface;
    surface->setReady();

    session->registerSurface(surface);

    surface->close();

    ASSERT_EQ(surface->live(), true);

    session->setLive(false);

    EXPECT_EQ(surface->live(), false);

    delete surface;
}
