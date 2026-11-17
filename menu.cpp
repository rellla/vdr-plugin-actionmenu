// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file menu.cpp
 * The Action Menu
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <map>
#include <string>

#include <vdr/channels.h>
#include <vdr/device.h>
#include <vdr/i18n.h>
#include <vdr/interface.h>
#include <vdr/menu.h>
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
 * Replace the placeholders of a command with the current values
 *
 * @param command      the command
 *
 * @return the command with %channel% (number of the current channel), %channelid%
 *         (its ID) and %recording% (directory of the replayed recording)
 */
std::string Expand(const std::string &command)
{
	if (command.find('%') == std::string::npos)
		return command;

	std::map<std::string, std::string> values = { { "channel", "" }, { "channelid", "" }, { "recording", "" } };

	int number = cDevice::CurrentChannel();
	if (number) {
		values["channel"] = std::to_string(number);
		LOCK_CHANNELS_READ;
		if (const cChannel *channel = Channels->GetByNumber(number))
			values["channelid"] = *channel->GetChannelID().ToString();
	}

	if (const char *recording = cReplayControl::NowReplaying())
		values["recording"] = recording;

	return ExpandPlaceholders(command, values);
}

/**
 * Result of an SVDRP command
 */
enum class eSvdrpResult {
	Ok,
	NoPlugin,         ///< the plugin isn't loaded
	UnknownCommand,   ///< the plugin doesn't know the command
	Error,            ///< the plugin replied with an error (code 500 or higher)
};

/**
 * Run an SVDRP command of a plugin
 *
 * @param pluginName       the plugin
 * @param commandLine      the command with its options (with placeholders)
 * @param[out] reply       the first line of the reply
 *
 * @return the result
 */
eSvdrpResult Svdrp(const std::string &pluginName, const std::string &commandLine, std::string &reply)
{
	cPlugin *plugin = cPluginManager::GetPlugin(pluginName.c_str());
	if (!plugin)
		return eSvdrpResult::NoPlugin;

	std::string line = Expand(commandLine);
	std::string option;
	size_t end = line.find_first_of(" \t");
	std::string command = line.substr(0, end);
	if (end != std::string::npos && line.find_first_not_of(" \t", end) != std::string::npos)
		option = line.substr(line.find_first_not_of(" \t", end));

	int replyCode = 900;
	cString text = plugin->SVDRPCommand(command.c_str(), option.empty() ? nullptr : option.c_str(), replyCode);
	if (!*text)
		return eSvdrpResult::UnknownCommand;

	reply = *text;
	reply = reply.substr(0, reply.find('\n'));

	return replyCode >= 500 ? eSvdrpResult::Error : eSvdrpResult::Ok;
}

/**
 * Run a shell command and wait for its end
 *
 * @param command      the command (with placeholders)
 *
 * @return the output of the command (stdout)
 */
std::string ShellOutput(const std::string &command)
{
	std::string output;
	cPipe pipe;
	if (!pipe.Open(Expand(command).c_str(), "r")) {
		esyslog("actionmenu: can't run %s", command.c_str());
		return output;
	}

	char buffer[1024];
	size_t length;
	while ((length = fread(buffer, 1, sizeof(buffer), pipe)) > 0)
		output.append(buffer, length);
	pipe.Close();

	return output;
}

/**
 * Get the text of an entry with its state
 *
 * The state is the part of the first line of the reply or the output after the
 * last ": " (e.g. "on" of the reply "Passthrough: on"), otherwise the whole line.
 *
 * @param entry        the entry
 *
 * @return e.g. "Passthrough: on", the text without state or "text: ?", if the
 *         state can't be read
 */
