// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file menu.cpp
 * The Action Menu
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <string>

#include <vdr/i18n.h>
#include <vdr/plugin.h>
#include <vdr/remote.h>
#include <vdr/skins.h>
#include <vdr/thread.h>
#include <vdr/tools.h>

#include "menu.h"

namespace {

const char *const COLORS[] = { "Red", "Green", "Yellow", "Blue" };

/** Index of a color key in COLORS (-1 for the other keys) */
int ColorIndex(const std::string &key)
{
	for (int i = 0; i < 4; i++) {
		if (key == COLORS[i])
			return i;
	}

	return -1;
}

/**
 * Entry in the list of the menu
 */
class cActionItem : public cOsdItem {
public:
	/** create the item of the entry */
	explicit cActionItem(const sMenuEntry &entry)
		: cOsdItem(cString::sprintf("%s\t%s%s", entry.key.c_str(), entry.text.c_str(),
		           entry.action == eEntryAction::Menu ? " ..." : "")),
		  m_entry(entry)
	{}
	const sMenuEntry &Entry(void) const { return m_entry; }  ///< the entry of the item

private:
	const sMenuEntry &m_entry;  ///< the entry of the item
};

} // namespace

/**
 * Create the menu
 *
 * @param title        title of the menu
 * @param entries      the entries
 */
cActionMenu::cActionMenu(const char *title, const std::vector<sMenuEntry> &entries)
	: cOsdMenu(title, 3),
	  m_entries(entries)
{
	for (const sMenuEntry &entry : m_entries) {
		int color = ColorIndex(entry.key);
		if (color >= 0)
			m_colorEntries[color] = &entry;
		else
			Add(new cActionItem(entry));
	}

	const char *help[4] = {};
	for (int i = 0; i < 4; i++)
		help[i] = m_colorEntries[i] ? m_colorEntries[i]->text.c_str() : nullptr;
	SetHelp(help[0], help[1], help[2], help[3]);
}

/**
 * Handle a key
 *
 * @param key          the key
 *
 * @return the state of the menu
 */
eOSState cActionMenu::ProcessKey(eKeys key)
{
	bool hadSubMenu = HasSubMenu();
	eOSState state = cOsdMenu::ProcessKey(key);
	if (hadSubMenu || state != osUnknown)
		return state;

	if (key == kOk) {
		cActionItem *item = dynamic_cast<cActionItem *>(Get(Current()));
		return item ? Run(item->Entry()) : osContinue;
	}

	const sMenuEntry *entry = Find(key);
	return entry ? Run(*entry) : state;
}

/**
 * Find the entry of a key
 *
 * @param key          the key
 *
 * @return the entry (nullptr, if no entry has the key)
 */
const sMenuEntry *cActionMenu::Find(eKeys key) const
{
	if (key >= kRed && key <= kBlue)
		return m_colorEntries[key - kRed];

	if (key >= k0 && key <= k9) {
		std::string digit(1, '0' + (key - k0));
		for (const sMenuEntry &entry : m_entries) {
			if (entry.key == digit)
				return &entry;
		}
	}

	return nullptr;
}

/**
 * Run the action of an entry
 *
 * @param entry        the entry
 *
 * @return osEnd, if the menu is closed, otherwise osContinue
 */
eOSState cActionMenu::Run(const sMenuEntry &entry)
{
	dsyslog("actionmenu: %s", entry.text.c_str());

	switch (entry.action) {
		case eEntryAction::Menu:
			return AddSubMenu(new cActionMenu(entry.text.c_str(), entry.entries));
		case eEntryAction::Svdrp:
			return RunSvdrp(entry);
		case eEntryAction::Plugin:
			return RunPlugin(entry);
		case eEntryAction::Keys:
			return RunKeys(entry);
		case eEntryAction::Shell:
			SystemExec(entry.command.c_str(), true);
			return entry.keepOpen ? osContinue : osEnd;
	}

	return osContinue;
}

/**
 * Run an SVDRP command of a plugin
 *
 * The reply is shown in the status line of the menu, if it stays open,
 * otherwise as a message after the menu is closed.
 *
 * @param entry        the entry
 *
 * @return osEnd, if the menu is closed, otherwise osContinue
 */
eOSState cActionMenu::RunSvdrp(const sMenuEntry &entry)
{
	cPlugin *plugin = cPluginManager::GetPlugin(entry.plugin.c_str());
	if (!plugin)
		return Error(cString::sprintf(tr("The plugin %s isn't loaded"), entry.plugin.c_str()));

	std::string option;
	size_t end = entry.command.find_first_of(" \t");
	std::string command = entry.command.substr(0, end);
	if (end != std::string::npos)
		option = entry.command.substr(entry.command.find_first_not_of(" \t", end));

	int replyCode = 900;
	cString reply = plugin->SVDRPCommand(command.c_str(), option.empty() ? nullptr : option.c_str(), replyCode);
	if (!*reply)
		return Error(cString::sprintf(tr("The plugin %s doesn't know the command %s"), entry.plugin.c_str(), command.c_str()));

	// show only the first line of the reply
	std::string text = *reply;
	text = text.substr(0, text.find('\n'));

	if (replyCode >= 500)
		return Error(text.c_str());

	if (entry.keepOpen) {
		SetStatus(text.c_str());
		return osContinue;
	}

	if (!text.empty())
		Skins.QueueMessage(mtInfo, text.c_str());

	return osEnd;
}

/**
 * Send keys to VDR, they are handled after the menu is closed
 *
 * @param entry        the entry
 *
 * @return osEnd, if the keys are sent, otherwise osContinue
 */
eOSState cActionMenu::RunKeys(const sMenuEntry &entry)
{
	std::vector<eKeys> keys;
	const std::string &names = entry.command;
	size_t pos = 0;
	while ((pos = names.find_first_not_of(" \t", pos)) != std::string::npos) {
		size_t end = names.find_first_of(" \t", pos);
		std::string name = names.substr(pos, end - pos);
		eKeys key = cKey::FromString(name.c_str());
		if (key == kNone)
			return Error(cString::sprintf(tr("Unknown key %s"), name.c_str()));
		keys.push_back(key);
		pos = end;
	}

	for (eKeys key : keys)
		cRemote::Put(key);

	return osEnd;
}

/**
 * Open the main menu of a plugin, after the menu is closed
 *
 * @param entry        the entry
 *
 * @return osEnd, if the plugin is called, otherwise osContinue
 */
eOSState cActionMenu::RunPlugin(const sMenuEntry &entry)
{
	if (!cPluginManager::GetPlugin(entry.plugin.c_str()))
		return Error(cString::sprintf(tr("The plugin %s isn't loaded"), entry.plugin.c_str()));

	if (!cRemote::CallPlugin(entry.plugin.c_str()))
		return Error(cString::sprintf(tr("The plugin %s can't be opened now"), entry.plugin.c_str()));

	return osEnd;
}

/**
 * Show an error, the menu stays open
 *
 * @param text         the error
 *
 * @return osContinue
 */
eOSState cActionMenu::Error(const char *text)
{
	Skins.Message(mtError, text);

	return osContinue;
}
