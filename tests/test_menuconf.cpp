// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file test_menuconf.cpp
 * Unit tests for cMenuConfig
 */

#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "menuconf.h"

namespace {

/** Parse the content of a actionmenu.conf */
bool Parse(cMenuConfig &config, const std::string &text)
{
	std::istringstream in(text);
	return config.Parse(in);
}

} // namespace

TEST_CASE("actionmenu.conf - actions", "[menuconf]") {
	cMenuConfig config;

	REQUIRE(Parse(config,
		"# comment\n"
		"\n"
		"1   \"PiP on/off\"     svdrp drmdevice PITO\n"
		"  2 \"Zoom\"           svdrp!  drmdevice  ZOOM next \n"
		"red \"EPG search\"     plugin epgsearch\n"
		"-   \"Schedule\"       key Schedule Down\n"
		"Blue \"Script\"        shell! /usr/bin/script.sh --arg \"x y\"\n"
		"5   \"Das Erste\"      channel 1\n"
		"6   \"ZDF\"            channel S19.2E-1-1079-28006\n"
		"7   \"Temperature\"    output vcgencmd measure_temp 2>&1\n"));

	const auto &e = config.Entries();
	REQUIRE(e.size() == 8);

	REQUIRE(e[0].key == "1");
	REQUIRE(e[0].text == "PiP on/off");
	REQUIRE(e[0].action == eEntryAction::Svdrp);
	REQUIRE(e[0].plugin == "drmdevice");
	REQUIRE(e[0].command == "PITO");
	REQUIRE_FALSE(e[0].keepOpen);

	REQUIRE(e[1].keepOpen);
	REQUIRE(e[1].plugin == "drmdevice");
	REQUIRE(e[1].command == "ZOOM next");

	REQUIRE(e[2].key == "Red");
	REQUIRE(e[2].action == eEntryAction::Plugin);
	REQUIRE(e[2].plugin == "epgsearch");

	REQUIRE(e[3].key.empty());
	REQUIRE(e[3].action == eEntryAction::Keys);
	REQUIRE(e[3].command == "Schedule Down");

	REQUIRE(e[4].key == "Blue");
	REQUIRE(e[4].action == eEntryAction::Shell);
	REQUIRE(e[4].keepOpen);
	REQUIRE(e[4].command == "/usr/bin/script.sh --arg \"x y\"");

	REQUIRE(e[5].action == eEntryAction::Channel);
	REQUIRE(e[5].command == "1");
	REQUIRE(e[6].command == "S19.2E-1-1079-28006");

	REQUIRE(e[7].action == eEntryAction::Output);
	REQUIRE(e[7].command == "vcgencmd measure_temp 2>&1");
}

TEST_CASE("actionmenu.conf - states", "[menuconf]") {
	cMenuConfig config;

	REQUIRE(Parse(config,
		"1 \"Passthrough\" [svdrp drmdevice PASS] svdrp! drmdevice PASS toggle\n"
		"Red \"Ambient light\"[ SVDRP hyperion  AMBI ]svdrp hyperion AMBI toggle\n"
		"2 \"Output\" [svdrp drmdevice STAT] {\n"
		"}\n"
		"3 \"Zoom\" svdrp drmdevice ZOOM next\n"
		"4 \"Temperature\" [shell vcgencmd measure_temp | cut -d= -f2] shell! true\n"));

	const auto &e = config.Entries();
	REQUIRE(e[0].stateAction == eEntryAction::Svdrp);
	REQUIRE(e[0].statePlugin == "drmdevice");
	REQUIRE(e[0].stateCommand == "PASS");
	REQUIRE(e[0].command == "PASS toggle");
	REQUIRE(e[0].keepOpen);
	REQUIRE(e[1].statePlugin == "hyperion");
	REQUIRE(e[1].stateCommand == "AMBI");
	REQUIRE(e[1].plugin == "hyperion");
	REQUIRE(e[2].action == eEntryAction::Menu);
	REQUIRE(e[2].stateCommand == "STAT");
	REQUIRE(e[3].statePlugin.empty());
	REQUIRE(e[3].stateCommand.empty());
	REQUIRE(e[4].stateAction == eEntryAction::Shell);
	REQUIRE(e[4].statePlugin.empty());
	REQUIRE(e[4].stateCommand == "vcgencmd measure_temp | cut -d= -f2");
	REQUIRE(e[4].command == "true");
}

