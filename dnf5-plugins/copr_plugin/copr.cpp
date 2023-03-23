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

#include <glob.h>

#include <iostream>

#define dnf5_command "dnf5"

namespace dnf5 {

using namespace libdnf::cli;

template <typename... Args>
void warning(const char * format, Args &&... args) {
    std::cerr << "WARNING: " + libdnf::utils::sformat(format, std::forward<Args>(args)...) << std::endl;
}

CoprCommand * CoprSubCommand::copr_cmd() {
    return static_cast<CoprCommand *>(this->get_parent_command());
}


std::vector<std::string> repo_fallbacks(const std::string & name_version) {
    std::vector<std::string> retval;
    retval.push_back(name_version);
    return retval;
}


void CoprSubCommand::load_copr_config_file(const std::string & filename) {
    if (copr_config == nullptr)
        copr_config = std::make_unique<CoprConfig>();
    if (!std::filesystem::exists(filename))
        return;
    copr_config->read(filename);
}


void CoprSubCommand::configure() {
    load_copr_config_file("/etc/dnf/plugins/copr.vendor.conf");
    load_copr_config_file("/etc/dnf/plugins/copr.conf");
    std::string pattern = "/etc/dnf/plugins/copr.d/*.conf";
    glob_t glob_result;
    glob(pattern.c_str(), GLOB_MARK, nullptr, &glob_result);
    for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
        std::string file_path = glob_result.gl_pathv[i];
        load_copr_config_file(file_path);
    }

    // For DNF4, we used to have:
    // https://github.com/rpm-software-management/dnf-plugins-core/blob/48b29df7e6bb882ebc5a5a927726252626c2ab59/plugins/copr.py#L43-L47
    // This is a non-trivial task to do so we do a best effort guesses here.
    // Distributions that don't match this detection mechanism can provide
    // the copr.vendor.conf:
    //
    //   [main]
    //   distribution = abc
    //   releasever = xyz
    //
    // Or provide fix here:

    if (!copr_config->has_option("main", "distribution")) {
        copr_config->set_value("main", "distribution", os_release.get_value("ID"));
    }

    if (!copr_config->has_option("main", "releasever")) {
        copr_config->set_value("main", "releasever", os_release.get_value("VERSION_ID"));
    }

    // Set the "name_version" for the later convenience
    std::string name_version = copr_config->get_value("main", "distribution");
    name_version += "-" + copr_config->get_value("main", "releasever");
    copr_config->set_value("main", "name_version", name_version);

    std::string arch = get_context().base.get_vars()->get_value("arch");
    copr_config->set_value("main", "arch", arch);
}


std::string CoprSubCommand::selected_hubspec() {
    std::string hubspec = copr_cmd()->hub();
    if (hubspec == "")
        hubspec = DEFAULT_COPR_HUB;
    return hubspec;
}


}  // namespace dnf5
