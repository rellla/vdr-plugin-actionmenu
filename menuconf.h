// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file menuconf.h
 * Configuration File actionmenu.conf
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#ifndef __ACTIONMENU_MENUCONF_H
#define __ACTIONMENU_MENUCONF_H

#include <istream>
#include <string>
#include <vector>

/**
 * Action of an entry
 */
enum class eEntryAction {
	Menu,     ///< open a submenu with the entries
	Svdrp,    ///< SVDRP command of a plugin
	Plugin,   ///< open the main menu of a plugin
	Keys,     ///< send keys to VDR
	Shell,    ///< run a shell command in the background
};

/**
 * Entry of a menu
 */
struct sMenuEntry {
	std::string key;                             ///< 0 - 9, Red, Green, Yellow, Blue or empty (only selectable in the list)
	std::string text;                            ///< text in the menu
	eEntryAction action = eEntryAction::Menu;    ///< what the entry does
	bool keepOpen = false;                       ///< the menu stays open after the action
	std::string plugin;                          ///< plugin of the SVDRP command or the plugin to open
	std::string command;                         ///< SVDRP command with its options, the keys or the shell command
	std::vector<sMenuEntry> entries;             ///< entries of a submenu
};

/**
 * The entries of actionmenu.conf
 */
class cMenuConfig {
public:
	bool Load(const std::string &);
	bool Parse(std::istream &);
	const std::vector<sMenuEntry> &Entries(void) const { return m_entries; }  ///< entries of the main menu
	const std::string &Error(void) const { return m_error; }                  ///< the error of Load() or Parse()

private:
	std::vector<sMenuEntry> m_entries;        ///< entries of the main menu
	std::string m_error;                      ///< description of the error

	bool Fail(int, const std::string &);
	bool ParseEntry(const std::string &, int, sMenuEntry &);
};

#endif