TEST_CASE("actionmenu.conf - entries without action", "[menuconf]") {
	cMenuConfig config;

	REQUIRE(Parse(config,
		"- \"Temperature\" [shell vcgencmd measure_temp | cut -d= -f2]\n"
		"1 \"Output\" [svdrp drmdevice STAT]  \n"
		"Red \"Ambient light\" [svdrp hyperion AMBI]\n"));

	const auto &e = config.Entries();
	REQUIRE(e.size() == 3);
	for (const auto &entry : e)
		REQUIRE(entry.action == eEntryAction::Info);
	REQUIRE(e[0].stateAction == eEntryAction::Shell);
	REQUIRE(e[0].stateCommand == "vcgencmd measure_temp | cut -d= -f2");
	REQUIRE(e[1].key == "1");
	REQUIRE(e[1].stateCommand == "STAT");
	REQUIRE(e[2].key == "Red");
}

TEST_CASE("actionmenu.conf - headers", "[menuconf]") {
	cMenuConfig config;

	REQUIRE(Parse(config,
		"- \"Output\"\n"
		"1 \"Detach\" svdrp drmdevice DETA\n"
		"-   \"Picture in picture\"  \n"));

	const auto &e = config.Entries();
	REQUIRE(e.size() == 3);
	REQUIRE(e[0].action == eEntryAction::Info);
	REQUIRE(e[0].key.empty());
	REQUIRE(e[0].stateCommand.empty());
	REQUIRE(e[0].text == "Output");
	REQUIRE(e[2].action == eEntryAction::Info);

	SECTION("an entry with a key needs an action or a state") {
		REQUIRE_FALSE(Parse(config, "1 \"A\"\n"));
		REQUIRE(config.Error() == "line 1: the action is missing");
		REQUIRE_FALSE(Parse(config, "Red \"A\"\n"));
	}

	SECTION("a header doesn't ask") {
		REQUIRE_FALSE(Parse(config, "- \"A\"?\n"));
		REQUIRE(config.Error() == "line 1: an entry without action doesn't ask");
	}
}

TEST_CASE("actionmenu.conf - confirmation", "[menuconf]") {
	cMenuConfig config;

	REQUIRE(Parse(config,
		"1 \"Restart VDR\"? shell systemctl restart vdr\n"
		"2 \"Detach\"?[svdrp drmdevice STAT] svdrp drmdevice DETA\n"
		"3 \"Why?\" key Ok\n"));

	const auto &e = config.Entries();
	REQUIRE(e[0].confirm);
	REQUIRE(e[0].command == "systemctl restart vdr");
	REQUIRE(e[1].confirm);
	REQUIRE(e[1].stateCommand == "STAT");
	REQUIRE(e[1].command == "DETA");
	REQUIRE_FALSE(e[2].confirm);
	REQUIRE(e[2].text == "Why?");
}

TEST_CASE("actionmenu.conf - submenus", "[menuconf]") {
	cMenuConfig config;

	REQUIRE(Parse(config,
		"Red \"Output\" {\n"
		"    1 \"Detach\" svdrp drmdevice DETA\n"
		"    2 \"More\" {\n"
		"        1 \"Attach\" svdrp drmdevice ATTA\n"
		"    }\n"
		"    3 \"Status\" svdrp drmdevice STAT\n"
		"}\n"
		"1 \"Zoom\" svdrp drmdevice ZOOM next\n"));

	const auto &e = config.Entries();
	REQUIRE(e.size() == 2);
	REQUIRE(e[0].action == eEntryAction::Menu);
	REQUIRE(e[0].entries.size() == 3);
	REQUIRE(e[0].entries[1].entries.size() == 1);
	REQUIRE(e[0].entries[1].entries[0].command == "ATTA");
	REQUIRE(e[0].entries[2].command == "STAT");
	REQUIRE(e[1].text == "Zoom");

	SECTION("the same key in another menu") {
		REQUIRE(Parse(config, "1 \"A\" {\n1 \"B\" key Ok\n}\n"));
	}

	SECTION("empty submenu") {
		REQUIRE(Parse(config, "1 \"A\" {\n}\n"));
		REQUIRE(config.Entries()[0].entries.empty());
	}
}

