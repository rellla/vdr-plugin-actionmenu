// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file menu.h
 * The Action Menu
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#ifndef __ACTIONMENU_MENU_H
#define __ACTIONMENU_MENU_H

#include <cstdint>
#include <string>
#include <vector>

#include <vdr/osdbase.h>

#include "menuconf.h"

/**
 * Menu with the entries of actionmenu.conf
 *
 * The entries with a color key are the color buttons of the skin, the other ones
 * are in the list. A number or color key runs its entry at once, Up/Down and Ok
 * select an entry of the list, Left/Right page and Back goes back to the menu
 * above.
 */
class cActionMenu : public cOsdMenu {
public:
	cActionMenu(const char *, const std::vector<sMenuEntry> &);
	virtual eOSState ProcessKey(eKeys);

private:
	std::vector<sMenuEntry> m_entries;               ///< the shown entries (copied, the menu outlives the configuration)
	const sMenuEntry *m_colorEntries[4] = {};        ///< entries of the color buttons (red, green, yellow, blue)
	std::string m_helpTexts[4];                      ///< texts of the color buttons (SetHelp() keeps the pointers)
	bool m_hasStates = false;                        ///< true, if an entry shows its state
	bool m_hasRefresh = false;                       ///< true, if a state is read again periodically
	uint64_t m_nextHelpRefresh[4] = {};              ///< time to read the states of the color buttons again

	void SetHelpTexts(void);
	void UpdateStates(void);
	void RefreshStates(void);
	const sMenuEntry *Find(eKeys) const;
	eOSState Run(const sMenuEntry &);
	eOSState RunSvdrp(const sMenuEntry &);
	eOSState RunKeys(const sMenuEntry &);
	eOSState RunPlugin(const sMenuEntry &);
	eOSState RunChannel(const sMenuEntry &);
	eOSState RunOutput(const sMenuEntry &);
	eOSState Error(const char *);
};

#endif
