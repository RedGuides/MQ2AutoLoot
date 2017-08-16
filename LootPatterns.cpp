//
// Provide access to the Patterns section in Loot.ini
//

#include <regex>
#include <vector>
#include <Windows.h>
#include "LootPatterns.h"

constexpr auto max_string = 2048;
constexpr auto section_name = "Patterns";

using namespace std;

static vector<string> names;
static vector<string> patterns;
static vector<regex> expressions;
static vector<string> actions;

void forget_loot_patterns()
{
	names.clear();
	patterns.clear();
	expressions.clear();
	actions.clear();
}

void read_loot_patterns(const char* inifile, void(*report)(const char*))
{
	forget_loot_patterns();
	char keynames[max_string]; // string of strings for keynames
	auto nchars = GetPrivateProfileString(section_name, nullptr, nullptr, keynames, max_string, inifile);
	if (nchars == 0) return;
	const regex rx("\"(.+)\"=\\s.(\\w+)");
	for (const char* key = keynames; *key; ++key)
	{
		char value[max_string];
		GetPrivateProfileString(section_name, key, nullptr, value, max_string, inifile);
		cmatch submatch;
		if (regex_search(value, submatch, rx))
		{
			if (submatch.size() == 3)
			{
				const auto action(submatch.str(2));
				if (action == "Keep" || action == "Ignore")
				{
					names.push_back(string(key));
					patterns.push_back(submatch.str(1));
					expressions.push_back(regex(submatch.str(1)));
					actions.push_back(action);
				}
				else
				{
					if (report) report("Action for patterns must be Keep or Ignore");
				}
			}
		}
		// advance to end of current string in string of strings
		while (*key) ++key;
	}
}

void list_loot_patterns(void(report)(const char*))
{
	for (size_t i = 0; i < names.size(); ++i)
	{
		report(("Pattern " + names[i] + "=\"" + patterns[i] + "\"=" + actions[i]).c_str());
	}
}

bool action_from_loot_patterns(const char* lootname, char* action, int max_size)
{
	for (size_t i = 0; i < expressions.size(); ++i)
	{
		if (regex_match(lootname, expressions[i]))
			return (strcpy_s(action, max_size, actions[i].c_str()) == 0);
	}
	return false;
}