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
	std::vector<sMenuEntry> m_entries;                  ///< the entries (copied, the menu outlives the configuration)
	const sMenuEntry *m_colorEntries[4] = {};           ///< entries of the color buttons (red, green, yellow, blue)

	const sMenuEntry *Find(eKeys) const;
	eOSState Run(const sMenuEntry &);
	eOSState RunSvdrp(const sMenuEntry &);
	eOSState RunKeys(const sMenuEntry &);
	eOSState RunPlugin(const sMenuEntry &);
	eOSState RunChannel(const sMenuEntry &);
	eOSState Error(const char *);
};

#endif
