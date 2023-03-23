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


#include "copr_repo.hpp"

#include "json.hpp"

#include <fnmatch.h>

#include <filesystem>
#include <fstream>
#include <string>


namespace dnf5 {


std::string expand_at_in_groupname(const std::string & ownername) {
    if (ownername.starts_with("@"))
        return "group_" + ownername.substr(1);
    return ownername;
}


std::string copr_id_from_repo_id(const std::string & repo_id) {
    /// Convert the repoID to Copr ID (that we can enable or disable using
    /// the 'dnf copr' utility.  Example:
    /// - copr:copr.fedorainfracloud.org:group_copr:copr:suffix
    /// + copr.fedorainfracloud.org/@copr/copr
    /// Return empty string if the conversion isn't possible.

    if (!repo_id.starts_with("copr:"))
        return "";

    // copr.fedorainfracloud.org:group_copr:copr:suffix
    auto copr_id = std::regex_replace(repo_id, std::regex("^copr:"), "");
    // copr.fedorainfracloud.org/group_copr:copr:suffix
    copr_id = std::regex_replace(copr_id, std::regex(":"), "/", std::regex_constants::format_first_only);
    // copr.fedorainfracloud.org/@copr:copr:suffix
    copr_id = std::regex_replace(copr_id, std::regex("/group_"), "/@");
    // copr.fedorainfracloud.org/@copr/copr:suffix
    copr_id = std::regex_replace(copr_id, std::regex(":"), "/", std::regex_constants::format_first_only);
    // drop "multilib" suffix: copr.fedorainfracloud.org/copr/ping:ml
    copr_id = std::regex_replace(copr_id, std::regex(":ml$"), "");
    return copr_id;
}


void available_chroots_error(const std::set<std::string> & chroots, const std::string & chroot) {
    std::stringstream message;
    if (chroot != "")
        message << _("Chroot not found in the given Copr project") << " (" << chroot << ").";
    else
        message << _("Unable to detect chroot, specify it explicitly.");

    bool first = true;
    message << " " << _("You can choose one of the avaiable chroots explicitly:") << std::endl;
    for (const auto & available : chroots) {
        if (!first)
            message << std::endl;
        else
            first = false;
        message << " " << available;
    }
    throw std::runtime_error(message.str());
}


std::string copr_id_to_copr_file(const std::string & repo_id) {
    /// Convert copr ID to a repo filename.
    /// - copr.fedorainfracloud.org/@copr/copr-pull-requests:pr:2545
    /// + _copr:copr.fedorainfracloud.org:group_copr:copr-pull-requests:pr:2545.repo

    std::string output = std::regex_replace(repo_id, std::regex(":ml$"), "");
    output = std::regex_replace(output, std::regex("/"), ":");
    output = std::regex_replace(output, std::regex("@"), "group_");
    return "_copr:" + output + ".repo";
}


std::string get_repo_triplet(
    const std::set<std::string> & available_chroots,
    const std::string & config_name_version,
    const std::string & config_arch,
    std::string & name_version) {
    /// fedora-17 x86_64 => fedora-$releasever-$arch
    /// explicit_chroot  => explicit_chroot (if found)
    /// fedora-eln       => ?
    /// rhel-8           => rhel-$basearch-$arch => epel-$basearch-$arch
    /// centos-8         => rhel-$basearch-$arch => epel-$basearch-$arch

    // all available chroots to enable

    for (const auto & nv : repo_fallbacks(config_name_version)) {
        name_version = nv;
        auto chroot = nv + "-" + config_arch;
        if (available_chroots.contains(chroot)) {
            // do the magic with $releasever and $basearch
            if (nv == "fedora-eln")
                return nv + "-$basearch";
            if (nv.starts_with("fedora-"))
                return "fedora-$releasever-$basearch";

            if (nv.starts_with("opensuse-leap-"))
                return "opensuse-leap-$releasever-$basearch";

            if (nv.starts_with("mageia")) {
                std::string os_version = "$releasever";
                if (nv.ends_with("cauldron")) {
                    os_version = "cauldron";
                }
                return "mageia-" + os_version + "-$basearch";
            }

            return nv + "-$basearch";
        }
    }

    name_version = "";
    return "";
}


CoprRepo::CoprRepo() {
    enabled = false;
    has_external_deps = false;
    multilib = false;
};

CoprRepo::CoprRepo(
    libdnf::Base & base,
    const std::unique_ptr<CoprConfig> & copr_config,
    const std::string & hubspec,
    const std::string & project_owner,
    const std::string & project_dirname,
    const std::string & selected_chroot) {
    enabled = false;
    has_external_deps = false;
    multilib = false;

    // All available chroots in the selected repo.
    auto config_name_version = copr_config->get_value("main", "name_version");
    auto arch = copr_config->get_value("main", "arch");

    auto url = copr_config->get_repo_url(hubspec, project_owner, project_dirname, config_name_version);
    auto json = std::make_unique<Json>(base, url);

    auto json_repos = json->get_dict_item("repos");
    std::set<std::string> available_chroots;
    for (const auto & name_version : json_repos->keys()) {
        auto arches = json_repos->get_dict_item(name_version)->get_dict_item("arch");
        for (const auto & arch_found : arches->keys()) {
            auto chroot = name_version + "-" + arch_found;
            available_chroots.insert(chroot);
        }
    }

    auto project_name = std::regex_replace(project_dirname, std::regex(":.*"), "");
    std::string baseurl_chroot = "";
    std::string json_selector;

    if (selected_chroot != "") {
        // We do not expand $basearch and $releasever here because user
        // explicitly asked for a particular some chroot (which might
        // intentionally be a different distro or a cross-arch chroot).
        baseurl_chroot = selected_chroot;
        if (!available_chroots.contains(selected_chroot))
            available_chroots_error(available_chroots, selected_chroot);
        // remove the "-<arch>" suffix
        json_selector = std::regex_replace(selected_chroot, std::regex("-[^-]*"), "");
        arch = std::regex_replace(selected_chroot, std::regex(".*-"), "");
    } else {
        baseurl_chroot = get_repo_triplet(available_chroots, config_name_version, arch, json_selector);
        if (baseurl_chroot == "") {
            auto detected_chroot = config_name_version + "-" + arch;
            available_chroots_error(available_chroots, detected_chroot);
        }
    }

    auto owner = expand_at_in_groupname(project_owner);
    std::string repo_id = "copr:" + copr_config->get_hub_hostname(hubspec) + ":" + owner + ":" + project_dirname;
    set_id_from_repo_id(repo_id);

    std::string results_url = json->get_dict_item("results_url")->string();
    std::string baseurl = results_url + "/" + project_owner + "/" + project_dirname + "/" + baseurl_chroot + "/";
    std::string name = "Copr repo for " + project_dirname + " owned by " + project_owner;
    std::string gpgkey = results_url + "/" + project_owner + "/" + project_name + "/" + "pubkey.gpg";

    auto main_repo_json = json_repos->get_dict_item(json_selector)->get_dict_item("arch")->get_dict_item(arch);
    auto main_repo = CoprRepoPart(repo_id, name, true, baseurl, gpgkey);
    main_repo.update_from_json_opts(main_repo_json);
    repositories.push_back(main_repo);

    // multilib repositories
    if (selected_chroot == "" && main_repo_json->has_key("multilib")) {
        auto mljson = main_repo_json->get_dict_item("multilib");
        int suffix = 0;
        for (auto key : mljson->keys()) {
            std::string ml_suffix = ":ml";
            if (suffix) {
                ml_suffix += std::to_string(suffix);
            }
            std::string multilib_id = repo_id + ml_suffix;
            auto multilib_chroot = std::regex_replace(baseurl_chroot, std::regex("\\$basearch"), key);
            baseurl = results_url + "/" + project_owner + "/" + project_dirname + "/" + multilib_chroot + "/";
            auto ml_repo = CoprRepoPart(multilib_id, name + " (" + key + ")", true, baseurl, gpgkey);
            ml_repo.update_from_json_opts(main_repo_json);
            ml_repo.update_from_json_opts(mljson->get_dict_item(key));
            repositories.push_back(ml_repo);
            suffix += 1;
        }
    }

    auto deps = json->get_dict_item("dependencies");

    // external dependencies
    for (size_t i = 0; i < deps->array_length(); i++) {
        auto dep = deps->get_array_item(i);

        auto type = dep->get_dict_item("type")->string();
        if (type == "copr") {
            auto repo = CoprRepoPart(dep, results_url, baseurl_chroot);
            repositories.push_back(repo);
        } else if (type == "external_baseurl") {
            auto repo = CoprRepoPart(dep, baseurl_chroot);
            repositories.push_back(repo);
        }
    }
}

void CoprRepo::save() {
    std::filesystem::path path = "/etc/yum.repos.d";
    path = "/tmp";
    path /= copr_id_to_copr_file(id);
    std::ofstream repofile;
    repofile.open(path);
    repofile << *this;
    repofile.close();

    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::group_read |
            std::filesystem::perms::others_read,
        std::filesystem::perm_options::add);
}

// bool partly_enabled = false
void CoprRepo::add_dnf_repo(libdnf::repo::RepoWeakPtr dnfRepo) {
    set_id_from_repo_id(dnfRepo->get_id());
    auto cp = CoprRepoPart(dnfRepo);
    this->enabled |= cp.enabled;
    this->has_external_deps |= cp.id.starts_with("coprdep:");
    if (fnmatch("copr:*:*:*:ml", cp.id.c_str(), 0) == 0)
        this->multilib = true;
    repositories.push_back(cp);
}

void CoprRepo::set_id_from_repo_id(const std::string & dnfRepoID) {
    // copr:copr.fedorainfracloud.org:group_codescan:csutils
    // copr.fedorainfracloud.org/@codescan/csutils
    if (id != "")
        return;
    auto copr_id = copr_id_from_repo_id(dnfRepoID);
    if (copr_id != "")
        id = copr_id;
}

std::ostream & operator<<(std::ostream & os, const class CoprRepo & copr_repo) {
    bool first = true;
    for (const auto & repo_part : copr_repo.repositories) {
        if (!first)
            os << std::endl;
        first = false;
        os << repo_part;
    }
    return os;
}

}  // namespace dnf5
