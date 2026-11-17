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

TEST_CASE("actionmenu.conf - chains", "[menuconf]") {
	cMenuConfig config;

	REQUIRE(Parse(config,
		"1 \"TV off\"? [svdrp drmdevice STAT] (\n"
		"    svdrp drmdevice DETA\n"
		"    # comment\n"
		"\n"
		"    shell /usr/local/bin/tv-off.sh\n"
		"    key Power\n"
		")\n"
		"2 \"PiP +\" (\n"
		"    svdrp! drmdevice PIPU\n"
		"    shell! true\n"
		")\n"
		"3 \"Next\" key Ok\n"));

	const auto &e = config.Entries();
	REQUIRE(e.size() == 3);
	REQUIRE(e[0].action == eEntryAction::Chain);
	REQUIRE(e[0].confirm);
	REQUIRE(e[0].stateCommand == "STAT");
	REQUIRE(e[0].actions.size() == 3);
	REQUIRE(e[0].actions[0].action == eEntryAction::Svdrp);
	REQUIRE(e[0].actions[0].command == "DETA");
	REQUIRE(e[0].actions[0].text == "TV off");
	REQUIRE(e[0].actions[1].action == eEntryAction::Shell);
	REQUIRE(e[0].actions[2].action == eEntryAction::Keys);
	REQUIRE(e[1].actions[0].keepOpen);
	REQUIRE(e[1].actions[1].keepOpen);
	REQUIRE(e[2].action == eEntryAction::Keys);

	const std::pair<const char *, const char *> errors[] = {
		{ "1 \"A\" (\n)\n",                                  "line 1: the chain has no actions" },
		{ "1 \"A\" (\nkey Ok\n",                              "line 1: the chain isn't closed with ')'" },
		{ "1 \"A\" (\nkey Ok\nshell x\n)\n",                 "line 3: plugin, key and output can only be the last action of a chain" },
		{ "1 \"A\" (\noutput df\nkey Ok\n)\n",               "line 3: plugin, key and output can only be the last action of a chain" },
		{ "1 \"A\" (\nplugin x\nplugin y\n)\n",              "line 3: plugin, key and output can only be the last action of a chain" },
		{ "1 \"A\" (\nsvdrp x\n)\n",                          "line 2: svdrp needs a plugin and a command" },
		{ "1 \"A\" (\n2 \"B\" key Ok\n)\n",                  "line 2: unknown action '2' (svdrp, plugin, key, shell, channel, output, { or ()" },
		{ "1 \"A\" (\n{\n)\n",                                "line 2: unknown action '{' (svdrp, plugin, key, shell, channel, output, { or ()" },
		{ "1 \"A\" (\nkey! Ok\n)\n",                          "line 2: key! isn't possible, only svdrp! and shell! keep the menu open" },
	};
	for (const auto &[text, error] : errors) {
		INFO(text);
		REQUIRE_FALSE(Parse(config, text));
		REQUIRE(config.Error() == error);
	}
}

TEST_CASE("actionmenu.conf - sections", "[menuconf]") {
	cMenuConfig config;

	REQUIRE(Parse(config,
		"1 \"Always\" key Ok\n"
		"[live]\n"
		"Red \"Das Erste\" channel 1\n"
		"2 \"Submenu\" {\n"
		"    3 \"Always in the submenu\" key Ok\n"
		"    [Replay]\n"
		"    4 \"Replay in the submenu\" key Ok\n"
		"}\n"
		"[replay]\n"
		"Red \"Cut\" key Ok\n"
		"[ALL]\n"
		"5 \"Always again\" key Ok\n"));

	const auto &e = config.Entries();
	REQUIRE(e.size() == 5);
	REQUIRE(e[0].section == eMenuSection::All);
	REQUIRE(e[1].section == eMenuSection::Live);
	REQUIRE(e[2].section == eMenuSection::Live);
	REQUIRE(e[2].entries[0].section == eMenuSection::All);
	REQUIRE(e[2].entries[1].section == eMenuSection::Replay);
	REQUIRE(e[3].section == eMenuSection::Replay);
	REQUIRE(e[3].key == "Red");
	REQUIRE(e[4].section == eMenuSection::All);

	const std::pair<const char *, const char *> errors[] = {
		{ "[radio]\n",                                          "line 1: unknown section [radio] ([all], [live] or [replay])" },
		{ "Red \"A\" key Ok\n[live]\nRed \"B\" key Ok\n",       "line 3: the key Red is already used in this menu" },
		{ "[replay]\nRed \"A\" key Ok\n[all]\nRed \"B\" key Ok\n", "line 4: the key Red is already used in this menu" },
		{ "[live]\n1 \"A\" key Ok\n1 \"B\" key Ok\n",             "line 3: the key 1 is already used in this menu" },
	};
	for (const auto &[text, error] : errors) {
		INFO(text);
		REQUIRE_FALSE(Parse(config, text));
		REQUIRE(config.Error() == error);
	}
}

