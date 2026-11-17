// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file menuconf.cpp
 * Configuration File actionmenu.conf
 *
 * Every line is an entry of the menu or the end of a submenu:
 *
 *     <key> "<text>" [<state>] <action>
 *     <key> "<text>" [<state>] {
 *         <entries of the submenu>
 *     }
 *
 * The optional state in brackets is "[svdrp <plugin> <command>]" or
 * "[shell <command>]", the reply or the output is shown as state of the entry.
 * "[refresh [<seconds>]]" after it reads the state again periodically.
 * A '?' directly after the text asks before the action. An entry with a state
 * doesn't need an action, it only shows the state then. An entry without key,
 * action and state is a header, which structures the menu.
 * The key is 0 - 9, Red, Green, Yellow, Blue or "-" (no key, only selectable in
 * the list). The action is one of
 *
 *     svdrp <plugin> <command> [<options>]
 *     plugin <plugin>
 *     key <key> [<key> ...]
 *     shell <command>
 *     channel <number or channel ID>
 *     output <command>
 *
 * A chain runs several actions one after the other:
 *
 *     <key> "<text>" (
 *         <action>
 *         ...
 *     )
 *
 * The lines [live], [replay] and [all] start a section of the menu, whose
 * entries are only shown without replay, during a replay or always (the
 * default). A section ends with the next one or the end of the (sub)menu.
 *
 * "svdrp!" and "shell!" keep the menu open. The commands may contain the
 * placeholders %channel%, %channelid% and %recording% (see ExpandPlaceholders()),
 * in shell commands they are quoted (see ShellQuote()). Empty lines and lines
 * starting with '#' are ignored.
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <cstdlib>
#include <fstream>

#include <strings.h>

#include "menuconf.h"

namespace {

const char *const WHITESPACE = " \t\r";
const int DEFAULT_REFRESH_SECONDS = 2;   ///< interval of "[refresh]" without seconds

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

/**
 * Find the ']', which closes the '[' at the beginning of a text
 *
 * The brackets in between are counted, so a command may contain pairs of them
 * (e.g. "[ -f file ]" or "sed 's/[^0-9]//'").
 *
 * @param s            the text, which starts with '['
 *
 * @return position of the ']' or std::string::npos
 */
size_t ClosingBracket(const std::string &s)
{
	int depth = 0;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '[')
			depth++;
		else if (s[i] == ']' && --depth == 0)
			return i;
	}

	return std::string::npos;
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

	// the menus, which are open at the current line
	struct sOpenMenu {
		std::vector<sMenuEntry> *entries;   ///< entries of the menu
		int line;                           ///< line of the start of the menu
		eMenuSection section;               ///< the current section of the menu
	};
	std::vector<sOpenMenu> menus = { { &m_entries, 0, eMenuSection::All } };
	sMenuEntry *chain = nullptr;   // the chain, whose actions are read
	int chainLine = 0;          // line of the start of the chain

	std::string text;
	for (int line = 1; std::getline(in, text); line++) {
		text = Trim(text);
		if (text.empty() || text[0] == '#')
			continue;

		if (chain) {
			if (text == ")") {
				if (chain->actions.empty())
					return Fail(chainLine, "the chain has no actions");
				chain = nullptr;
				continue;
			}

			// these actions leave the menu
			if (!chain->actions.empty()) {
				eEntryAction last = chain->actions.back().action;
				if (last == eEntryAction::Plugin || last == eEntryAction::Keys || last == eEntryAction::Output)
					return Fail(line, "plugin, key and output can only be the last action of a chain");
			}

			sMenuEntry action;
			action.text = chain->text;
			if (!ParseAction(text, line, action))
				return false;
			chain->actions.push_back(action);
			continue;
		}

		if (text == "}") {
			if (menus.size() == 1)
				return Fail(line, "'}' without a submenu");
			menus.pop_back();
			continue;
		}

		if (text[0] == '[') {
			if (!strcasecmp(text.c_str(), "[all]"))
				menus.back().section = eMenuSection::All;
			else if (!strcasecmp(text.c_str(), "[live]"))
				menus.back().section = eMenuSection::Live;
			else if (!strcasecmp(text.c_str(), "[replay]"))
				menus.back().section = eMenuSection::Replay;
			else
				return Fail(line, "unknown section " + text + " ([all], [live] or [replay])");
			continue;
		}

		sMenuEntry entry;
		if (!ParseEntry(text, line, entry))
			return false;
		entry.section = menus.back().section;

		std::vector<sMenuEntry> &menu = *menus.back().entries;
		if (!entry.key.empty()) {
			for (const sMenuEntry &other : menu) {
				// a key can be used once without replay and once during a replay
				bool shownTogether = other.section == eMenuSection::All || entry.section == eMenuSection::All
				                     || other.section == entry.section;
				if (other.key == entry.key && shownTogether)
					return Fail(line, "the key " + entry.key + " is already used in this menu");
			}
		}

		menu.push_back(entry);
		// the entries of the menu don't change, until its submenu or chain is closed
		if (entry.action == eEntryAction::Menu)
			menus.push_back({ &menu.back().entries, line, eMenuSection::All });
		if (entry.action == eEntryAction::Chain) {
			chain = &menu.back();
			chainLine = line;
		}
	}

	if (chain)
		return Fail(chainLine, "the chain isn't closed with ')'");
	if (menus.size() > 1)
		return Fail(menus.back().line, "the submenu isn't closed with '}'");

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

	entry.confirm = rest.compare(end + 1, 1, "?") == 0;
	std::string action = Trim(rest.substr(end + (entry.confirm ? 2 : 1)));
	if (!strncasecmp(action.c_str(), "[refresh", 8))
		return Fail(line, "refresh needs a state before it");

	if (!action.empty() && action[0] == '[') {
		size_t close = ClosingBracket(action);
		if (close == std::string::npos)
			return Fail(line, "the closing ']' of the state is missing");
		std::string command;
		word = FirstWord(Trim(action.substr(1, close - 1)), command);
		if (!strcasecmp(word.c_str(), "svdrp")) {
			entry.stateAction = eEntryAction::Svdrp;
			entry.statePlugin = FirstWord(command, entry.stateCommand);
		} else if (!strcasecmp(word.c_str(), "shell")) {
			entry.stateAction = eEntryAction::Shell;
			entry.stateCommand = command;
		}
		if (entry.stateCommand.empty())
			return Fail(line, "the state needs svdrp <plugin> <command> or shell <command>");
		action = Trim(action.substr(close + 1));

		if (!strncasecmp(action.c_str(), "[refresh", 8)) {
			close = action.find(']');
			if (close == std::string::npos)
				return Fail(line, "the closing ']' of refresh is missing");
			std::string seconds = Trim(action.substr(8, close - 8));
			entry.refreshSeconds = seconds.empty() ? DEFAULT_REFRESH_SECONDS : atoi(seconds.c_str());
			if (entry.refreshSeconds < 1 || entry.refreshSeconds > 3600 || seconds.find_first_not_of("0123456789") != std::string::npos)
				return Fail(line, "refresh needs the seconds (1 - 3600)");
			action = Trim(action.substr(close + 1));
		}
	}

	if (action == "{") {
		entry.action = eEntryAction::Menu;
		if (entry.confirm)
			return Fail(line, "a submenu doesn't ask before it is opened");
		return true;
	}

	if (action.empty() && (!entry.stateCommand.empty() || entry.key.empty())) {
		entry.action = eEntryAction::Info;
		if (entry.confirm)
			return Fail(line, "an entry without action doesn't ask");
		return true;
	}

	if (action == "(") {
		entry.action = eEntryAction::Chain;
		return true;
	}

	return ParseAction(action, line, entry);
}

