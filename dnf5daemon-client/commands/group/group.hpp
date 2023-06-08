/*
Copyright Contributors to the libdnf project.

This file is part of libdnf: https://github.com/rpm-software-management/libdnf/

Libdnf is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Libdnf is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with libdnf.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef DNF5DAEMON_CLIENT_COMMANDS_GROUP_GROUP_HPP
#define DNF5DAEMON_CLIENT_COMMANDS_GROUP_GROUP_HPP

#include "commands/command.hpp"

#include <libdnf5/conf/option_enum.hpp>

#include <memory>
#include <vector>

namespace dnfdaemon::client {

class GroupCommand : public DaemonCommand {
public:
    explicit GroupCommand(Context & context) : DaemonCommand(context, "group") {}
    void set_parent_command() override;
    void set_argument_parser() override;
    void pre_configure() override;
};

}  // namespace dnfdaemon::client

#endif
