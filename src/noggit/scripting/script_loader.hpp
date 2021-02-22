// This file is part of the Script Brushes extension of Noggit3 by TSWoW (https://github.com/tswow/)
// licensed under GNU General Public License (version 3).
#pragma once

#include <string>

namespace noggit
{
    namespace scripting
    {
        int load_scripts();

        void send_left_click();
        void send_left_hold();
        void send_left_release();

        void send_right_click();
        void send_right_hold();
        void send_right_release();

        int script_count();
        std::string selected_script_name();
        const std::string &get_script_name(int id);
        const std::string &get_script_display_name(int id);
        void select_script(int index);
        int get_selected_script();
    } // namespace scripting
} // namespace noggit