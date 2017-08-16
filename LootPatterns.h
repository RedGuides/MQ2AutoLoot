#pragma once

void read_loot_patterns(const char* inifile, void(*report)(const char*));
void forget_loot_patterns();
void list_loot_patterns(void(report)(const char *));
bool action_from_loot_patterns(const char* lootname, char* action, int max_size);