TEST_CASE("actionmenu.conf - refresh", "[menuconf]") {
	cMenuConfig config;

	REQUIRE(Parse(config,
		"- \"Temperature\" [shell vcgencmd measure_temp] [refresh]\n"
		"- \"Load\" [shell cut -d' ' -f1 /proc/loadavg][REFRESH 10]\n"
		"1 \"Passthrough\" [svdrp drmdevice PASS] [refresh 5] svdrp! drmdevice PASS toggle\n"
		"2 \"Zoom\" [svdrp drmdevice ZOOM] svdrp drmdevice ZOOM next\n"));

	const auto &e = config.Entries();
	REQUIRE(e[0].refreshSeconds == 2);
	REQUIRE(e[0].action == eEntryAction::Info);
	REQUIRE(e[1].refreshSeconds == 10);
	REQUIRE(e[2].refreshSeconds == 5);
	REQUIRE(e[2].action == eEntryAction::Svdrp);
	REQUIRE(e[2].command == "PASS toggle");
	REQUIRE(e[3].refreshSeconds == 0);

	const std::pair<const char *, const char *> errors[] = {
		{ "- \"A\" [refresh]\n",                             "line 1: refresh needs a state before it" },
		{ "1 \"A\" [refresh] key Ok\n",                      "line 1: refresh needs a state before it" },
		{ "- \"A\" [shell x] [refresh\n",                    "line 1: the closing ']' of refresh is missing" },
		{ "- \"A\" [shell x] [refresh 0]\n",                 "line 1: refresh needs the seconds (1 - 3600)" },
		{ "- \"A\" [shell x] [refresh 3601]\n",              "line 1: refresh needs the seconds (1 - 3600)" },
		{ "- \"A\" [shell x] [refresh 2s]\n",                "line 1: refresh needs the seconds (1 - 3600)" },
		{ "- \"A\" [shell x] [refresh -1]\n",                "line 1: refresh needs the seconds (1 - 3600)" },
	};
	for (const auto &[text, error] : errors) {
		INFO(text);
		REQUIRE_FALSE(Parse(config, text));
		REQUIRE(config.Error() == error);
	}
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
		{ "1 \"A\" ? key Ok\n",                  "line 1: unknown action '?' (svdrp, plugin, key, shell, channel, output, { or ()" },
		{ "1 \"A\" run x\n",                    "line 1: unknown action 'run' (svdrp, plugin, key, shell, channel, output, { or ()" },
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

TEST_CASE("Placeholders", "[menuconf]") {
	const std::map<std::string, std::string> values = {
		{ "channel", "12" }, { "channelid", "S19.2E-1-1019-10301" }, { "recording", "" },
	};

	REQUIRE(ExpandPlaceholders("PION %channel%", values) == "PION 12");
	REQUIRE(ExpandPlaceholders("x %channelid% %channel%%channel%", values) == "x S19.2E-1-1019-10301 1212");
	REQUIRE(ExpandPlaceholders("rm \"%recording%\"", values) == "rm \"\"");
	REQUIRE(ExpandPlaceholders("no placeholder", values) == "no placeholder");
	// unknown placeholders and single '%' stay
	REQUIRE(ExpandPlaceholders("date +%H:%M", values) == "date +%H:%M");
	REQUIRE(ExpandPlaceholders("100% %unknown% %channel%", values) == "100% %unknown% 12");
	REQUIRE(ExpandPlaceholders("50%%channel%", values) == "50%12");
	REQUIRE(ExpandPlaceholders("%", values) == "%");
	REQUIRE(ExpandPlaceholders("%channel", values) == "%channel");
	REQUIRE(ExpandPlaceholders("", values).empty());
}

TEST_CASE("Shell quoting", "[menuconf]") {
	REQUIRE(ShellQuote("") == "''");
	REQUIRE(ShellQuote("12") == "'12'");
	REQUIRE(ShellQuote("/video/Tatort/2026-11-15.20.15.1-0.rec") == "'/video/Tatort/2026-11-15.20.15.1-0.rec'");
	REQUIRE(ShellQuote("a b $HOME `id` \"x\" \\") == "'a b $HOME `id` \"x\" \\'");
	REQUIRE(ShellQuote("it's") == "'it'\\''s'");
	REQUIRE(ShellQuote("''") == "''\\'''\\'''");
}

TEST_CASE("actionmenu.conf.example", "[menuconf]") {
	cMenuConfig config;

	bool loaded = config.Load("../actionmenu.conf.example");
	INFO(config.Error());
	REQUIRE(loaded);
	REQUIRE_FALSE(config.Entries().empty());
}