/**
 * Read an action
 *
 * @param action       the action with its arguments
 * @param line         number of the line
 * @param[out] entry   the entry, which gets the action
 *
 * @return false on errors
 */
bool cMenuConfig::ParseAction(const std::string &action, int line, sMenuEntry &entry)
{
	std::string rest;
	std::string word = FirstWord(action, rest);
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
	} else if (!strcasecmp(word.c_str(), "channel")) {
		entry.action = eEntryAction::Channel;
		entry.command = rest;
		if (entry.command.empty() || entry.command.find_first_of(WHITESPACE) != std::string::npos)
			return Fail(line, "channel needs the number or the ID of a channel");
	} else if (!strcasecmp(word.c_str(), "output")) {
		entry.action = eEntryAction::Output;
		entry.command = rest;
		if (entry.command.empty())
			return Fail(line, "output needs a command");
	} else if (word.empty()) {
		return Fail(line, "the action is missing");
	} else {
		return Fail(line, "unknown action '" + word + "' (svdrp, plugin, key, shell, channel, output, { or ()");
	}

	// the other actions close the menu to let VDR or the plugin handle it
	if (entry.keepOpen && entry.action != eEntryAction::Svdrp && entry.action != eEntryAction::Shell)
		return Fail(line, word + "! isn't possible, only svdrp! and shell! keep the menu open");

	return true;
}

/**
 * Replace the placeholders of a command
 *
 * A placeholder is a name between two '%', e.g. %channel%.
 *
 * @param text         the command
 * @param values       the values of the placeholders (the names without '%')
 *
 * @return the command with the values, an unknown placeholder stays as it is
 */
std::string ExpandPlaceholders(const std::string &text, const std::map<std::string, std::string> &values)
{
	std::string result;
	size_t pos = 0;
	while (pos < text.size()) {
		size_t start = text.find('%', pos);
		size_t end = start == std::string::npos ? std::string::npos : text.find('%', start + 1);
		if (end == std::string::npos) {
			result += text.substr(pos);
			break;
		}

		result += text.substr(pos, start - pos);
		auto value = values.find(text.substr(start + 1, end - start - 1));
		if (value == values.end()) {
			// the second '%' may start a placeholder
			result += '%';
			pos = start + 1;
		} else {
			result += value->second;
			pos = end + 1;
		}
	}

	return result;
}

/**
 * Quote a value for the shell
 *
 * @param value        the value of a placeholder
 *
 * @return the value in single quotes, so the shell doesn't interpret it (a
 *         single quote in it becomes '\'')
 */
std::string ShellQuote(const std::string &value)
{
	std::string result = "'";
	for (char c : value) {
		if (c == '\'')
			result += "'\\''";
		else
			result += c;
	}

	return result + "'";
}
