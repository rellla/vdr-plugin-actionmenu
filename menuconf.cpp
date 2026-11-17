// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file menuconf.cpp
 * Configuration File actionmenu.conf
 *
 * Every line is an entry of the menu or the end of a submenu:
 *
 *     <key> "<text>" <action>
 *     <key> "<text>" {
 *         <entries of the submenu>
 *     }
 *
 * The key is 0 - 9, Red, Green, Yellow, Blue or "-" (no key, only selectable in
 * the list). The action is one of
 *
 *     svdrp <plugin> <command> [<options>]
 *     plugin <plugin>
 *     key <key> [<key> ...]
 *     shell <command>
 *
 * "svdrp!" and "shell!" keep the menu open. Empty lines and lines starting with
 * '#' are ignored.
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <fstream>

#include <strings.h>

#include "menuconf.h"

namespace {

const char *const WHITESPACE = " \t\r";

/** Remove the whitespace at the beginning and the end */
std::string Trim(const std::string &s)
{
	size_t first = s.find_first_not_of(WHITESPACE);
	if (first == std::string::npos)
		return "";

	return s.substr(first, s.find_last_not_of(WHITESPACE) - first + 1);
}

/**
 * Split the first word off
 *
 * @param s            the text
 * @param[out] rest    the text after the word
 *
 * @return the word
 */
std::string FirstWord(const std::string &s, std::string &rest)
{
	size_t end = s.find_first_of(WHITESPACE);
	rest = end == std::string::npos ? "" : Trim(s.substr(end));

	return s.substr(0, end);
}

/**
 * Get the name of a key, as it is stored in sMenuEntry::key
 *
 * @param word         key of the configuration file
 * @param[out] key     0 - 9, Red, Green, Yellow, Blue or empty for "-"
 *
 * @return false, if the word isn't a key
 */
bool KeyName(const std::string &word, std::string &key)
{
	static const char *const colors[] = { "Red", "Green", "Yellow", "Blue" };

	if (word == "-") {
		key.clear();
		return true;
	}
	if (word.size() == 1 && word[0] >= '0' && word[0] <= '9') {
		key = word;
		return true;
	}
	for (const char *color : colors) {
		if (!strcasecmp(word.c_str(), color)) {
			key = color;
			return true;
		}
	}

	return false;
}

} // namespace

/**
 * Read the entries from a file
 *
 * @param fileName     the file
 *
 * @return false on errors (see Error())
 */
bool cMenuConfig::Load(const std::string &fileName)
{
	std::ifstream file(fileName);
	if (!file) {
		m_entries.clear();
		m_error = "can't open " + fileName;
		return false;
	}

	return Parse(file);
}

/**
 * Read the entries
 *
 * @param in           the content of actionmenu.conf
 *
 * @return false on errors (see Error()), there are no entries then
 */
bool cMenuConfig::Parse(std::istream &in)
{
	m_entries.clear();
	m_error.clear();

	// the menus, which are open at the current line, with the line of their start
	std::vector<std::pair<std::vector<sMenuEntry> *, int>> menus = { { &m_entries, 0 } };

	std::string text;
	for (int line = 1; std::getline(in, text); line++) {
		text = Trim(text);
		if (text.empty() || text[0] == '#')
			continue;

		if (text == "}") {
			if (menus.size() == 1)
				return Fail(line, "'}' without a submenu");
			menus.pop_back();
			continue;
		}

		sMenuEntry entry;
		if (!ParseEntry(text, line, entry))
			return false;

		std::vector<sMenuEntry> &menu = *menus.back().first;
		if (!entry.key.empty()) {
			for (const sMenuEntry &other : menu) {
				if (other.key == entry.key)
					return Fail(line, "the key " + entry.key + " is already used in this menu");
			}
		}

		menu.push_back(entry);
		// the entries of the menu don't change, until its submenu is closed
		if (entry.action == eEntryAction::Menu)
			menus.push_back({ &menu.back().entries, line });
	}

	if (menus.size() > 1)
		return Fail(menus.back().second, "the submenu isn't closed with '}'");

	return true;
}

/**
 * Store an error, all entries are dropped
 *
 * @param line         line of the error
 * @param error        description of the error
 *
 * @return false
 */
bool cMenuConfig::Fail(int line, const std::string &error)
{
	m_entries.clear();
	m_error = "line " + std::to_string(line) + ": " + error;

	return false;
}

/**
 * Read an entry
 *
 * @param text         the line without the whitespace around it
 * @param line         number of the line
 * @param[out] entry   the entry
 *
 * @return false on errors
 */
bool cMenuConfig::ParseEntry(const std::string &text, int line, sMenuEntry &entry)
{
	std::string rest;
	std::string word = FirstWord(text, rest);
	if (!KeyName(word, entry.key))
		return Fail(line, "unknown key '" + word + "' (0 - 9, Red, Green, Yellow, Blue or -)");

	if (rest.empty() || rest[0] != '"')
		return Fail(line, "the text in quotes is missing");
	size_t end = rest.find('"', 1);
	if (end == std::string::npos)
		return Fail(line, "the closing quote of the text is missing");
	entry.text = rest.substr(1, end - 1);
	if (entry.text.empty())
		return Fail(line, "the text is empty");

	std::string action = Trim(rest.substr(end + 1));
	if (action == "{") {
		entry.action = eEntryAction::Menu;
		return true;
	}

	word = FirstWord(action, rest);
	entry.keepOpen = !word.empty() && word.back() == '!';
	if (entry.keepOpen)
		word.pop_back();

	if (!strcasecmp(word.c_str(), "svdrp")) {
		entry.action = eEntryAction::Svdrp;
		entry.plugin = FirstWord(rest, entry.command);
		if (entry.command.empty())
			return Fail(line, "svdrp needs a plugin and a command");
	} else if (!strcasecmp(word.c_str(), "plugin")) {
		entry.action = eEntryAction::Plugin;
		entry.plugin = rest;
		if (entry.plugin.empty() || entry.plugin.find_first_of(WHITESPACE) != std::string::npos)
			return Fail(line, "plugin needs the name of a plugin");
	} else if (!strcasecmp(word.c_str(), "key")) {
		entry.action = eEntryAction::Keys;
		entry.command = rest;
		if (entry.command.empty())
			return Fail(line, "key needs at least one key");
	} else if (!strcasecmp(word.c_str(), "shell")) {
		entry.action = eEntryAction::Shell;
		entry.command = rest;
		if (entry.command.empty())
			return Fail(line, "shell needs a command");
	} else if (word.empty()) {
		return Fail(line, "the action is missing");
	} else {
		return Fail(line, "unknown action '" + word + "' (svdrp, plugin, key, shell or {)");
	}

	// the other actions close the menu to let VDR or the plugin handle it
	if (entry.keepOpen && entry.action != eEntryAction::Svdrp && entry.action != eEntryAction::Shell)
		return Fail(line, word + "! isn't possible, only svdrp! and shell! keep the menu open");

	return true;
}
