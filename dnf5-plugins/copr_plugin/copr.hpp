/*
Copyright Contributors to the libdnf project.

This file is part of libdnf: https://github.com/rpm-software-management/libdnf/

Libdnf is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 of the License, or
(at your option) any later version.

Libdnf is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with libdnf.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef DNF5_COMMANDS_COPR_COPR_HPP
#define DNF5_COMMANDS_COPR_COPR_HPP

#include "utils/bgettext/bgettext-lib.h"
#include "utils/bgettext/bgettext-mark-domain.h"

// TODO: implement 'dnf copr check' - This feature should try to compare the
//       currently installed repo files with the current Copr state (e.g.
//       projects removed in Copr, external requirements changed, multilib
//       changed, chroot EOL).
// TODO: implement 'dnf copr fix' - ^^^ should help

#include "os_release.hpp"

#include <dnf5/context.hpp>
#include <libdnf/conf/config_parser.hpp>

static const char * const COPR_COMMAND_DESCRIPTION = _("Manage Copr repositories (community add-ons)");
static const char * const COPR_THIRD_PARTY_WARNING =
    _("Enabling a Copr repository. Please note that this repository is not part\n"
      "of the main distribution, and quality may vary.\n"
      "\n"
      "The Fedora Project does not exercise any power over the contents of\n"
      "this repository beyond the rules outlined in the Copr FAQ at\n"
      "<https://docs.pagure.org/copr.copr/user_documentation.html#what-i-can-build-in-copr>,\n"
      "and packages are not held to any quality or security level.\n"
      "\n"
      "Please do not file bug reports about these packages in Fedora\n"
      "Bugzilla. In case of problems, contact the owner of this repository.\n");

#define DEFAULT_COPR_HUB "copr.fedorainfracloud.org"
#define dnf5_command     "dnf5"

namespace dnf5 {

using bool_opt_t = std::unique_ptr<libdnf::cli::session::BoolOption>;
using str_map_t = std::map<std::string, std::string>;

class CoprCommand;

class CoprConfig : public libdnf::ConfigParser {
public:
    std::string get_hub_hostname(const std::string & hubspec) {
        if (!this->has_section(hubspec))
            return hubspec;
        return this->get_value(hubspec, "hostname");
    }
    std::string get_hub_url(const std::string & hubspec) {
        std::string protocol = "https";
        std::string port = "";
        std::string host = hubspec;
        if (this->has_option(hubspec, "hostname")) {
            host = this->get_value(hubspec, "hostname");
        }
        if (this->has_option(hubspec, "protocol")) {
            protocol = this->get_value(hubspec, "protocol");
        }
        if (this->has_option(hubspec, "port")) {
            port = ":" + this->get_value(hubspec, "port");
        }
        return protocol + "://" + host + port;
    }

    std::string get_repo_url(
        const std::string & hubspec,
        const std::string & ownername,
        const std::string & dirname,
        const std::string & name_version) {
        return get_hub_url(hubspec) + "/api_3/rpmrepo/" + ownername + "/" + dirname + "/" + name_version + "/";
    }
};


class CoprSubCommand : public Command {
public:
    explicit CoprSubCommand(Context & context, const std::string & name) : Command(context, name) {}
    CoprCommand * copr_cmd();
    void configure() override;

protected:
    /// TODO: move these to CoprCommand (somehow)
    void load_copr_config_file(const std::string &);
    OSRelease os_release;
    std::unique_ptr<CoprConfig> copr_config{nullptr};
    std::string get_hub_hostname(const std::string & hubspec);
    std::string selected_hubspec();
};


class CoprListCommand : public CoprSubCommand {
public:
    explicit CoprListCommand(Context & context) : CoprSubCommand(context, "list") {}
    void set_argument_parser() override;
    void run() override;

private:
    bool_opt_t installed{nullptr};
};


class CoprDebugCommand : public CoprSubCommand {
public:
    explicit CoprDebugCommand(Context & context) : CoprSubCommand(context, "debug") {}
    void run() override;
    void set_argument_parser() override{};
};


class CoprEnableCommand : public CoprSubCommand {
public:
    explicit CoprEnableCommand(Context & context) : CoprSubCommand(context, "enable") {}
    void run() override;
    void set_argument_parser() override;

private:
    std::string opt_hub = "";
    std::string opt_owner;
    std::string opt_dirname;
    std::string opt_chroot = "";
};


class CoprCommand : public Command {
public:
    explicit CoprCommand(Context & context) : Command(context, "copr") {}

    void set_parent_command() override {
        auto * arg_parser_parent_cmd = this->get_session().get_argument_parser().get_root_command();
        auto * arg_parser_this_cmd = this->get_argument_parser_command();
        arg_parser_parent_cmd->register_command(arg_parser_this_cmd);
    }

    void set_argument_parser() override {
        auto & cmd = *this->get_argument_parser_command();
        cmd.set_description(COPR_COMMAND_DESCRIPTION);
        cmd.set_long_description(COPR_COMMAND_DESCRIPTION);

        auto & parser = cmd.get_argument_parser();

        auto * hub_arg = parser.add_new_named_arg("hub");
        hub_arg->set_long_name("hub");
        hub_arg->set_description(_("Copr hub (web-UI) hostname"));
        hub_arg->set_arg_value_help("HOSTNAME");
        hub_arg->link_value(&hub_option);
        hub_arg->set_has_value(true);
        cmd.register_named_arg(hub_arg);
    };

    void register_subcommands() override {
        register_subcommand(std::make_unique<CoprListCommand>(get_context()));
        register_subcommand(std::make_unique<CoprEnableCommand>(get_context()));
        register_subcommand(std::make_unique<CoprDebugCommand>(get_context()));
    }

    void pre_configure() override { this->throw_missing_command(); };

    const std::string & hub() { return this->hub_option.get_value(); }

private:
    libdnf::OptionString hub_option{""};
};


std::vector<std::string> repo_fallbacks(const std::string & name_version);


template <typename... Args>
void warning(const char * format, Args &&... args);


}  // namespace dnf5


#endif  // DNF5_COMMANDS_COPR_COPR_HPP
