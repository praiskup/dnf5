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

using namespace libdnf::cli;

namespace dnf5 {

void CoprEnableCommand::set_argument_parser() {
    auto & ctx = get_context();
    auto & cmd = *get_argument_parser_command();
    auto & parser = ctx.get_argument_parser();
    auto project = parser.add_new_positional_arg("PROJECT_SPEC", 1, nullptr, nullptr);
    project->set_description(
        _("Copr project ID to enable.  Use either a format OWNER/PROJECT "
          "or HUB/OWNER/PROJECT (if HUB is not specified, the default one, "
          "or --hub <ARG>, is used.  OWNER is either a username, or "
          "a @groupname.  PROJECT can be a simple project name, "
          "or a \"project directory\" containing colons, e.g. "
          "'project:custom:123'.  HUB can be either the Copr frontend "
          "hostname (e.g. copr.fedorainfracloud.org ) or the "
          "shortcut (e.g. fedora).  Example: 'fedora/@footeam/coolproject'."));

    project->set_parse_hook_func([this](
                                     [[maybe_unused]] ArgumentParser::PositionalArg * arg,
                                     [[maybe_unused]] int argc,
                                     const char * const argv[]) {
        std::string project_spec = argv[0];
        std::smatch match;
        enum { HUB = 2, OWNER = 3, DIRNAME = 4 };
        if (!std::regex_match(project_spec, match, std::regex("^(([^/]+)/)?([^/]+)/([^/]*)$"))) {
            throw ArgumentParserPositionalArgumentFormatError(M_("Invalid PROJECT_SPEC format '{}'"), project_spec);
        }
        opt_hub = match[HUB];
        opt_owner = match[OWNER];
        opt_dirname = match[DIRNAME];
        return true;
    });

    auto chroot = parser.add_new_positional_arg("CHROOT", ArgumentParser::PositionalArg::OPTIONAL, nullptr, nullptr);
    chroot->set_description(
        _("Chroot specified in the NAME-RELEASE-ARCH format, "
          "e.g. 'fedora-rawhide-ppc64le'.  When not specified, "
          "the 'dnf copr' command attempts to detect it."));
    chroot->set_parse_hook_func([this](
                                    [[maybe_unused]] ArgumentParser::PositionalArg * arg,
                                    [[maybe_unused]] int argc,
                                    const char * const argv[]) {
        opt_chroot = argv[0];
        return true;
    });

    cmd.register_positional_arg(project);
    cmd.register_positional_arg(chroot);
}


void CoprEnableCommand::run() {
    auto & ctx = get_context();
    auto hubspec = opt_hub != "" ? opt_hub : selected_hubspec();
    CoprRepo(ctx.base, copr_config, hubspec, opt_owner, opt_dirname, opt_chroot).save();
}

}  // namespace dnf5
