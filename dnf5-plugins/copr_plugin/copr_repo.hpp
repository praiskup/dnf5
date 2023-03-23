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

#ifndef DNF5_COMMANDS_COPR_REPO_HPP
#define DNF5_COMMANDS_COPR_REPO_HPP

#include "copr.hpp"
#include "json.hpp"

#include "libdnf/base/base.hpp"


namespace dnf5 {


void available_chroots_error(const std::set<std::string> & chroots, const std::string & chroot = "");


class CoprRepoPart {
    // TODO: make attributes private
public:
    CoprRepoPart(){};
    explicit CoprRepoPart(libdnf::repo::RepoWeakPtr dnfRepo) : id(dnfRepo->get_id()), enabled(dnfRepo->is_enabled()){};
    explicit CoprRepoPart(
        const std::string & id,
        const std::string & name,
        bool enabled,
        const std::string & baseurl,
        const std::string & gpgkey)
        : id(id),
          enabled(enabled),
          baseurl(baseurl),
          name(name),
          gpgkey(gpgkey) {}

    explicit CoprRepoPart(const std::unique_ptr<Json> & json_dep, const std::string & chroot) {
        update_from_json_opts(json_dep);
        auto data = json_dep->get_dict_item("data");
        auto pattern = data->get_dict_item("pattern")->string();
        baseurl = std::regex_replace(pattern, std::regex("\\$chroot"), chroot);
    }

    explicit CoprRepoPart(
        const std::unique_ptr<Json> & json_dep, const std::string & results_url, const std::string & chroot) {
        update_from_json_opts(json_dep);
        auto info = json_dep->get_dict_item("data");
        auto owner = info->get_dict_item("owner")->string();
        auto project = info->get_dict_item("projectname")->string();
        set_copr_pub_key(results_url, owner, project);
        set_copr_baseurl(results_url, owner, project, chroot);
    }

    std::string id;
    bool enabled;
    std::string baseurl;

    friend std::ostream & operator<<(std::ostream & os, const class CoprRepoPart & repo) {
        os << "[" << repo.id << "]" << std::endl;
        os << "name=" << repo.name << std::endl;
        os << "baseurl=" << repo.baseurl << std::endl;
        os << "type=rpm-md" << std::endl;
        os << "skip_if_unavailable=True" << std::endl;
        os << "gpgcheck=" << ((repo.gpgkey == "") ? 0 : 1) << std::endl;
        if (repo.gpgkey != "")
            os << "gpgkey=" << repo.gpgkey << std::endl;
        os << "repo_gpgcheck=0" << std::endl;
        if (repo.cost)
            os << "cost=" << repo.cost << std::endl;
        os << "enabled=1" << std::endl;
        os << "enabled_metadata=1" << std::endl;
        if (repo.priority != 99)
            os << "priority=" << repo.priority << std::endl;
        if (repo.module_hotfixes)
            os << "module_hotfixes=1" << std::endl;
        return os;
    }

    void update_from_json_opts(const std::unique_ptr<Json> & json) {
        if (!json->has_key("opts"))
            return;
        auto opts = json->get_dict_item("opts");
        for (const auto & key : opts->keys()) {
            auto val = opts->get_dict_item(key);
            if (key == "cost")
                cost = std::stoi(val->string());
            else if (key == "priority")
                priority = std::stoi(val->string());
            else if (key == "module_hotfixes")
                module_hotfixes = val->boolean();
            else if (key == "id")
                id = val->string();
            else if (key == "name")
                name = val->string();
        }
    }

    void set_copr_pub_key(const std::string & results_url, const std::string & owner, const std::string & projectname) {
        gpgkey = results_url + "/" + owner + "/" + projectname + "/pubkey.gpg";
    }

    void set_copr_baseurl(
        const std::string & results_url,
        const std::string & owner,
        const std::string & dirname,
        const std::string & chroot) {
        baseurl = results_url + "/" + owner + "/" + dirname + "/" + chroot + "/";
    }

private:
    std::string name;
    std::string gpgkey;
    int priority = 99;
    int cost = 0;
    bool module_hotfixes = false;
};


class CoprRepo {
public:
    CoprRepo();

    explicit CoprRepo(
        libdnf::Base & base,
        const std::unique_ptr<CoprConfig> & copr_config,
        const std::string & hubspec,
        const std::string & project_owner,
        const std::string & project_dirname,
        const std::string & selected_chroot);

    void save();

    friend std::ostream & operator<<(std::ostream & os, const class CoprRepo & copr_repo);

    std::string id;         /// the id, groups are like '@GROUPNAME'
    std::string repo_file;  /// full path
    std::vector<CoprRepoPart> repositories;
    bool enabled;
    bool has_external_deps;
    bool multilib;

    // bool partly_enabled = false
    void add_dnf_repo(libdnf::repo::RepoWeakPtr dnfRepo);

private:
    void set_id_from_repo_id(const std::string & dnfRepoID);
};

using CoprRepoCallback = std::function<void(const CoprRepo &)>;


}  // namespace dnf5

#endif  // DNF5_COMMANDS_COPR_REPO_HPP