std::string EntryText(const sMenuEntry &entry)
{
	if (entry.stateCommand.empty())
		return entry.text;

	std::string reply;
	bool ok;
	if (entry.stateAction == eEntryAction::Shell) {
		reply = ShellOutput(entry.stateCommand);
		reply = reply.substr(0, reply.find('\n'));
		ok = !reply.empty();
	} else
		ok = Svdrp(entry.statePlugin, entry.stateCommand, reply) == eSvdrpResult::Ok;

	if (!ok) {
		dsyslog("actionmenu: can't read the state of %s: %s", entry.text.c_str(), reply.c_str());
		return entry.text + ": ?";
	}

	size_t colon = reply.rfind(": ");
	return entry.text + ": " + (colon == std::string::npos ? reply : reply.substr(colon + 2));
}

/**
 * Get the entries, which are shown now
 *
 * @param entries      all entries of the menu
 *
 * @return the entries of the current section (without replay or during a replay)
 */
std::vector<sMenuEntry> CurrentEntries(const std::vector<sMenuEntry> &entries)
{
	cDevice *device = cDevice::PrimaryDevice();
	eMenuSection current = device->Replaying() && !device->Transferring() ? eMenuSection::Replay : eMenuSection::Live;

	std::vector<sMenuEntry> result;
	for (const sMenuEntry &entry : entries) {
		if (entry.section == eMenuSection::All || entry.section == current)
			result.push_back(entry);
	}

	return result;
}

/**
 * Entry in the list of the menu
 */
class cActionItem : public cOsdItem {
public:
	/** create the item of the entry, an entry without action can't be selected */
	explicit cActionItem(const sMenuEntry &entry) : m_entry(entry)
	{
		SetSelectable(entry.action != eEntryAction::Info);
		Update();
	}
	const sMenuEntry &Entry(void) const { return m_entry; }                        ///< the entry of the item

	/** true, if the state of the entry needs to be read again */
	bool RefreshDue(uint64_t now) const { return m_entry.refreshSeconds && now >= m_nextRefresh; }

	/** read the state of the entry again */
	void Update(void)
	{
		m_nextRefresh = cTimeMs::Now() + m_entry.refreshSeconds * 1000;
		if (IsHeader())
			SetText(m_entry.text.c_str());
		else
			SetText(cString::sprintf("%s\t%s%s", m_entry.key.c_str(), EntryText(m_entry).c_str(),
			        m_entry.action == eEntryAction::Menu ? " ..." : ""));
	}

	/** true, if the entry is a header, which starts at the left border */
	bool IsHeader(void) const { return m_entry.action == eEntryAction::Info && m_entry.stateCommand.empty(); }

private:
	const sMenuEntry &m_entry;        ///< the entry of the item
	uint64_t m_nextRefresh = 0;       ///< time to read the state again (see sMenuEntry::refreshSeconds)
};

} // namespace

/**
 * Create the menu
 *
 * @param title        title of the menu
 * @param entries      the entries, the menu shows the ones of the current section
 */
cActionMenu::cActionMenu(const char *title, const std::vector<sMenuEntry> &entries)
	: cOsdMenu(title, 3),
	  m_entries(CurrentEntries(entries))
{
	for (const sMenuEntry &entry : m_entries) {
		int color = ColorIndex(entry.key);
		if (color >= 0)
			m_colorEntries[color] = &entry;
		else
			Add(new cActionItem(entry));
		m_hasStates |= !entry.stateCommand.empty();
		m_hasRefresh |= entry.refreshSeconds > 0;
	}

	SetHelpTexts();
}

/**
 * Set the texts of the color buttons with the states of their entries
 */
void cActionMenu::SetHelpTexts(void)
{
	const char *help[4] = {};
	for (int i = 0; i < 4; i++) {
		if (m_colorEntries[i]) {
			m_helpTexts[i] = EntryText(*m_colorEntries[i]);
			help[i] = m_helpTexts[i].c_str();
			m_nextHelpRefresh[i] = cTimeMs::Now() + m_colorEntries[i]->refreshSeconds * 1000;
		}
	}
	SetHelp(help[0], help[1], help[2], help[3]);
}

/**
 * Read the states again, which are due to be refreshed
 */
