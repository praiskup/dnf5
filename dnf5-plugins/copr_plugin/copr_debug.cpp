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

#include "copr.hpp"
#include "copr_repo.hpp"

#include <iostream>

namespace dnf5 {

void CoprDebugCommand::run() {
    auto name_version = copr_config->get_value("main", "name_version");
    auto arch = copr_config->get_value("main", "arch");
    std::cout << "hubspec: " << selected_hubspec() << std::endl;
    std::cout << "hub_hostname: " << copr_config->get_hub_hostname(selected_hubspec()) << std::endl;
    std::cout << "name_version: " << name_version << std::endl;
    std::cout << "arch: " << arch << std::endl;
    std::cout << "repo_fallback_priority:" << std::endl;
    for (const auto & item : repo_fallbacks(name_version)) {
        std::cout << "  - " << item << std::endl;
    }
}

}  // namespace dnf5