TEST_CASE("actionmenu.conf - errors", "[menuconf]") {
	cMenuConfig config;

	const std::pair<const char *, const char *> errors[] = {
		{ "x \"A\" key Ok\n",                   "line 1: unknown key 'x' (0 - 9, Red, Green, Yellow, Blue or -)" },
		{ "10 \"A\" key Ok\n",                  "line 1: unknown key '10' (0 - 9, Red, Green, Yellow, Blue or -)" },
		{ "1 A key Ok\n",                       "line 1: the text in quotes is missing" },
		{ "1\n",                                "line 1: the text in quotes is missing" },
		{ "1 \"A key Ok\n",                     "line 1: the closing quote of the text is missing" },
		{ "1 \"\" key Ok\n",                    "line 1: the text is empty" },
		{ "1 \"A\"\n",                          "line 1: the action is missing" },
		{ "1 \"A\" [svdrp drmdevice PASS svdrp x y\n", "line 1: the closing ']' of the state is missing" },
		{ "1 \"A\" [svdrp drmdevice] svdrp x y\n",     "line 1: the state needs svdrp <plugin> <command> or shell <command>" },
		{ "1 \"A\" [drmdevice PASS] svdrp x y\n",      "line 1: the state needs svdrp <plugin> <command> or shell <command>" },
		{ "1 \"A\" [shell] svdrp x y\n",               "line 1: the state needs svdrp <plugin> <command> or shell <command>" },
		{ "1 \"A\" [] svdrp x y\n",                    "line 1: the state needs svdrp <plugin> <command> or shell <command>" },
		{ "1 \"A\"? [svdrp drmdevice PASS]\n",         "line 1: an entry without action doesn't ask" },
		{ "1 \"A\"? {\n}\n",                     "line 1: a submenu doesn't ask before it is opened" },
		{ "1 \"A\" ? key Ok\n",                  "line 1: unknown action '?' (svdrp, plugin, key, shell, channel, output or {)" },
		{ "1 \"A\" run x\n",                    "line 1: unknown action 'run' (svdrp, plugin, key, shell, channel, output or {)" },
		{ "1 \"A\" svdrp drmdevice\n",          "line 1: svdrp needs a plugin and a command" },
		{ "1 \"A\" plugin\n",                   "line 1: plugin needs the name of a plugin" },
		{ "1 \"A\" plugin a b\n",               "line 1: plugin needs the name of a plugin" },
		{ "1 \"A\" key\n",                      "line 1: key needs at least one key" },
		{ "1 \"A\" shell\n",                    "line 1: shell needs a command" },
		{ "1 \"A\" channel\n",                  "line 1: channel needs the number or the ID of a channel" },
		{ "1 \"A\" channel 1 2\n",              "line 1: channel needs the number or the ID of a channel" },
		{ "1 \"A\" channel! 1\n",               "line 1: channel! isn't possible, only svdrp! and shell! keep the menu open" },
		{ "1 \"A\" output\n",                   "line 1: output needs a command" },
		{ "1 \"A\" output! df\n",               "line 1: output! isn't possible, only svdrp! and shell! keep the menu open" },
		{ "1 \"A\" key! Ok\n",                  "line 1: key! isn't possible, only svdrp! and shell! keep the menu open" },
		{ "1 \"A\" plugin! epgsearch\n",        "line 1: plugin! isn't possible, only svdrp! and shell! keep the menu open" },
		{ "\n1 \"A\" key Ok\n1 \"B\" key Ok\n", "line 3: the key 1 is already used in this menu" },
		{ "Red \"A\" key Ok\nRED \"B\" key Ok\n", "line 2: the key Red is already used in this menu" },
		{ "1 \"A\" key Ok\n}\n",                "line 2: '}' without a submenu" },
		{ "1 \"A\" {\n2 \"B\" {\n}\n",          "line 1: the submenu isn't closed with '}'" },
	};

	for (const auto &[text, error] : errors) {
		INFO(text);
		REQUIRE_FALSE(Parse(config, text));
		REQUIRE(config.Error() == error);
		REQUIRE(config.Entries().empty());
	}

	SECTION("entries without a key don't collide") {
		REQUIRE(Parse(config, "- \"A\" key Ok\n- \"B\" key Ok\n"));
		REQUIRE(config.Entries().size() == 2);
	}

	SECTION("a missing file") {
		REQUIRE_FALSE(config.Load("/nonexistent/actionmenu.conf"));
		REQUIRE(config.Error() == "can't open /nonexistent/actionmenu.conf");
	}
}
