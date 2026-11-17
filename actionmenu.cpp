// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file actionmenu.cpp
 * VDR Plugin 'actionmenu'
 *
 * A menu with actions for the keys of the remote control, which are configured
 * in actionmenu.conf. It is opened with a key macro (e.g. "User1 @actionmenu").
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <getopt.h>
#include <strings.h>

#include <string>

#include <vdr/i18n.h>
#include <vdr/menuitems.h>
#include <vdr/plugin.h>
#include <vdr/remote.h>
#include <vdr/skins.h>

#include "menuconf.h"
#include "menu.h"

static const char *VERSION        = "0.2.0";
static const char *DESCRIPTION    = trNOOP("Menu with configurable actions for the keys of the remote control");
static const char *MAINMENUENTRY  = trNOOP("Action menu");

/**
 * Setup page
 */
class cMenuSetupActionmenu : public cMenuSetupPage {
public:
	/** create the setup page */
	explicit cMenuSetupActionmenu(int &hideMainMenuEntry)
		: m_hideMainMenuEntry(hideMainMenuEntry),
		  m_newHideMainMenuEntry(hideMainMenuEntry)
	{
		Add(new cMenuEditBoolItem(tr("Hide main menu entry"), &m_newHideMainMenuEntry));
	}

protected:
	/** store the settings */
	virtual void Store(void)
	{
		m_hideMainMenuEntry = m_newHideMainMenuEntry;
		SetupStore("HideMainMenuEntry", m_hideMainMenuEntry);
	}

private:
	int &m_hideMainMenuEntry;      ///< setting of the plugin
	int m_newHideMainMenuEntry;    ///< edited setting
};

/**
 * The plugin
 */
class cPluginActionmenu : public cPlugin {
public:
	virtual const char *Version(void) { return VERSION; }
	virtual const char *Description(void) { return tr(DESCRIPTION); }
	virtual const char *CommandLineHelp(void);
	virtual bool ProcessArgs(int, char *[]);
	/** the main menu entry, if it isn't hidden */
	virtual const char *MainMenuEntry(void) { return m_hideMainMenuEntry ? nullptr : tr(MAINMENUENTRY); }
	virtual cOsdObject *MainMenuAction(void);
	virtual cMenuSetupPage *SetupMenu(void) { return new cMenuSetupActionmenu(m_hideMainMenuEntry); }  ///< create the setup page
	virtual bool SetupParse(const char *, const char *);

private:
	std::string m_configFile;      ///< the configuration file (empty: actionmenu.conf in the config directory)
	int m_hideMainMenuEntry = 0;   ///< hide the main menu entry
};

/**
 * Help text of the command line options
 */
const char *cPluginActionmenu::CommandLineHelp(void)
{
	return "  -c FILE,  --config=FILE  read the menu from FILE\n"
	       "                           (default: actionmenu.conf in the config directory)\n";
}

/**
 * Read the command line options
 */
bool cPluginActionmenu::ProcessArgs(int argc, char *argv[])
{
	static const struct option longOptions[] = {
		{ "config", required_argument, nullptr, 'c' },
		{ nullptr, 0, nullptr, 0 }
	};

	int c;
	while ((c = getopt_long(argc, argv, "c:", longOptions, nullptr)) != -1) {
		if (c != 'c')
			return false;
		m_configFile = optarg;
	}

	return true;
}

/**
 * Open the menu
 *
 * actionmenu.conf is read every time, so changes need no restart of VDR.
 */
cOsdObject *cPluginActionmenu::MainMenuAction(void)
{
	std::string fileName = m_configFile.empty() ? *AddDirectory(ConfigDirectory(PLUGIN_NAME_I18N), "actionmenu.conf") : m_configFile;

	cMenuConfig config;
	if (!config.Load(fileName)) {
		esyslog("actionmenu: %s: %s", fileName.c_str(), config.Error().c_str());
		Skins.Message(mtError, cString::sprintf("actionmenu.conf: %s", config.Error().c_str()));
		return nullptr;
	}

	// a key macro like "@actionmenu Blue 2" runs the entry without showing the menu,
	// its keys are already waiting
	const std::vector<sMenuEntry> *entries = &config.Entries();
	std::string title = tr(MAINMENUENTRY);
	while (cRemote::HasKeys()) {
		eKeys key = cRemote::Get(0);
		const sMenuEntry *entry = FindEntry(*entries, key);
		if (!entry) {
			cRemote::Put(key, true);
			break;
		}

		// a submenu is opened directly
		if (entry->action == eEntryAction::Menu) {
			entries = &entry->entries;
			title = entry->text;
			continue;
		}

		cActionMenu *menu = new cActionMenu(title.c_str(), *entries);
		if (menu->RunDirectly(key))
			return menu;
		delete menu;
		return nullptr;
	}

	return new cActionMenu(title.c_str(), *entries);
}

/**
 * Parse a setup.conf parameter
 */
bool cPluginActionmenu::SetupParse(const char *name, const char *value)
{
	if (!strcasecmp(name, "HideMainMenuEntry")) {
		m_hideMainMenuEntry = atoi(value);
		return true;
	}

	return false;
}

VDRPLUGINCREATOR(cPluginActionmenu); // Don't touch this!