void cActionMenu::RefreshStates(void)
{
	uint64_t now = cTimeMs::Now();

	for (cOsdItem *item = First(); item; item = Next(item)) {
		cActionItem *actionItem = static_cast<cActionItem *>(item);
		if (actionItem->RefreshDue(now)) {
			actionItem->Update();
			DisplayItem(item);
		}
	}

	for (int i = 0; i < 4; i++) {
		if (m_colorEntries[i] && m_colorEntries[i]->refreshSeconds && now >= m_nextHelpRefresh[i]) {
			SetHelpTexts();
			break;
		}
	}
}

/**
 * Read the states of the entries again and show them
 */
void cActionMenu::UpdateStates(void)
{
	if (!m_hasStates)
		return;

	for (cOsdItem *item = First(); item; item = Next(item))
		static_cast<cActionItem *>(item)->Update();
	SetHelpTexts();
	Display();
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
	// VDR calls it without a key at least once per second
	if (key == kNone && m_hasRefresh && !HasSubMenu())
		RefreshStates();

	bool hadSubMenu = HasSubMenu();
	eOSState state = cOsdMenu::ProcessKey(key);
	if (hadSubMenu) {
		// the actions of the submenu may have changed the states
		if (!HasSubMenu())
			UpdateStates();
		return state;
	}
	if (state != osUnknown)
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
	if (entry.confirm && !Interface->Confirm(cString::sprintf("%s?", entry.text.c_str())))
		return osContinue;

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
			SystemExec(Expand(entry.command).c_str(), true);
			if (!entry.keepOpen)
				return osEnd;
			UpdateStates();
			return osContinue;
		case eEntryAction::Channel:
			return RunChannel(entry);
		case eEntryAction::Output:
			return RunOutput(entry);
		case eEntryAction::Info:
			// the key of the entry reads the states again
			UpdateStates();
			return osContinue;
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
	std::string text;
	switch (Svdrp(entry.plugin, entry.command, text)) {
		case eSvdrpResult::NoPlugin:
			return Error(cString::sprintf(tr("The plugin %s isn't loaded"), entry.plugin.c_str()));
		case eSvdrpResult::UnknownCommand:
			return Error(cString::sprintf(tr("The plugin %s doesn't know the command %s"), entry.plugin.c_str(), entry.command.c_str()));
		case eSvdrpResult::Error:
			return Error(text.c_str());
		case eSvdrpResult::Ok:
			break;
	}

	if (entry.keepOpen) {
		UpdateStates();
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
 * Switch to a channel
 *
 * @param entry        the entry with the number or the ID of the channel
 *
 * @return osEnd, if the channel is switched, otherwise osContinue
 */
eOSState cActionMenu::RunChannel(const sMenuEntry &entry)
{
	const char *channel = entry.command.c_str();

	{
		LOCK_CHANNELS_READ;
		const cChannel *ch = isnumber(channel) ? Channels->GetByNumber(atoi(channel))
		                                       : Channels->GetByChannelID(tChannelID::FromString(channel));
		if (ch)
			return cDevice::PrimaryDevice()->SwitchChannel(ch, true) ? osEnd : osContinue;
	}

	// not with the lock of the channels, the message waits
	return Error(cString::sprintf(tr("Channel %s not found"), channel));
}

/**
 * Run a shell command and show its output
 *
 * VDR waits for the end of the command, like with the commands of commands.conf.
 *
 * @param entry        the entry
 *
 * @return osContinue, the output is shown in a submenu
 */
eOSState cActionMenu::RunOutput(const sMenuEntry &entry)
{
	Skins.Message(mtStatus, cString::sprintf("%s...", entry.text.c_str()));
	std::string output = ShellOutput(entry.command);
	Skins.Message(mtStatus, nullptr);

	if (output.empty()) {
		Skins.Message(mtInfo, tr("The command printed nothing"));
		return osContinue;
	}

	return AddSubMenu(new cMenuText(entry.text.c_str(), output.c_str(), fontFix));
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
