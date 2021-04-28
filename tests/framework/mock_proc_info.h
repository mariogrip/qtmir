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

#ifndef MOCK_PROC_INFO_H
#define MOCK_PROC_INFO_H

#include <QtMir/Application/proc_info.h>

#include <gmock/gmock.h>

namespace qtmir {

struct MockProcInfo : public qtmir::ProcInfo
{
    MockProcInfo();
    virtual ~MockProcInfo() = default;

    MOCK_METHOD1(command_line, QByteArray(pid_t));
    MOCK_METHOD1(set_environment, QByteArray(pid_t));

    std::unique_ptr<CommandLine> commandLine(pid_t pid) override;
    std::unique_ptr<Environment> environment(pid_t pid) override;
};

} // namespace qtmir

#endif // MOCK_PROC_INFO_H
