// MQ2AutoLoot.cpp : Defines the entry point for the DLL application.
//Author: Plure
//
// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.

#define PLUGIN_NAME					"MQ2AutoLoot"                // Plugin Name
#define VERSION						1.11
   
#define PERSONALBANKER_CLASS		40
#define MERCHANT_CLASS				41
#define GUILDBANKER_CLASS			66

#ifndef PLUGIN_API
#include "../MQ2Plugin.h"
PreSetup(PLUGIN_NAME);
PLUGIN_VERSION(VERSION);
#endif PLUGIN_API

#include "../MQ2AutoLootSort/LootSort.h"
#include "LootPatterns.h"
#include "MQ2AutoLoot.h"
#include "ItemActions.h"


//MQ2EQBC shit
bool(*fAreTheyConnected)(char* szName);

// Variables that are setable through the /AutoLoot command
int						iUseAutoLoot = 1;
int						iSpamLootInfo = 1;
int						iCursorDelay = 10;
int						iDistributeLootDelay = 5;
int						iSaveBagSlots = 0;
int						iQuestKeep = 0;
int						iBarMinSellPrice = 1;
int						iLogLoot = 1;
int						iRaidLoot = 0;
char					szNoDropDefault[MAX_STRING];
char					szLootINI[MAX_STRING];
char					szExcludedBag1[MAX_STRING];
char					szExcludedBag2[MAX_STRING];
char					szGuildItemPermission[MAX_STRING];
// Variables that are used within the plugin, but not setable 
bool					Initialized = false;
bool					CheckedAutoLootAll = false;  // used to check if Auto Loot All is checks
bool					StartCursorTimer = true;  // 
bool					StartDistributeLootTimer = true;  // 
bool					bDistributeItemSucceeded = false; // Set true when you successfully distribute an item to a raid/group member
bool					bDistributeItemFailed = false; // Set true when you fail distribute an item to a raid/group member
LONG					DistributeI; //Index for looping over people in the group to try and distribute an item 
LONG					DistributeK;  // Index for the item to be distributed
DWORD					DestroyID;
DWORD					CursorItemID;
DWORD					DistributeItemID;
char					szTemp[MAX_STRING];
char					szCommand[MAX_STRING];
char					szLogPath[MAX_STRING];
char					szLogFileName[MAX_STRING];

pluginclock::time_point	LootTimer = pluginclock::now();
pluginclock::time_point	CursorTimer = pluginclock::now();
pluginclock::time_point	DestroyStuffCancelTimer = pluginclock::now();

/********************************************************************************
****
**     Functions necessary for OnPulse
****
********************************************************************************/

bool InGameOK(void) // Returns TRUE if character is in game and has valid character data structures
{
	return(GetGameState() == GAMESTATE_INGAME && GetCharInfo() && GetCharInfo()->pSpawn && GetCharInfo()->pSpawn->StandState != STANDSTATE_DEAD && GetCharInfo2());
}

void SetAutoLootVariables(void)
{
	sprintf_s(INIFileName, "%s\\%s.ini", gszINIPath, PLUGIN_NAME);
	sprintf_s(szLogPath, "%s\\", gszLogPath);
	sprintf_s(szLogFileName, "%sLoot_Log.log", szLogPath);
	iUseAutoLoot = GetPrivateProfileInt(GetCharInfo()->Name, "UseAutoLoot", 1, INIFileName);
	if (GetPrivateProfileString(GetCharInfo()->Name, "lootini", 0, szLootINI, MAX_STRING, INIFileName) == 0)
	{
		sprintf_s(szLootINI, "%s\\Macros\\Loot.ini", gszINIPath);  // Default location is in your \macros\loot.ini
		WritePrivateProfileString(GetCharInfo()->Name, "lootini", szLootINI, INIFileName);
	}
	LONG Version = GetPrivateProfileInt("Settings", "Version", -1, szLootINI);
	if (Version == -1)
	{
		CreateLootINI();
		CHAR Version[MAX_STRING] = { 0 };
		sprintf_s(Version, "%1.2f", VERSION);
		WritePrivateProfileString("Settings", "Version", Version, szLootINI);
	}
	else if (Version != VERSION)
	{

	}
	iLogLoot = GetPrivateProfileInt("Settings", "LogLoot", -1, szLootINI);
	if (iLogLoot == -1)
	{
		iLogLoot = 0;
		WritePrivateProfileString("Settings", "LogLoot", "0", szLootINI);
	}
	iRaidLoot = GetPrivateProfileInt("Settings", "RaidLoot", -1, szLootINI);
	if (iRaidLoot == -1)
	{
		iRaidLoot = 0;
		WritePrivateProfileString("Settings", "RaidLoot", "0", szLootINI);
	}
	iBarMinSellPrice = GetPrivateProfileInt("Settings", "BarMinSellPrice", -1, szLootINI);
	if (iBarMinSellPrice == -1)
	{
		iBarMinSellPrice = 1;
		WritePrivateProfileString("Settings", "BarMinSellPrice", "1", szLootINI);
	}
	iDistributeLootDelay = GetPrivateProfileInt("Settings", "DistributeLootDelay", -1, szLootINI);
	if (iDistributeLootDelay == -1)
	{
		iDistributeLootDelay = 5;
		WritePrivateProfileString("Settings", "DistributeLootDelay", "5", szLootINI);
	}
	iCursorDelay = GetPrivateProfileInt("Settings", "CursorDelay", -1, szLootINI);
	if (iCursorDelay == -1)
	{
		iCursorDelay = 10;
		WritePrivateProfileString("Settings", "CursorDelay", "10", szLootINI);
	}
	iSaveBagSlots = GetPrivateProfileInt("Settings", "SaveBagSlots", -1, szLootINI);
	if (iSaveBagSlots == -1)
	{
		iSaveBagSlots = 0;
		WritePrivateProfileString("Settings", "SaveBagSlots", "0", szLootINI);
	}
	iSpamLootInfo = GetPrivateProfileInt("Settings", "SpamLootInfo", -1, szLootINI);
	if (iSpamLootInfo == -1)
	{
		iSpamLootInfo = 1;
		WritePrivateProfileString("Settings", "SpamLootInfo", "1", szLootINI);
	}
	iQuestKeep = GetPrivateProfileInt("Settings", "QuestKeep", -1, szLootINI);
	if (iQuestKeep == -1)
	{
		iQuestKeep = 10;
		WritePrivateProfileString("Settings", "QuestKeep", "10", szLootINI);
	}
	if (GetPrivateProfileString("Settings", "NoDropDefault", 0, szNoDropDefault, MAX_STRING, szLootINI) == 0)
	{
		sprintf_s(szNoDropDefault, "Quest");
		WritePrivateProfileString("Settings", "NoDropDefault", szNoDropDefault, szLootINI);
	}
	if (GetPrivateProfileString("Settings", "ExcludedBag1", 0, szExcludedBag1, MAX_STRING, szLootINI) == 0)
	{
		sprintf_s(szExcludedBag1, "Extraplanar Trade Satchel");
		WritePrivateProfileString("Settings", "ExcludeBag1", szExcludedBag1, szLootINI);
	}
	if (GetPrivateProfileString("Settings", "ExcludedBag2", 0, szExcludedBag2, MAX_STRING, szLootINI) == 0)
	{
		sprintf_s(szExcludedBag2, "Extraplanar Trade Satchel");
		WritePrivateProfileString("Settings", "ExcludeBag2", szExcludedBag2, szLootINI);
	}
	if (GetPrivateProfileString("Settings", "GuildItemPermission", 0, szGuildItemPermission, MAX_STRING, szLootINI) == 0)
	{
		sprintf_s(szGuildItemPermission, "View Only");
		WritePrivateProfileString("Settings", "GuildItemPermission", szGuildItemPermission, szLootINI);
	}
	const auto report_function = [](const char* message) {WriteChatf(PLUGIN_MSG ":: %s", message); };
	read_loot_patterns(szLootINI, report_function);

	if (Initialized) // Won't spam this on start up of plugin, will only spam if someone reloads their settings
	{
		WriteChatf(PLUGIN_MSG ":: AutoLoot is %s", iUseAutoLoot ? "\agON\ax" : "\arOFF\ax");
		WriteChatf(PLUGIN_MSG ":: Stop looting when \ag%d\ax slots are left", iSaveBagSlots);
		WriteChatf(PLUGIN_MSG ":: Spam looting actions %s", iSpamLootInfo ? "\agON\ax" : "\arOFF\ax");
		WriteChatf(PLUGIN_MSG ":: The master looter will wait \ag%d\ax seconds before trying to distribute loot", iDistributeLootDelay);
		WriteChatf(PLUGIN_MSG ":: You will wait \ag%d\ax seconds before trying to autoinventory items on your cursor", iCursorDelay);
		WriteChatf(PLUGIN_MSG ":: The minimum price for all items to be bartered is: \ag%d\ax", iBarMinSellPrice);
		WriteChatf(PLUGIN_MSG ":: Logging loot actions for master looter is %s", iLogLoot ? "\agON\ax" : "\arOFF\ax");
		WriteChatf(PLUGIN_MSG ":: Raid looting is turned: %s", iRaidLoot ? "\agON\ax" : "\arOFF\ax");
		WriteChatf(PLUGIN_MSG ":: Your default number to keep of new quest items is: \ag%d\ax", iQuestKeep);
		WriteChatf(PLUGIN_MSG ":: Your default for new no drop items is: \ag%s\ax", szNoDropDefault);
		WriteChatf(PLUGIN_MSG ":: Will exclude \ar%s\ax when checking for free slots", szExcludedBag1);
		WriteChatf(PLUGIN_MSG ":: Will exclude \ar%s\ax when checking for free slots", szExcludedBag2);
		WriteChatf(PLUGIN_MSG ":: Your default permission for items put into your guild bank is: \ag%s\ax", szGuildItemPermission);
		list_loot_patterns(report_function);
		WriteChatf(PLUGIN_MSG ":: The location for your loot ini is:\n \ag%s\ax", szLootINI);
	}
}

void CreateLootINI(void)
{
	const auto sections = { "Settings","Patterns","Global","A","B","C","D","E",
		"F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z" };
	for (const auto section : sections)
		WritePrivateProfileString(section, "|", "=====================================================================|", szLootINI);
}

bool CheckCursor(void)  // Returns true if an item is on your cursor
{
	PCHARINFO2 pChar2 = GetCharInfo2();
	if (pChar2->pInventoryArray && pChar2->pInventoryArray->Inventory.Cursor)
	{
		if (PCONTENTS pItem = pChar2->pInventoryArray->Inventory.Cursor)
		{
			if (!WinState((CXWnd*)pTradeWnd) && !WinState((CXWnd*)pGiveWnd) && !WinState((CXWnd*)pMerchantWnd) && !WinState((CXWnd*)pBankWnd) && !WinState((CXWnd*)FindMQ2Window("GuildBankWnd")) && !WinState((CXWnd*)FindMQ2Window("TradeskillWnd")))
			{
				if (StartCursorTimer)  // Going to set CursorItemID and CursorTimer
				{
					StartCursorTimer = false;
					CursorItemID = pItem->ID;
					CursorTimer = pluginclock::now() + std::chrono::seconds(iCursorDelay); // Wait CursorDelay in seconds before attempting to autoinventory the item on your cursor
				}
				else if (CursorItemID != pItem->ID) // You changed items on your cursor, time to reset CursorItemID and CursorTimer
				{
					CursorItemID = pItem->ID;
					CursorTimer = pluginclock::now() + std::chrono::seconds(iCursorDelay); // Wait CursorDelay in seconds before attempting to autoinventory the item on your cursor
				}
				else if (pluginclock::now() > CursorTimer)  // Waited CursorDelay, now going to see if you have room to autoinventory the item on your cursor
				{
					if (FitInInventory(pItem->Item2->Size))
					{
						if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: Putting \ag%s\ax into my inventory", pItem->Item2->Name); }
						DoCommand(GetCharInfo()->pSpawn, "/autoinventory");
						StartCursorTimer = true;
					}
					else
					{
						if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: \ag%s\ax doesn't fit into my inventory", pItem->Item2->Name); }
						StartCursorTimer = true;
					}
				}
			}
		}
		return true;
	}
	else
	{
		StartCursorTimer = true;
		return false;
	}

}

bool DestroyStuff(void)
{
	if (DestroyID == 0) return false;
	PCHARINFO pChar = GetCharInfo();
	PCHARINFO2 pChar2 = GetCharInfo2();
	if (pluginclock::now() > DestroyStuffCancelTimer)
	{
		if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: Destroying timer ran out, moving on!"); }
		DestroyID = 0;
		return false;
	}
	// If I have the item on my cursor I need to destroy it
	if (pChar2->pInventoryArray && pChar2->pInventoryArray->Inventory.Cursor)
	{
		if (PCONTENTS pItem = pChar2->pInventoryArray->Inventory.Cursor)
		{
			if (pItem->ID == DestroyID)
			{
				if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: Destroying \ar%s\ax!", pItem->Item2->Name); }
				DoCommand(GetCharInfo()->pSpawn, "/destroy");
				DestroyID = 0;
				return true;
			}
		}
	}
	if (pChar2 && pChar2->pInventoryArray && pChar2->pInventoryArray->InventoryArray) //check my inventory slots
	{
		for (unsigned long nSlot = BAG_SLOT_START; nSlot < NUM_INV_SLOTS; nSlot++)
		{
			if (PCONTENTS pItem = pChar2->pInventoryArray->InventoryArray[nSlot])
			{
				if (pItem->ID == DestroyID)
				{
					PickupItem(eItemContainerPossessions, pItem);
					LootTimer = pluginclock::now() + std::chrono::milliseconds(100);
					return true;
				}
			}
		}
	}
	if (pChar2 && pChar2->pInventoryArray) //Checking my bags
	{
		for (unsigned long nPack = 0; nPack < 10; nPack++)
		{
			if (PCONTENTS pPack = pChar2->pInventoryArray->Inventory.Pack[nPack])
			{
				if (PITEMINFO pItemPack = GetItemFromContents(pPack))
				{
					if (pItemPack->Type == ITEMTYPE_PACK && pPack->Contents.ContainedItems.pItems)
					{
						for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
						{
							if (PCONTENTS pItem = pPack->Contents.ContainedItems.pItems->Item[nItem])
							{
								if (pItem->ID == DestroyID)
								{
									PickupItem(eItemContainerPossessions, pItem);
									LootTimer = pluginclock::now() + std::chrono::milliseconds(100);
									return true;
								}
							}
						}
					}
				}
			}
		}
	}
	return true;
}

bool CheckWindows(bool ItemOnCursor) // Returns true if your attempting to accept trade requests or click the confirmation box for no drop items
{
	// When confirmation box for looting no drop items pops up this will allow it to be clicked
	if (CSidlScreenWnd *pWnd = (CSidlScreenWnd *)FindMQ2Window("ConfirmationDialogBox"))
	{
		if (pWnd->dShow)
		{
			if (CStmlWnd *Child = (CStmlWnd*)pWnd->GetChildItem("CD_TextOutput"))
			{
				char ConfirmationText[MAX_STRING];
				GetCXStr(Child->STMLText, ConfirmationText, sizeof(ConfirmationText));
				if (strstr(ConfirmationText, "is a NO DROP item, are you sure you wish to loot it?"))
				{
					if (WinState((CXWnd*)pLootWnd))
					{
						PEQLOOTWINDOW pLoot = (PEQLOOTWINDOW)pLootWnd;
						for (int nLootSlots = 0; nLootSlots < (int)pLoot->NumOfSlots; nLootSlots++)
						{
							if (PCONTENTS pContents = pLoot->pInventoryArray->InventoryArray[nLootSlots])
							{
								if (PITEMINFO pItem = GetItemFromContents(pContents))
								{
									if (strstr(ConfirmationText, pItem->Name))
									{
										if (!CheckIfItemIsLoreByID(pContents->ID))
										{
											//Ok so I don't have the item and it is lore or it is not lore and I can accept it
											if (CXWnd *pWndButton = pWnd->GetChildItem("CD_Yes_Button"))
											{
												if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: Accepting a no drop item"); }
												SendWndClick2(pWndButton, "leftmouseup");
												LootTimer = pluginclock::now() + std::chrono::milliseconds(1000);
												return true;
											}
										}
									}
								}
							}
						}
					}
					else
					{
						if (CXWnd *pWndButton = pWnd->GetChildItem("CD_Yes_Button"))
						{
							if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: Accepting a no drop item"); }
							SendWndClick2(pWndButton, "leftmouseup");
							LootTimer = pluginclock::now() + std::chrono::milliseconds(1000);
							return true;
						}
					}
				}
			}
		}
	}

	if (PEQTRADEWINDOW pTrade = (PEQTRADEWINDOW)pTradeWnd)
	{
		if (pTrade->HisTradeReady && !pTrade->MyTradeReady && !ItemOnCursor)
		{
			if (CXWnd* pTRDW_HisName = pTradeWnd->GetChildItem("TRDW_HisName"))
			{
				GetCXStr(pTRDW_HisName->WindowText, szTemp, MAX_STRING - 1);
				if (szTemp[0] != '\0')
				{
					if (HandleEQBC())
					{
						if (fAreTheyConnected(szTemp))
						{
							if (CXWnd *pWndButton = pTradeWnd->GetChildItem("TRDW_Trade_Button"))
							{
								SendWndClick2(pWndButton, "leftmouseup");
								LootTimer = pluginclock::now() + std::chrono::milliseconds(1000);
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool SetLootSettings(void) // Turn off Auto Loot All
{
	if (CheckedAutoLootAll) return false;
	if (!WinState((CXWnd*)FindMQ2Window("LootSettingsWnd")))
	{
		if (CXWnd *pWndButton = pAdvancedLootWnd->GetChildItem("ADLW_LootSettingsBtn"))
		{
			SendWndClick2(pWndButton, "leftmouseup");
			LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
			return true;
		}
	}
	if (CButtonWnd *pWndButton = (CButtonWnd*)FindMQ2Window("LootSettingsWnd")->GetChildItem("LS_AutoLootAllCheckbox"))
	{
		if (pWndButton->bActive)
		{
			if (pWndButton->Checked)
			{
				SendWndClick2(pWndButton, "leftmouseup");
				LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
				return true;
			}
			else
			{
				DoCommand(GetCharInfo()->pSpawn, "/squelch /windowstate LootSettingsWnd close");
				CheckedAutoLootAll = true;
			}
		}
	}
	return false;
}

bool HandlePersonalLoot(bool ItemOnCursor, PCHARINFO pChar, PEQADVLOOTWND pAdvLoot, CListWnd *pPersonalList, CListWnd *pSharedList) // Handle items in your personal loot window
{
	if (pAdvLoot->pPLootList)
	{
		for (LONG k = 0; k < pPersonalList->ItemsArray.Count; k++)
		{
			LONG listindex = pPersonalList->GetItemData(k);
			if (listindex != -1)
			{
				DWORD multiplier = sizeof(LOOTITEM) * listindex;
				if (PLOOTITEM pPersonalItem = (PLOOTITEM)(((DWORD)pAdvLoot->pPLootList->pLootItem) + multiplier))
				{
					CHAR action[MAX_STRING];
					action[0] = '\0';
					CHAR INISection[] = { pPersonalItem->Name[0],'\0' };
					auto found = GetPrivateProfileString(INISection, pPersonalItem->Name, 0, action, MAX_STRING, szLootINI) || action_from_loot_patterns(pPersonalItem->Name, action, MAX_STRING);
					if (!found)
					{
						if (pPersonalItem->NoDrop)
						{
							CHAR LootEntry[MAX_STRING];
							LootEntry[0] = '\0';
							CHAR *pParsedToken = NULL;
							CHAR *pParsedValue = strtok_s(szNoDropDefault, "|", &pParsedToken);
							if (!_stricmp(pParsedValue, "Quest"))
							{
								sprintf_s(LootEntry, "%s|%d", pParsedValue, iQuestKeep);
							}
							else if (!_stricmp(pParsedValue, "Keep") || !_stricmp(pParsedValue, "Ignore"))
							{
								sprintf_s(LootEntry, "%s", pParsedValue);
							}
							else
							{
								sprintf_s(LootEntry, "Quest|1");
							}
							WriteChatf(PLUGIN_MSG ":: The \ag%s\ax is not in the database, setting it to %s", pPersonalItem->Name, LootEntry);
							WritePrivateProfileString(INISection, pPersonalItem->Name, LootEntry, szLootINI);
							sprintf_s(action, "%s", LootEntry);
						}
						else
						{
							WriteChatf(PLUGIN_MSG ":: The \ag%s\ax is not in the database, setting it to Keep", pPersonalItem->Name);
							WritePrivateProfileString(INISection, pPersonalItem->Name, "Keep", szLootINI);
							sprintf_s(action, "Keep");
						}
					}
					else
					{
						if (LootInProgress(pAdvLoot, pPersonalList, pSharedList)) return true;
						CHAR *pParsedToken = NULL;
						CHAR *pParsedValue = strtok_s(action, "|", &pParsedToken);
						if (!_stricmp(pParsedValue, "Keep") || !_stricmp(pParsedValue, "Sell") || !_stricmp(pParsedValue, "Deposit") || !_stricmp(pParsedValue, "Barter") || !_stricmp(pParsedValue, "Quest") || !_stricmp(pParsedValue, "Gear") || !_stricmp(pParsedValue, "Destroy"))
						{
							if (pPersonalItem->LootDetails->Locked || CheckIfItemIsLoreByID(pPersonalItem->ItemID) || !DoIHaveSpace(pPersonalItem->Name, pPersonalItem->MaxStack, pPersonalItem->LootDetails->StackCount))
							{
								if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: PList: \ag%s\ax is lore/locked/I don't have room, setting to leave", pPersonalItem->Name); }
								if (iLogLoot)
								{
									sprintf_s(szTemp, "%s :: PList: %s is lore/locked/I don't have room, setting to leave", pChar->Name, pPersonalItem->Name);
									CreateLogEntry(szTemp);
								}
								LONG LootInd = k + 1;
								sprintf_s(szCommand, "/advloot personal %d leave", LootInd);
								DoCommand(GetCharInfo()->pSpawn, szCommand);
								return true;
							}
							if (!_stricmp(action, "Destroy"))
							{
								if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: PList: looting \ar%s\ax to destoy it", pPersonalItem->Name); }
								if (iLogLoot)
								{
									sprintf_s(szTemp, "%s :: PList: looting %s to destroy it", pChar->Name, pPersonalItem->Name);
									CreateLogEntry(szTemp);
								}
								DestroyID = pPersonalItem->ItemID;
								DestroyStuffCancelTimer = pluginclock::now() + std::chrono::seconds(10);
							}
							if (pPersonalItem->NoDrop) // Adding a 1 second delay to click accept on no drop items
							{
								LootTimer = pluginclock::now() + std::chrono::milliseconds(1000);
							}
							else // Adding a small delay for regular items of 0.2 seconds
							{
								LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
							}
							if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: PList: Setting \ag%s\ax to loot", pPersonalItem->Name); }
							if (iLogLoot)
							{
								sprintf_s(szTemp, "%s :: PList: looting %s", pChar->Name, pPersonalItem->Name);
								CreateLogEntry(szTemp);
							}
							if (CXWnd *pwnd = GetAdvLootPersonalListItem(listindex, 2)) { SendWndClick2(pwnd, "leftmouseup"); } // Loot the item
							return true;
						}
						else if (!_stricmp(pParsedValue, "Ignore"))
						{
							LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
							if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: PList: Setting \ag%s\ax to leave", pPersonalItem->Name); }
							if (iLogLoot)
							{
								sprintf_s(szTemp, "%s :: PList: leaving %s", pChar->Name, pPersonalItem->Name);
								CreateLogEntry(szTemp);
							}
							if (CXWnd *pwnd = GetAdvLootPersonalListItem(listindex, 3)) { SendWndClick2(pwnd, "leftmouseup"); } // Leave the item
							return true;
						}
						else
						{
							if (pPersonalItem->NoDrop)
							{
								CHAR LootEntry[MAX_STRING];
								LootEntry[0] = '\0';
								CHAR *pParsedToken = NULL;
								CHAR *pParsedValue = strtok_s(szNoDropDefault, "|", &pParsedToken);
								if (!_stricmp(pParsedValue, "Quest"))
								{
									sprintf_s(LootEntry, "%s|%d", pParsedValue, iQuestKeep);
								}
								else if (!_stricmp(pParsedValue, "Keep") || !_stricmp(pParsedValue, "Ignore"))
								{
									sprintf_s(LootEntry, "%s", pParsedValue);
								}
								else
								{
									sprintf_s(LootEntry, "Quest|1");
								}
								WriteChatf(PLUGIN_MSG ":: The \ag%s\ax is not in the database, setting it to %s", pPersonalItem->Name, LootEntry);
								WritePrivateProfileString(INISection, pPersonalItem->Name, LootEntry, szLootINI);
								return true;
							}
							else
							{
								if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: The \ag%s\ax was set to \ag%s\ax, changing to Keep", pPersonalItem->Name, pParsedValue); }
								WritePrivateProfileString(INISection, pPersonalItem->Name, "Keep", szLootINI);
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool HandleSharedLoot(bool ItemOnCursor, PCHARINFO pChar, PCHARINFO2 pChar2, PEQADVLOOTWND pAdvLoot, CListWnd *pPersonalList, CListWnd *pSharedList) // Handle items in your shared loot window
{
	if (pSharedList)
	{
		bool MasterLooter = false;
		//If I don't have a masterlooter set and I am leader I will set myself master looter
		if (pRaid && pRaid->RaidMemberCount > 0) // Ok we're in a raid, lets see who should handle loot
		{
			int ml = 0;
			for (int ml = 0; ml < 72; ml++)
			{
				if (pRaid->RaidMemberUsed[ml] && pRaid->RaidMember[ml].MasterLooter)
				{
					if (!_stricmp(pChar->Name, pRaid->RaidMember[ml].Name))
					{
						MasterLooter = true;
					}
					break;
				}
			}
			if (ml == 72)
			{
				if (!_stricmp(pChar->Name, pRaid->RaidLeaderName))
				{
					MasterLooter = true;
				}
			}
		}
		else // Ok so we're not in a raid, maybe we are in a group
		{
			if (pChar->pGroupInfo && pChar->pGroupInfo->pMember && pChar->pGroupInfo->pMember[0])
			{
				int ml = 0;
				for (ml = 0; ml < 6; ml++)
				{
					if (pChar->pGroupInfo->pMember[ml] && pChar->pGroupInfo->pMember[ml]->pName && pChar->pGroupInfo->pMember[ml]->MasterLooter)
					{
						break;
					}
				}
				if (ml == 6 && pChar->pGroupInfo->pLeader && pChar->pGroupInfo->pLeader->pSpawn && pChar->pGroupInfo->pLeader->pSpawn->SpawnID)
				{
					if (pChar->pGroupInfo->pLeader->pSpawn->SpawnID == pChar->pSpawn->SpawnID)  // oh shit we have loot and no master looter set yet and I am the leader, so lets make me the leader
					{
						WriteChatf(PLUGIN_MSG ":: I am setting myself to master looter");
						sprintf_s(szCommand, "/grouproles set %s 5", pChar->Name);
						DoCommand(GetCharInfo()->pSpawn, szCommand);
						LootTimer = pluginclock::now() + std::chrono::seconds(5);  //Two seconds was too short, it attempts to set masterlooter a second time.  Setting to 5 seconds that should fix this
						return true;
					}
				}
				if (pChar->pGroupInfo->pMember[0]->MasterLooter) 
				{ 
					MasterLooter = true; 
				}
			}
			else
			{
				return true; // There is no way this should happen, but incase it does I want the plugin to ignore shit
			}
		}

		//Loop over the item array to find see if I need to set something
		for (LONG k = 0; k < pSharedList->ItemsArray.Count; k++)
		{
			LONG listindex = pSharedList->GetItemData(k);
			if (listindex != -1)
			{
				DWORD multiplier = sizeof(LOOTITEM) * listindex;
				if (PLOOTITEM pShareItem = (PLOOTITEM)(((DWORD)pAdvLoot->pCLootList->pLootItem) + multiplier))
				{
					if (!StartDistributeLootTimer) // I am checking if the item has successfully been passed out
					{
						if (DistributeK == k)
						{
							if (DistributeItemID != pShareItem->ItemID)
							{
								StartDistributeLootTimer = true;
							}
						}
					}
					if (!pShareItem->AutoRoll && !pShareItem->No && !pShareItem->Need && !pShareItem->Greed) // Ok lets check to see if we have any of the boxes check
					{
						if (pShareItem->bAutoRoll && pShareItem->AskTimer > 0 || !pShareItem->bAutoRoll) // Ok if the master looter has set the item to AutoRoll and the AskTimer is greater then 0 I can proceed, otherwise it cause some annoying spamming of the Advanced Loot Window... Thanks to Chatwiththisname
						{
							CHAR INISection[]{ pShareItem->Name[0],'\0' };
							bool IWant = false;  // Will be set true if you want and can accept the item
							bool IDoNotWant = false;  // Will be set true if you don't want or can't accept
							bool CheckIfOthersWant = false;  // Will be set true if I am ML and I can't accept or don't need
							CHAR Value[MAX_STRING];
							Value[0] = '\0';
							if (GetPrivateProfileString(INISection, pShareItem->Name, 0, Value, MAX_STRING, szLootINI) == 0)
							{
								if (pShareItem->NoDrop)
								{
									CHAR LootEntry[MAX_STRING] = { 0 };
									CHAR *pParsedToken = NULL;
									CHAR *pParsedValue = strtok_s(szNoDropDefault, "|", &pParsedToken);
									if (!_stricmp(pParsedValue, "Quest"))
									{
										sprintf_s(LootEntry, "%s|%d", pParsedValue, iQuestKeep);
									}
									else if (!_stricmp(pParsedValue, "Keep") || !_stricmp(pParsedValue, "Ignore"))
									{
										sprintf_s(LootEntry, "%s", pParsedValue);
									}
									else
									{
										sprintf_s(LootEntry, "Quest|1");
									}
									WriteChatf(PLUGIN_MSG ":: The \ag%s\ax is not in the database, setting it to %s", pShareItem->Name, LootEntry);
									WritePrivateProfileString(INISection, pShareItem->Name, LootEntry, szLootINI);
									return true;
								}
								else
								{
									if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: The \ag%s\ax is not in the database, setting it to Keep", pShareItem->Name); }
									WritePrivateProfileString(INISection, pShareItem->Name, "Keep", szLootINI);
									return true;
								}
							}
							else
							{
								CHAR *pParsedToken = NULL;
								CHAR *pParsedValue = strtok_s(Value, "|", &pParsedToken);
								if (!_stricmp(pParsedValue, "Quest"))
								{
									DWORD QuestNumber;
									pParsedValue = strtok_s(NULL, "|", &pParsedToken);
									if (pParsedValue == NULL)
									{
										QuestNumber = 1;
										if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: You did not set the quest number for \ag%s\ax, changing it to Quest|1", pShareItem->Name); }
										WritePrivateProfileString(INISection, pShareItem->Name, "Quest|1", szLootINI);
									}
									else
									{
										if (IsNumber(pParsedValue))
										{
											QuestNumber = atoi(pParsedValue);
										}
									}
									if (ItemOnCursor || pShareItem->LootDetails->Locked || !DoIHaveSpace(pShareItem->Name, pShareItem->MaxStack, pShareItem->LootDetails->StackCount) || CheckIfItemIsLoreByID(pShareItem->ItemID) || QuestNumber <= FindItemCount(pShareItem->Name))
									{
										if (!MasterLooter)
										{
											IDoNotWant = true;
										}
										else
										{
											CheckIfOthersWant = true;
										}
									}
									else
									{
										IWant = true;
									}
								}
								else if (!_stricmp(pParsedValue, "Gear"))
								{
									bool RightClass = false; // Will set true if your class is one of the entries after Gear|Classes|...
									DWORD GearNumber = 0;  // The number of this item to loot
									pParsedValue = strtok_s(NULL, "|", &pParsedToken);
									if (pParsedValue == NULL)
									{
										if (PCONTENTS pItem = FindBankItemByID(pShareItem->ItemID))
										{
											WriteChatf(PLUGIN_MSG ":: Found:\ag%s\ax, in my bank!", pShareItem->Name);
											CreateLootEntry("Gear", "", GetItemFromContents(pItem));
											return true;
										}
										else if (PCONTENTS pItem = FindItemByID(pShareItem->ItemID))
										{
											WriteChatf(PLUGIN_MSG ":: Found:\ag%s\ax, in my packs!", pShareItem->Name);
											CreateLootEntry("Gear", "", GetItemFromContents(pItem));
											return true;
										}
										else
										{
											WriteChatf(PLUGIN_MSG ":: \ag%s\ax hasn't ever had classes set, setting it to loot!", pShareItem->Name);
											IWant = true;
										}
									}
									while (pParsedValue != NULL)
									{
										if (!_stricmp(pParsedValue, ClassInfo[pChar2->Class].UCShortName))
										{
											RightClass = true;
										}
										if (!_stricmp(pParsedValue, "NumberToLoot"))
										{
											pParsedValue = strtok_s(NULL, "|", &pParsedToken);
											if (pParsedValue == NULL)
											{
												GearNumber = 1;
												if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: You did not set the Gear Number for \ag%s\ax, Please change your loot ini entry", pShareItem->Name); }
											}
											else
											{
												if (IsNumber(pParsedValue))
												{
													GearNumber = atoi(pParsedValue);
												}
												break;
											}
										}
										pParsedValue = strtok_s(NULL, "|", &pParsedToken);
									}
									if (ItemOnCursor || pShareItem->LootDetails->Locked || !DoIHaveSpace(pShareItem->Name, pShareItem->MaxStack, pShareItem->LootDetails->StackCount) || CheckIfItemIsLoreByID(pShareItem->ItemID) || !RightClass || GearNumber <= FindItemCount(pShareItem->Name))
									{
										if (!MasterLooter)
										{
											IDoNotWant = true;
										}
										else 
										{
											CheckIfOthersWant = true;
										}
									}
									else
									{
										IWant = true;
									}
								}
								else if (!_stricmp(pParsedValue, "Keep") || !_stricmp(pParsedValue, "Deposit") || !_stricmp(pParsedValue, "Sell") || !_stricmp(pParsedValue, "Barter"))
								{
									if (ItemOnCursor || pShareItem->LootDetails->Locked || !DoIHaveSpace(pShareItem->Name, pShareItem->MaxStack, pShareItem->LootDetails->StackCount) || CheckIfItemIsLoreByID(pShareItem->ItemID))
									{
										if (!MasterLooter)
										{
											IDoNotWant = true;
										}
										else
										{
											CheckIfOthersWant = true;
										}
									}
									else
									{
										IWant = true;
									}
								}
								else if (!_stricmp(Value, "Destroy"))
								{
									if (!MasterLooter)
									{
										IDoNotWant = true;
									}
									else
									{
										if (ItemOnCursor || pShareItem->LootDetails->Locked || !DoIHaveSpace(pShareItem->Name, pShareItem->MaxStack, pShareItem->LootDetails->StackCount) || CheckIfItemIsLoreByID(pShareItem->ItemID))
										{
											IDoNotWant = true;
										}
										else
										{
											IWant = true;
										}
									}
								}
								else if (!_stricmp(pParsedValue, "Ignore"))
								{
									IDoNotWant = true;
								}
								else
								{
									if (pShareItem->NoDrop)
									{
										CHAR LootEntry[MAX_STRING] = { 0 };
										CHAR *pParsedToken = NULL;
										CHAR *pParsedValue = strtok_s(szNoDropDefault, "|", &pParsedToken);
										if (!_stricmp(pParsedValue, "Quest"))
										{
											sprintf_s(LootEntry, "%s|%d", pParsedValue, iQuestKeep);
										}
										else if (!_stricmp(pParsedValue, "Keep") || !_stricmp(pParsedValue, "Ignore"))
										{
											sprintf_s(LootEntry, "%s", pParsedValue);
										}
										else
										{
											sprintf_s(LootEntry, "Quest|1");
										}
										WriteChatf(PLUGIN_MSG ":: The \ag%s\ax is not in the database, setting it to %s", pShareItem->Name, LootEntry);
										WritePrivateProfileString(INISection, pShareItem->Name, LootEntry, szLootINI);
										return true;
									}
									else
									{
										if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: The \ag%s\ax was set to \ag%s\ax, changing to Keep", pShareItem->Name, pParsedValue); }
										WritePrivateProfileString(INISection, pShareItem->Name, "Keep", szLootINI);
										return true;
									}
								}
								if (LootInProgress(pAdvLoot, pPersonalList, pSharedList)) return true;
								if (IWant && MasterLooter)
								{
									//I want and I am the master looter
									if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: SList: Giving \ag%s\ax to me", pShareItem->Name); }
									if (iLogLoot)
									{
										sprintf_s(szTemp, "%s :: SList: looting %s", pChar->Name, pShareItem->Name);
										CreateLogEntry(szTemp);
									}
									LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
									pAdvancedLootWnd->DoSharedAdvLootAction(pShareItem, &CXStr(pChar->Name), 0, pShareItem->LootDetails->StackCount);
									return true;
								}
								else if (IWant && !MasterLooter)
								{
									//I want and i am not the master looter
									if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: SList: Setting \ag%s\ax to need", pShareItem->Name); }
									LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
									if (CXWnd *pwnd = GetAdvLootSharedListItem(listindex, 9)) { SendWndClick2(pwnd, "leftmouseup"); } // Setting to need
									return true;
								}
								else if (IDoNotWant && MasterLooter)
								{
									//I don't want and am the master looter
									if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: SList: Setting \ag%s\ax to leave", pShareItem->Name); }
									if (iLogLoot)
									{
										sprintf_s(szTemp, "%s :: SList: leaving %s", pChar->Name, pShareItem->Name);
										CreateLogEntry(szTemp);
									}
									LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
									pAdvancedLootWnd->DoSharedAdvLootAction(pShareItem, &CXStr(pChar->Name), 1, pShareItem->LootDetails->StackCount);
									return true;
								}
								else if (IDoNotWant && !MasterLooter)
								{
									//I don't want and i am not the master looter
									if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: SList: Setting \ag%s\ax to no", pShareItem->Name); }
									LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
									if (CXWnd *pwnd = GetAdvLootSharedListItem(listindex, 11)) { SendWndClick2(pwnd, "leftmouseup"); } // Setting to no
									return true;
								}
								else if (CheckIfOthersWant)
								{
									DWORD nThreadID = 0;
									CreateThread(NULL, NULL, PassOutLoot, (PVOID)0, 0, &nThreadID);
									return true;
								}
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool WinState(CXWnd *Wnd) // Returns TRUE if the specified UI window is visible
{
	return (Wnd && ((PCSIDLWND)Wnd)->dShow);
}

PMQPLUGIN Plugin(char* PluginName)
{
	long Length = strlen(PluginName) + 1;
	PMQPLUGIN pLook = pPlugins;
	while (pLook && _strnicmp(PluginName, pLook->szFilename, Length)) pLook = pLook->pNext;
	return pLook;
}

bool HandleEQBC(void)
{
	unsigned short sEQBCConnected = 0;
	bool bEQBCLoaded = false;
	unsigned short(*fisConnected)() = NULL;
	fAreTheyConnected = NULL;
	if (PMQPLUGIN pLook = Plugin("mq2eqbc"))
	{
		fisConnected = (unsigned short(*)())GetProcAddress(pLook->hModule, "isConnected");
		fAreTheyConnected = (bool(*)(char* szName))GetProcAddress(pLook->hModule, "AreTheyConnected");
		bEQBCLoaded = true;
	}
	if (fisConnected && bEQBCLoaded)
	{
		sEQBCConnected = fisConnected();
	}
	if (fisConnected && fAreTheyConnected && sEQBCConnected && bEQBCLoaded)
	{
		return true;
	}
	return false;
}

bool DoIHaveSpace(CHAR* pszItemName, DWORD pdMaxStackSize, DWORD pdStackSize)
{
	bool FitInStack = false;
	LONG nPack = 0;
	LONG Count = 0;
	PCHARINFO pCharInfo = GetCharInfo();
	PCHARINFO2 pChar2 = GetCharInfo2();

	if (pChar2 && pChar2->pInventoryArray && pChar2->pInventoryArray->InventoryArray) //check my inventory slots
	{
		for (unsigned long nSlot = BAG_SLOT_START; nSlot < NUM_INV_SLOTS; nSlot++)
		{
			if (PCONTENTS pItem = pChar2->pInventoryArray->InventoryArray[nSlot])
			{
				if (PITEMINFO theitem = GetItemFromContents(pItem))
				{
					if (!_stricmp(pszItemName, theitem->Name))
					{
						if ((theitem->Type != ITEMTYPE_NORMAL) || (((EQ_Item*)pItem)->IsStackable() != 1))
						{

						}
						else
						{
							if (pdStackSize + pItem->StackCount <= pdMaxStackSize)
							{
								FitInStack = true;
							}
						}
					}
				}
			}
			else
			{
				Count++;
			}
		}
	}
	if (pChar2 && pChar2->pInventoryArray) //Checking my bags
	{
		for (unsigned long nPack = 0; nPack < 10; nPack++)
		{
			if (PCONTENTS pPack = pChar2->pInventoryArray->Inventory.Pack[nPack])
			{
				if (PITEMINFO pItemPack = GetItemFromContents(pPack))
				{
					if (pItemPack->Type == ITEMTYPE_PACK && (_stricmp(pItemPack->Name, szExcludedBag1) || _stricmp(pItemPack->Name, szExcludedBag2)))
					{
						if (pPack->Contents.ContainedItems.pItems)
						{
							for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
							{
								if (PCONTENTS pItem = pPack->Contents.ContainedItems.pItems->Item[nItem])
								{
									if (PITEMINFO theitem = GetItemFromContents(pItem))
									{
										if (!_stricmp(pszItemName, theitem->Name))
										{
											if ((theitem->Type != ITEMTYPE_NORMAL) || (((EQ_Item*)pItem)->IsStackable() != 1))
											{
											}
											else
											{
												if (pdStackSize + pItem->StackCount <= pdMaxStackSize)
												{
													FitInStack = true;
												}
											}
										}
									}
								}
								else
								{
									Count++;
								}
							}
						}
						else
						{
							Count = Count + pItemPack->Slots;
						}
					}
				}
			}
		}
	}
	if (Count > iSaveBagSlots)
	{
		return true;
	}
	else if (FitInStack)
	{
		return true;
	}
	return false;
}

bool FitInInventory(DWORD pdItemSize)
{
	bool FitInStack = false;
	LONG nPack = 0;
	LONG Count = 0;
	PCHARINFO pCharInfo = GetCharInfo();
	PCHARINFO2 pChar2 = GetCharInfo2();
	if (pChar2 && pChar2->pInventoryArray && pChar2->pInventoryArray->InventoryArray) //check my inventory slots
	{
		for (unsigned long nSlot = BAG_SLOT_START; nSlot < NUM_INV_SLOTS; nSlot++)
		{
			if (PCONTENTS pItem = pChar2->pInventoryArray->InventoryArray[nSlot])
			{

			}
			else
			{
				return true;
			}
		}
	}
	if (pChar2 && pChar2->pInventoryArray) //Checking my bags
	{
		for (unsigned long nPack = 0; nPack < 10; nPack++)
		{
			if (PCONTENTS pPack = pChar2->pInventoryArray->Inventory.Pack[nPack])
			{
				if (PITEMINFO pItemPack = GetItemFromContents(pPack))
				{
					if (pItemPack->Type == ITEMTYPE_PACK && (_stricmp(pItemPack->Name, szExcludedBag1) || _stricmp(pItemPack->Name, szExcludedBag2)))
					{
						if (pPack->Contents.ContainedItems.pItems)
						{
							for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
							{
								if (PCONTENTS pItem = pPack->Contents.ContainedItems.pItems->Item[nItem])
								{

								}
								else
								{
									if (pItemPack->SizeCapacity >= pdItemSize)
									{
										return true;
									}
								}
							}
						}
						else
						{
							if (pItemPack->SizeCapacity >= pdItemSize)
							{
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

int CheckIfItemIsLoreByID(int ItemID)
{
	if (PCONTENTS pItem = FindItemByID(ItemID)) return GetItemFromContents(FindItemByID(ItemID))->Lore;
	if (PCONTENTS pItem = FindBankItemByID(ItemID)) return GetItemFromContents(FindBankItemByID(ItemID))->Lore;
	return 0;
}

DWORD FindItemCount(CHAR* pszItemName)
{
	LONG nPack = 0;
	DWORD Count = 0;
	DWORD nAug = 0;
	//PCONTENTS pPack;
	PCHARINFO pCharInfo = GetCharInfo();
	PCHARINFO2 pChar2 = GetCharInfo2();
	if (pChar2 && pChar2->pInventoryArray && pChar2->pInventoryArray->InventoryArray) //check my inventory slots
	{
		for (unsigned long nSlot = 0; nSlot < NUM_INV_SLOTS; nSlot++) //NUM_INV_SLOTS == 33
		{
			if (PCONTENTS pItem = pChar2->pInventoryArray->InventoryArray[nSlot])
			{
				if (PITEMINFO theitem = GetItemFromContents(pItem))
				{
					if (!_stricmp(pszItemName, theitem->Name))
					{
						if ((theitem->Type != ITEMTYPE_NORMAL) || (((EQ_Item*)pItem)->IsStackable() != 1))
							Count++;
						else
							Count += pItem->StackCount;
					}
					else if (theitem->Type != ITEMTYPE_PACK)// for augs
					{
						if (pItem->Contents.ContainedItems.pItems && pItem->Contents.ContainedItems.Size)
						{
							for (nAug = 0; nAug < pItem->Contents.ContainedItems.Size; nAug++)
							{
								if (pItem->Contents.ContainedItems.pItems->Item[nAug])
								{
									if (PITEMINFO augitem = GetItemFromContents(pItem->Contents.ContainedItems.pItems->Item[nAug]))
									{
										if (augitem->Type == ITEMTYPE_NORMAL && augitem->AugType && !_stricmp(pszItemName, augitem->Name))
										{
											Count++;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if (pChar2 && pChar2->pInventoryArray) //Checking my bags
	{
		for (unsigned long nPack = 0; nPack < 10; nPack++)
		{
			if (PCONTENTS pPack = pChar2->pInventoryArray->Inventory.Pack[nPack])
			{
				if (PITEMINFO pItemPack = GetItemFromContents(pPack))
				{
					if (pItemPack->Type == ITEMTYPE_PACK && pPack->Contents.ContainedItems.pItems)
					{
						for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
						{
							if (PCONTENTS pItem = pPack->Contents.ContainedItems.pItems->Item[nItem])
							{
								if (PITEMINFO theitem = GetItemFromContents(pItem))
								{
									if (!_stricmp(pszItemName, theitem->Name))
									{
										if ((theitem->Type != ITEMTYPE_NORMAL) || (((EQ_Item*)pItem)->IsStackable() != 1))
											Count++;
										else
											Count += pItem->StackCount;
									}
									else //check for augs
									{
										if (pItem->Contents.ContainedItems.pItems && pItem->Contents.ContainedItems.Size)
										{
											for (nAug = 0; nAug < pItem->Contents.ContainedItems.Size; nAug++)
											{
												if (pItem->Contents.ContainedItems.pItems->Item[nAug])
												{
													if (PITEMINFO pAug = GetItemFromContents(pItem->Contents.ContainedItems.pItems->Item[nAug]))
													{
														if (pAug->Type == ITEMTYPE_NORMAL && pAug->AugType && !_stricmp(pszItemName, pAug->Name))
														{
															Count++;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if (pCharInfo && pCharInfo->pBankArray) //checking bank slots
	{
		for (nPack = 0; nPack < NUM_BANK_SLOTS; nPack++)
		{
			if (PCONTENTS pPack = pCharInfo->pBankArray->Bank[nPack])
			{
				if (GetItemFromContents(pPack)->Type != ITEMTYPE_PACK)  //It isn't a pack! we should see if this item is what we are looking for
				{
					if (PITEMINFO theitem = GetItemFromContents(pPack))
					{
						if (!_stricmp(pszItemName, theitem->Name))
						{
							if ((theitem->Type != ITEMTYPE_NORMAL) || (((EQ_Item*)pPack)->IsStackable() != 1))
								Count++;
							else
								Count += pPack->StackCount;
						}
						else //check for augs
						{
							if (pPack->Contents.ContainedItems.pItems && pPack->Contents.ContainedItems.Size)
							{
								for (nAug = 0; nAug < pPack->Contents.ContainedItems.Size; nAug++)
								{
									if (pPack->Contents.ContainedItems.pItems->Item[nAug])
									{
										if (PITEMINFO pAug = GetItemFromContents(pPack->Contents.ContainedItems.pItems->Item[nAug]))
										{
											if (pAug->Type == ITEMTYPE_NORMAL && pAug->AugType && !_stricmp(pszItemName, pAug->Name))
											{
												Count++;
											}
										}
									}
								}
							}
						}
					}
				}
				else
				{
					if (pPack->Contents.ContainedItems.pItems)
					{
						if (PITEMINFO pItemPack = GetItemFromContents(pPack))
						{
							for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
							{
								if (PCONTENTS pItem = pPack->Contents.ContainedItems.pItems->Item[nItem])
								{
									if (PITEMINFO theitem = GetItemFromContents(pItem))
									{
										if (!_stricmp(pszItemName, theitem->Name))
										{
											if ((theitem->Type != ITEMTYPE_NORMAL) || (((EQ_Item*)pItem)->IsStackable() != 1))
												Count++;
											else
												Count += pItem->StackCount;
										}
										else //check for augs
										{
											if (pItem->Contents.ContainedItems.pItems && pItem->Contents.ContainedItems.Size)
											{
												for (nAug = 0; nAug < pItem->Contents.ContainedItems.Size; nAug++)
												{
													if (pItem->Contents.ContainedItems.pItems->Item[nAug])
													{
														if (PITEMINFO pAug = GetItemFromContents(pItem->Contents.ContainedItems.pItems->Item[nAug]))
														{
															if (pAug->Type == ITEMTYPE_NORMAL && pAug->AugType && !_stricmp(pszItemName, pAug->Name))
															{
																Count++;
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	if (pCharInfo && pCharInfo->pSharedBankArray) //checking shared bank slots
	{
		for (nPack = 0; nPack < NUM_SHAREDBANK_SLOTS; nPack++)
		{
			if (PCONTENTS pPack = pCharInfo->pSharedBankArray->SharedBank[nPack])
			{
				if (GetItemFromContents(pPack)->Type != ITEMTYPE_PACK)  //It isn't a pack! we should see if this item is what we are looking for
				{
					if (PITEMINFO theitem = GetItemFromContents(pPack))
					{
						if (!_stricmp(pszItemName, theitem->Name))
						{
							if ((theitem->Type != ITEMTYPE_NORMAL) || (((EQ_Item*)pPack)->IsStackable() != 1))
								Count++;
							else
								Count += pPack->StackCount;
						}
						else //check for augs
						{
							if (pPack->Contents.ContainedItems.pItems && pPack->Contents.ContainedItems.Size)
							{
								for (nAug = 0; nAug < pPack->Contents.ContainedItems.Size; nAug++)
								{
									if (pPack->Contents.ContainedItems.pItems->Item[nAug])
									{
										if (PITEMINFO pAug = GetItemFromContents(pPack->Contents.ContainedItems.pItems->Item[nAug]))
										{
											if (pAug->Type == ITEMTYPE_NORMAL && pAug->AugType && !_stricmp(pszItemName, pAug->Name))
											{
												Count++;
											}
										}
									}
								}
							}
						}
					}
				}
				else
				{
					if (pPack->Contents.ContainedItems.pItems)
					{
						if (PITEMINFO pItemPack = GetItemFromContents(pPack))
						{
							for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
							{
								if (PCONTENTS pItem = pPack->Contents.ContainedItems.pItems->Item[nItem])
								{
									if (PITEMINFO theitem = GetItemFromContents(pItem))
									{
										if (!_stricmp(pszItemName, theitem->Name))
										{
											if ((theitem->Type != ITEMTYPE_NORMAL) || (((EQ_Item*)pItem)->IsStackable() != 1))
												Count++;
											else
												Count += pItem->StackCount;
										}
										else //check for augs
										{
											if (pItem->Contents.ContainedItems.pItems && pItem->Contents.ContainedItems.Size)
											{
												for (nAug = 0; nAug < pItem->Contents.ContainedItems.Size; nAug++)
												{
													if (pItem->Contents.ContainedItems.pItems->Item[nAug])
													{
														if (PITEMINFO pAug = GetItemFromContents(pItem->Contents.ContainedItems.pItems->Item[nAug]))
														{
															if (pAug->Type == ITEMTYPE_NORMAL && pAug->AugType && !_stricmp(pszItemName, pAug->Name))
															{
																Count++;
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return Count;
}

DWORD __stdcall PassOutLoot(PVOID pData)
{
	if (!InGameOK()) { return 0; }
	PCHARINFO pChar = GetCharInfo();
	if (!WinState((CXWnd*)pAdvancedLootWnd)) { return 0; }
	PEQADVLOOTWND pAdvLoot = (PEQADVLOOTWND)pAdvancedLootWnd;
	if (!pAdvLoot) { return 0; }
	CListWnd *pPersonalList = (CListWnd *)pAdvancedLootWnd->GetChildItem("ADLW_PLLList");
	CListWnd *pSharedList = (CListWnd *)pAdvLoot->pCLootList->SharedLootList;
	if (LootInProgress(pAdvLoot, pPersonalList, pSharedList)) { return 0; }
	if (pSharedList->GetItemData(0) != -1)
	{
		DWORD multiplier = sizeof(LOOTITEM) * pSharedList->GetItemData(0);
		if (PLOOTITEM pShareItem = (PLOOTITEM)(((DWORD)pAdvLoot->pCLootList->pLootItem) + multiplier))
		{
			if (iLogLoot)
			{
				if (pRaid && pRaid->RaidMemberCount > 0) // Ok we're in a raid, lets see who should handle loot
				{
					sprintf_s(szTemp, "%s :: SList: Attempting to pass out %s to my raid members", pChar->Name, pShareItem->Name);
				}
				else
				{
					sprintf_s(szTemp, "%s :: SList: Attempting to pass out %s to my group members", pChar->Name, pShareItem->Name);
				}
				CreateLogEntry(szTemp);
			}
			if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: SList: Setting the DistributeLootTimer to \ag%d\ax seconds", iDistributeLootDelay); }
			LootTimer = pluginclock::now() + std::chrono::seconds(iDistributeLootDelay) + std::chrono::seconds(30); // Lets lock out the plugin from doing loot actions while we attempt to pass out items
			pluginclock::time_point	DistributeLootTimer = pluginclock::now() + std::chrono::seconds(iDistributeLootDelay);
			while (pluginclock::now() < DistributeLootTimer) // While loop to wait for DistributeLootDelay to time out
			{
				Sleep(100);
			}
			if (!InGameOK() || !WinState((CXWnd*)pAdvancedLootWnd))
			{
				LootTimer = pluginclock::now();
				return 0;
			}
			if (pRaid && pRaid->RaidMemberCount > 0) // Ok we're in a raid, lets see who should handle loot
			{
				for (int nMember = 0; nMember < 72; nMember++)
				{
					if (pRaid->RaidMemberUsed[nMember]) // Ok this raid slot has a character in it 
					{					
						if (_stricmp(pChar->Name, pRaid->RaidMember[nMember].Name)) // The character isn't me
						{
							if (GetSpawnByName(pRaid->RaidMember[nMember].Name)) // The character is in the zone
							{
								if (DistributeLoot(pRaid->RaidMember[nMember].Name, pShareItem))
								{
									LootTimer = pluginclock::now();
									return 0;
								}
							}
						}
					}
				}
			}
			else // Ok so we're not in a raid, maybe we are in a group
			{
				for (int nMember = 1; nMember < 6; nMember++) // Lets start at 1, since I am in position 0
				{
					if (pChar->pGroupInfo)
					{
						if (pChar->pGroupInfo->pMember[nMember] && pChar->pGroupInfo->pMember[nMember]->pName && !pChar->pGroupInfo->pMember[nMember]->Mercenary)
						{
							if (!pChar->pGroupInfo->pMember[nMember]->Offline) // They aren't offline
							{
								if (pChar->pGroupInfo->pMember[nMember]->pSpawn)
								{
									if (DistributeLoot(pChar->pGroupInfo->pMember[nMember]->pSpawn->Name, pShareItem))
									{
										LootTimer = pluginclock::now();
										return 0;
									}
								}
							}
						}
					}
				}
			}
			if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: SList: No one wanted \ag%s\ax setting to leave", pShareItem->Name); }
			if (iLogLoot)
			{
				sprintf_s(szTemp, "%s :: SList: Attempted to pass out %s and no one wanted, leaving it on the corpse", pChar->Name, pShareItem->Name);
				CreateLogEntry(szTemp);
			}
			LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
			pAdvancedLootWnd->DoSharedAdvLootAction(pShareItem, &CXStr(pChar->Name), 1, pShareItem->LootDetails->StackCount); // Leaving the item on the corpse
		}
	}
	LootTimer = pluginclock::now();
	return 0;
}

bool DistributeLoot(CHAR* szName, PLOOTITEM pShareItem)
{
	bDistributeItemSucceeded = false;
	bDistributeItemFailed = false;
	if (!InGameOK()) { return true; }
	PCHARINFO pChar = GetCharInfo();
	if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: SList: Attempting to give \ag%s\ax to \ag%s\ax", pShareItem->Name, szName); }
	if (iLogLoot)
	{
		sprintf_s(szTemp, "%s :: SList: Attempting to distribute %s to %s", pChar->Name, pShareItem->Name, szName);
		CreateLogEntry(szTemp);
	}
	if (!WinState((CXWnd*)pAdvancedLootWnd)) { return true; }
	PEQADVLOOTWND pAdvLoot = (PEQADVLOOTWND)pAdvancedLootWnd;
	if (!pAdvLoot) { return true; }
	CListWnd *pPersonalList = (CListWnd *)pAdvancedLootWnd->GetChildItem("ADLW_PLLList");
	CListWnd *pSharedList = (CListWnd *)pAdvLoot->pCLootList->SharedLootList;
	LootTimer = pluginclock::now() + std::chrono::seconds(30); // Lets lock out the plugin from doing loot actions while we attempt to pass out items
	pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until I have an item in my cursor
	while (pluginclock::now() < WhileTimer) // While loop to wait till we are done with the previous looting command
	{
		if (!InGameOK() || !WinState((CXWnd*)pAdvancedLootWnd)) { return true; }
		if (!LootInProgress(pAdvLoot, pPersonalList, pSharedList)) { WhileTimer = pluginclock::now(); }
		Sleep(10);  // Sleep for 10 ms and lets check the previous conditions again
	}
	if (LootInProgress(pAdvLoot, pPersonalList, pSharedList)) { return true; }
	if (pSharedList->GetItemData(0) != -1)
	{
		DWORD multiplier = sizeof(LOOTITEM) * pSharedList->GetItemData(0);
		if (PLOOTITEM pShareItemNew = (PLOOTITEM)(((DWORD)pAdvLoot->pCLootList->pLootItem) + multiplier))
		{
			if (pShareItemNew->ItemID == pShareItem->ItemID)
			{
				bDistributeItemSucceeded = false;
				bDistributeItemFailed = false;
				pAdvancedLootWnd->DoSharedAdvLootAction(pShareItemNew, &CXStr(szName), 0, pShareItemNew->LootDetails->StackCount);
				pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until I have an item in my cursor
				while (pluginclock::now() < WhileTimer) // While loop to wait till we are done with the previous looting command
				{
					if (!InGameOK() || !WinState((CXWnd*)pAdvancedLootWnd)) { return true; }
					if (bDistributeItemSucceeded) { return true; }
					if (bDistributeItemFailed) { return false; }
					Sleep(10);  // Sleep for 10 ms and lets check the previous conditions again
				}
			}
		}
	}
	return true;
}

bool DirectoryExists(LPCTSTR lpszPath) {
	DWORD dw = ::GetFileAttributes(lpszPath);
	return (dw != INVALID_FILE_ATTRIBUTES && (dw & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

void CreateLogEntry(PCHAR szLogEntry)
{
	if (!DirectoryExists(szLogPath))
	{
		CreateDirectory(szLogPath, NULL);
		if (!DirectoryExists(szLogPath)) return;  //Shit i tried to create the directory and failed for some reason
	}
	FILE *fOut = NULL;
	CHAR szBuffer[MAX_STRING] = { 0 };
	DWORD i;

	for (i = 0; i<strlen(szLogFileName); i++) 
	{
		if (szLogFileName[i] == '\\') 
		{
			strncpy_s(szBuffer, szLogFileName, i);
		}
	}

	errno_t err = fopen_s(&fOut, szLogFileName, "at");
	if (err) 
	{
		WriteChatf(PLUGIN_MSG ":: Couldn't open log file:");
		WriteChatf(PLUGIN_MSG ":: \ar%s\ax", szLogFileName);
		return;
	}
	char tmpbuf[128];
	tm today = { 0 };
	time_t tm;
	tm = time(NULL);
	localtime_s(&today, &tm);
	strftime(tmpbuf, 128, "%Y/%m/%d %H:%M:%S", &today);
	sprintf_s(szBuffer, "[%s] %s", tmpbuf, szLogEntry);
	for (unsigned int i = 0; i < strlen(szBuffer); i++) 
	{
		if (szBuffer[i] == 0x07) 
		{
			if ((i + 2) < strlen(szBuffer))
				strcpy_s(&szBuffer[i], MAX_STRING, &szBuffer[i + 2]);
			else
				szBuffer[i] = 0;
		}
	}
	fprintf(fOut, "%s\n", szBuffer);
	fclose(fOut);
}

/********************************************************************************
****
**     Functions necessary for commands
****
********************************************************************************/

void AutoLootCommand(PSPAWNINFO pCHAR, PCHAR szLine)
{
	if (!InGameOK()) return;
	bool ShowInfo = false;
	bool NeedHelp = false;
	char Parm1[MAX_STRING];
	char Parm2[MAX_STRING];
	char Parm3[MAX_STRING];
	GetArg(Parm1, szLine, 1);
	GetArg(Parm2, szLine, 2);
	GetArg(Parm3, szLine, 3);

	if (!_stricmp(Parm1, "help"))
	{
		NeedHelp = true;
	}
	else if (!_stricmp(Parm1, "turn"))
	{
		if (!_stricmp(Parm2, "on")) iUseAutoLoot = SetBOOL(iUseAutoLoot, Parm2, GetCharInfo()->Name, "UseAutoLoot", INIFileName);
		if (!_stricmp(Parm2, "off"))
		{
			iUseAutoLoot = SetBOOL(iUseAutoLoot, Parm2, GetCharInfo()->Name, "UseAutoLoot", INIFileName);
			if (!bEndThreads)
			{
				bEndThreads = true;
				return;
			}
		}
		WriteChatf(PLUGIN_MSG ":: Set %s", iUseAutoLoot ? "\agON\ax" : "\arOFF\ax");
	}
	else if (!_stricmp(Parm1, "spamloot"))
	{
		if (!_stricmp(Parm2, "on")) iSpamLootInfo = SetBOOL(iSpamLootInfo, Parm2, "Settings", "SpamLootInfo", szLootINI);
		if (!_stricmp(Parm2, "off")) iSpamLootInfo = SetBOOL(iSpamLootInfo, Parm2, "Settings", "SpamLootInfo", szLootINI);
		WriteChatf(PLUGIN_MSG ":: Spam looting actions %s", iSpamLootInfo ? "\agON\ax" : "\arOFF\ax");
	}
	else if (!_stricmp(Parm1, "logloot"))
	{
		if (!_stricmp(Parm2, "on")) iLogLoot = SetBOOL(iLogLoot, Parm2, "Settings", "LogLoot", szLootINI);
		if (!_stricmp(Parm2, "off")) iLogLoot = SetBOOL(iLogLoot, Parm2, "Settings", "LogLoot", szLootINI);
		WriteChatf(PLUGIN_MSG ":: Logging loot actions for master looter is %s", iLogLoot ? "\agON\ax" : "\arOFF\ax");
	}
	else if (!_stricmp(Parm1, "raidloot"))
	{
		if (!_stricmp(Parm2, "on")) iRaidLoot = SetBOOL(iRaidLoot, Parm2, "Settings", "RaidLoot", szLootINI);
		if (!_stricmp(Parm2, "off")) iRaidLoot = SetBOOL(iRaidLoot, Parm2, "Settings", "RaidLoot", szLootINI);
		WriteChatf(PLUGIN_MSG ":: Raid looting is turned: %s", iRaidLoot ? "\agON\ax" : "\arOFF\ax");
	}
	else if (!_stricmp(Parm1, "barterminimum")) {
		if (IsNumber(Parm2))
		{
			iBarMinSellPrice = atoi(Parm2);
			WritePrivateProfileString("Settings", "BarMinSellPrice", Parm2, szLootINI);
			WriteChatf(PLUGIN_MSG ":: Stop looting when \ag%d\ax slots are left", iBarMinSellPrice);
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: Please send a valid number for your minimum barter price");
		}
	}
	else if (!_stricmp(Parm1, "saveslots"))
	{
		if (IsNumber(Parm2))
		{
			iSaveBagSlots = atoi(Parm2);
			WritePrivateProfileString("Settings", "SaveBagSlots", Parm2, szLootINI);
			WriteChatf(PLUGIN_MSG ":: Stop looting when \ag%d\ax slots are left", iSaveBagSlots);
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: Please send a valid number for the number of bag slots to save");
		}
	}
	else if (!_stricmp(Parm1, "distributedelay"))
	{
		if (IsNumber(Parm2))
		{
			iDistributeLootDelay = atoi(Parm2);
			WritePrivateProfileString("Settings", "DistributeLootDelay", Parm2, szLootINI);
			WriteChatf(PLUGIN_MSG ":: The master looter will wait \ag%d\ax seconds before trying to distribute loot", iDistributeLootDelay);
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: Please send a valid number for the distribute loot delay");
		}
	}
	else if (!_stricmp(Parm1, "cursordelay"))
	{
		if (IsNumber(Parm2))
		{
			iCursorDelay = atoi(Parm2);
			WritePrivateProfileString("Settings", "CursorDelay", Parm2, szLootINI);
			WriteChatf(PLUGIN_MSG ":: You will wait \ag%d\ax seconds before trying to autoinventory items on your cursor", iCursorDelay);
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: Please send a valid number for the cursor delay");
		}
	}
	else if (!_stricmp(Parm1, "questkeep"))
	{
		if (IsNumber(Parm2))
		{
			iQuestKeep = atoi(Parm2);
			WritePrivateProfileString("Settings", "QuestKeep", Parm2, szLootINI);
			WriteChatf(PLUGIN_MSG ":: Your default number to keep of new no drop items is: \ag%d\ax", iQuestKeep);
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: Please send a valid number for default number of quest items to keep");
		}
	}
	else if (!_stricmp(Parm1, "nodropdefault"))
	{
		if (!_stricmp(Parm2, "quest"))
		{
			sprintf_s(szNoDropDefault, "Quest");
		}
		else if (!_stricmp(Parm2, "keep"))
		{
			sprintf_s(szNoDropDefault, "Keep");
		}
		else if (!_stricmp(Parm2, "ignore"))
		{
			sprintf_s(szNoDropDefault, "Ignore");
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: \ar%s\ax is an invalid entry, please use [quest|keep|ignore]", szNoDropDefault);
			return;
		}
		WritePrivateProfileString("Settings", "NoDropDefault", szNoDropDefault, szLootINI);
		WriteChatf(PLUGIN_MSG ":: Your default for new no drop items is: \ag%s\ax", szNoDropDefault);
	}
	else if (!_stricmp(Parm1, "guilditempermission"))
	{
		// GBANK_PermissionCombo [0] = View Only
		// GBANK_PermissionCombo [1] = Single Member -> Don't do this one, as it throws up a pop up
		// GBANK_PermissionCombo [2] = Public If Usable	
		// GBANK_PermissionCombo [3] = Public
		if (!_stricmp(Parm2, "view only"))
		{
			sprintf_s(szGuildItemPermission, "View Only");
		}
		else if (!_stricmp(Parm2, "public if usable"))
		{
			sprintf_s(szGuildItemPermission, "Public if Usable");
		}
		else if (!_stricmp(Parm2, "public"))
		{
			sprintf_s(szGuildItemPermission, "Public");
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: \ar%s\ax is an invalid entry, please use [view only|public if usable|public]", szGuildItemPermission);
			return;
		}
		WritePrivateProfileString("Settings", "GuildItemPermission", szGuildItemPermission, szLootINI);
		WriteChatf(PLUGIN_MSG ":: Your default permission for items put into your guild bank is: \ag%s\ax", szGuildItemPermission);
	}
	else if (!_stricmp(Parm1, "excludedbag1"))
	{
		WritePrivateProfileString("Settings", "ExcludeBag1", Parm2, szLootINI);
		sprintf_s(szExcludedBag1, "%s", Parm2);
		WriteChatf(PLUGIN_MSG ":: Will exclude \ar%s\ax when checking for free slots", szExcludedBag1);
	}
	else if (!_stricmp(Parm1, "excludedbag2"))
	{
		WritePrivateProfileString("Settings", "ExcludeBag2", Parm2, szLootINI);
		sprintf_s(szExcludedBag2, "%s", Parm2);
		WriteChatf(PLUGIN_MSG ":: Will exclude \ar%s\ax when checking for free slots", szExcludedBag2);
	}
	else if (!_stricmp(Parm1, "lootini"))
	{
		sprintf_s(szLootINI, "%s\\Macros\\%s.ini", gszINIPath, Parm2);
		WritePrivateProfileString(GetCharInfo()->Name, "lootini", szLootINI, INIFileName);
		WriteChatf(PLUGIN_MSG ":: The location for your loot ini is:\n \ag%s\ax", szLootINI);
		SetAutoLootVariables();
	}
	else if (!_stricmp(Parm1, "reload"))
	{
		SetAutoLootVariables();
	}
	else if (!_stricmp(Parm1, "barter"))
	{
		if (!bEndThreads)
		{
			bEndThreads = true;
			return;
		}
		DWORD nThreadID = 0;
		CreateThread(NULL, NULL, BarterItems, (PVOID)0, 0, &nThreadID);
	}
	else if (!_stricmp(Parm1, "buy"))
	{
		if (!bEndThreads)
		{
			bEndThreads = true;
			return;
		}
		CHAR	szTemp1[MAX_STRING] = { 0 };
		sprintf_s(szTemp1, "%s|%s", Parm2, Parm3);
		if (IsNumber(Parm3))
		{
			DWORD nThreadID = 0;
			CreateThread(NULL, NULL, BuyItem, _strdup(szTemp1), 0, &nThreadID);
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: Invalid buy command");
		}
	}
	else if (!_stricmp(Parm1, "sell"))
	{
		if (!bEndThreads)
		{
			bEndThreads = true;
			return;
		}
		DWORD nThreadID = 0;
		CreateThread(NULL, NULL, SellItems, (PVOID)0, 0, &nThreadID);
	}
	else if (!_stricmp(Parm1, "deposit"))
	{
		if (!bEndThreads)
		{
			bEndThreads = true;
			return;
		}
		if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
		{
			if (psTarget->mActorClient.Class == PERSONALBANKER_CLASS)
			{
				DWORD nThreadID = 0;
				CreateThread(NULL, NULL, DepositPersonalBanker, (PVOID)0, 0, &nThreadID);
			}
			else if (psTarget->mActorClient.Class == GUILDBANKER_CLASS)
			{
				DWORD nThreadID = 0;
				CreateThread(NULL, NULL, DepositGuildBanker, (PVOID)0, 0, &nThreadID);
			}
			else
			{
				WriteChatf(PLUGIN_MSG ":: Please target a guild/personal banker!");
			}
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: Please target a guild/personal banker!");
		}
	}
	else if (!_stricmp(Parm1, "test"))
	{
		WriteChatf(PLUGIN_MSG ":: Testing stuff, please ignore this command.  I will remove it later once plugin is done");
		if (pRaid && pRaid->RaidMemberCount > 0) // Ok we're in a raid, lets see who should handle loot
		{
			for (int nMember = 0; nMember < 72; nMember++) // Lets start at 1, since I am in position 0
			{
				if (pRaid->RaidMemberUsed[nMember]) // Ok this raid slot has a character in it 
				{
					WriteChatf(PLUGIN_MSG ":: \ag%s\ax is in raid slot: %d", pRaid->RaidMember[nMember].Name, nMember);
				}
			}
		}
	}
	else if (!_stricmp(Parm1, "status"))
	{
		ShowInfo = true;
	}
	else if (!_stricmp(Parm1, "sort"))
	{
		sort_auto_loot(string(szLootINI), [](auto msg) {WriteChatf(PLUGIN_MSG ":: %s", msg.c_str()); });
	}
	else
	{
		NeedHelp = true;
	}
	if (NeedHelp) {
		WriteChatColor("Usage:");
		WriteChatColor("/AutoLoot -> displays settings");
		WriteChatColor("/AutoLoot reload -> AutoLoot will reload variables from MQ2AutoLoot.ini");
		WriteChatColor("/AutoLoot turn [on|off] -> Toggle autoloot");
		WriteChatColor("/AutoLoot spamloot [on|off] -> Toggle loot action messages");
		WriteChatColor("/AutoLoot logloot [on|off] -> Toggle logging of loot actions for the master looter");
		WriteChatColor("/AutoLoot raidloot [on|off] -> Toggle raid looting on and off");
		WriteChatColor("/AutoLoot saveslots #n -> Stops looting when #n slots are left");
		WriteChatColor("/AutoLoot distributedelay #n -> Master looter waits #n seconds to try and distribute the loot");
		WriteChatColor("/AutoLoot cursordelay #n -> You will wait #n seconds before trying to autoinventory items on your cursor");
		WriteChatColor("/AutoLoot barterminimum #n -> The minimum price for all items to be bartered is #n");
		WriteChatColor("/AutoLoot questkeep #n -> If nodropdefault is set to quest your new no drop items will be set to Quest|#n");
		WriteChatColor("/AutoLoot nodropdefault [quest|keep|ignore] -> Will set new no drop items to this value");
		WriteChatColor("/AutoLoot excludedbag1 XXX -> Will exclude bags named XXX when checking for how many slots you are free");
		WriteChatColor("/AutoLoot excludedbag2 XXX -> Will exclude bags named XXX when checking for how many slots you are free");
		WriteChatColor("/AutoLoot lootini FILENAME -> Will set your loot ini as FILENAME.ini in your macro folder");
		WriteChatColor("/AutoLoot sell -> If you have targeted a merchant, it will sell all the items marked Sell in your inventory");
		WriteChatColor("/AutoLoot deposit -> If you have your personal banker targetted it will put all items marked Keep into your bank");
		WriteChatColor("/AutoLoot deposit -> If you have your guild banker targetted it will put all items marked Deposit into your guild bank");
		WriteChatColor("/AutoLoot buy \"Item Name\" #n -> Will buy #n of \"Item Name\" from the merchant");
		WriteChatColor("/AutoLoot guilditempermission \"[view only|public if usable|public]\" -> Change your default permission for items put into your guild bank");
		WriteChatColor("/AutoLoot status -> Shows the settings for MQ2AutoLoot.");
		WriteChatColor("/AutoLoot sort -> Sort the Loot.ini file.");
		WriteChatColor("/AutoLoot help");
	}
	if (ShowInfo) {
		WriteChatf(PLUGIN_MSG ":: AutoLoot is %s", iUseAutoLoot ? "\agON\ax" : "\arOFF\ax");
		WriteChatf(PLUGIN_MSG ":: Stop looting when \ag%d\ax slots are left", iSaveBagSlots);
		WriteChatf(PLUGIN_MSG ":: Spam looting actions %s", iSpamLootInfo ? "\agON\ax" : "\arOFF\ax");
		WriteChatf(PLUGIN_MSG ":: The master looter will wait \ag%d\ax seconds before trying to distribute loot", iDistributeLootDelay);
		WriteChatf(PLUGIN_MSG ":: You will wait \ag%d\ax seconds before trying to autoinventory items on your cursor", iCursorDelay);
		WriteChatf(PLUGIN_MSG ":: The minimum price for all items to be bartered is: \ag%d\ax", iBarMinSellPrice);
		WriteChatf(PLUGIN_MSG ":: Logging loot actions for master looter is %s", iLogLoot ? "\agON\ax" : "\arOFF\ax");
		WriteChatf(PLUGIN_MSG ":: Raid looting is turned %s", iRaidLoot ? "\agON\ax" : "\arOFF\ax");
		WriteChatf(PLUGIN_MSG ":: Your default number to keep of new quest items is: \ag%d\ax", iQuestKeep);
		WriteChatf(PLUGIN_MSG ":: Your default for new no drop items is: \ag%s\ax", szNoDropDefault);
		WriteChatf(PLUGIN_MSG ":: Will exclude \ar%s\ax when checking for free slots", szExcludedBag1);
		WriteChatf(PLUGIN_MSG ":: Will exclude \ar%s\ax when checking for free slots", szExcludedBag2);
		WriteChatf(PLUGIN_MSG ":: Your default permission for items put into your guild bank is: \ag%s\ax", szGuildItemPermission);
		WriteChatf(PLUGIN_MSG ":: The location for your loot ini is:\n \ag%s\ax", szLootINI);
	}
}

LONG SetBOOL(long Cur, PCHAR Val, PCHAR Sec, PCHAR Key, PCHAR INI)
{
	long result = 0;
	if (!_strnicmp(Val, "false", 5) || !_strnicmp(Val, "off", 3) || !_strnicmp(Val, "0", 1))
	{
		result = 0;
	}
	else if (!_strnicmp(Val, "true", 4) || !_strnicmp(Val, "on", 2) || !_strnicmp(Val, "1", 1))
	{
		result = 1;
	}
	else
	{
		result = (!Cur) & 1;
	}
	if (Sec[0] && Key[0])
	{
		WritePrivateProfileString(Sec, Key, result ? "1" : "0", INI);
	}
	return result;
}

void SetItemCommand(PSPAWNINFO pCHAR, PCHAR szLine)
{
	if (!InGameOK()) return;
	bool NeedHelp = false;
	char Parm1[MAX_STRING];
	char Parm2[MAX_STRING];
	GetArg(Parm1, szLine, 1);
	GetArg(Parm2, szLine, 2);

	if (!_stricmp(Parm1, "help"))
	{
		NeedHelp = true;
	}
	else if (!_stricmp(Parm1, "Keep") || !_stricmp(Parm1, "Sell") || !_stricmp(Parm1, "Deposit") || !_stricmp(Parm1, "Barter") || !_stricmp(Parm1, "Gear") || !_stricmp(Parm1, "Quest") || !_stricmp(Parm1, "Ignore") || !_stricmp(Parm1, "Destroy") || !_stricmp(Parm1, "Status"))
	{
		if (PCHARINFO2 pChar2 = GetCharInfo2())
		{
			if (pChar2 && pChar2->pInventoryArray && pChar2->pInventoryArray->Inventory.Cursor)
			{
				if (PCONTENTS pItem = pChar2->pInventoryArray->Inventory.Cursor)
				{
					CreateLootEntry(Parm1, Parm2, GetItemFromContents(pItem));
				}
				else
				{
					WriteChatf(PLUGIN_MSG ":: There is no way this should fail as far as I know. There is an item on your cursor, but you were unable to get PCONTENTS from it.");
				}
			}
			else
			{
				WriteChatf(PLUGIN_MSG ":: There is no item on your cursor, please pick up the item and resend the command");
			}
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: There is no way this should fail as far as I know.  The plugin failed to get GetCharInfo2");
		}
	}
	else
	{
		NeedHelp = TRUE;
	}
	if (NeedHelp) {
		WriteChatColor("Usage:");
		WriteChatColor("/SetItem Keep -> Will loot.  '/AutoLoot deposit' will put these items into your personal banker.");
		WriteChatColor("/SetItem Deposit -> Will loot.  '/AutoLoot deposit' will put these items into your guild banker.");
		WriteChatColor("/SetItem Sell -> Will loot.  '/AutoLoot sell' will sell these items to a merchant.");
		WriteChatColor("/SetItem Barter #n -> Will loot, and barter if any buyers offer more then #n plat.");
		WriteChatColor("/SetItem Quest #n -> Will loot up to #n on each character running this plugin.");
		WriteChatColor("/SetItem Gear -> Will loot if your the specified class up to the number listed in the ini entry.");
		WriteChatColor("                 Example entry: Gear|Classes|WAR|PAL|SHD|BRD|NumberToLoot|2|");
		WriteChatColor("/SetItem Ignore -> Will not loot this item.");
		WriteChatColor("/SetItem Destroy -> The master looter will attempt to loot and then destroy this item.");
		WriteChatColor("/SetItem Status -> Will tell you what that item is set to in your loot.ini.");
		WriteChatColor("/SetItem help");
	}
}

void CreateLootEntry(CHAR* szAction, CHAR* szEntry, PITEMINFO pItem)
{
	CHAR INISection[MAX_STRING] = { 0 };
	CHAR INIValue[MAX_STRING] = { 0 };
	sprintf_s(INISection, "%c", pItem->Name[0]);
	if (!_stricmp(szAction, "Keep"))
	{
		WritePrivateProfileString(INISection, pItem->Name, "Keep", szLootINI);
		WriteChatf(PLUGIN_MSG ":: Setting \ag%s\ax to \agKeep\ax", pItem->Name);
	}
	else if (!_stricmp(szAction, "Sell"))
	{
		WritePrivateProfileString(INISection, pItem->Name, "Sell", szLootINI);
		WriteChatf(PLUGIN_MSG ":: Setting \ag%s\ax to \agSell\ax", pItem->Name);
	}
	else if (!_stricmp(szAction, "Deposit"))
	{
		WritePrivateProfileString(INISection, pItem->Name, "Deposit", szLootINI);
		WriteChatf(PLUGIN_MSG ":: Setting \ag%s\ax to \agDeposit\ax", pItem->Name);
	}
	else if (!_stricmp(szAction, "Ignore"))
	{
		WritePrivateProfileString(INISection, pItem->Name, "Ignore", szLootINI);
		WriteChatf(PLUGIN_MSG ":: Setting \ag%s\ax to \arIgnore\ax", pItem->Name);
	}
	else if (!_stricmp(szAction, "Destroy"))
	{
		WritePrivateProfileString(INISection, pItem->Name, "Destroy", szLootINI);
		WriteChatf(PLUGIN_MSG ":: Setting \ag%s\ax to \arDestroy\ax", pItem->Name);
	}
	else if (!_stricmp(szAction, "Quest"))
	{
		int QuestNumber = atoi(szEntry);
		sprintf_s(INIValue, "Quest|%d", QuestNumber);
		WritePrivateProfileString(INISection, pItem->Name, INIValue, szLootINI);
		WriteChatf(PLUGIN_MSG ":: Setting \ag%s\ax to \ag%s\ax", pItem->Name, INIValue);
	}
	else if (!_stricmp(szAction, "Barter"))
	{
		int BarterNumber = atoi(szEntry);
		sprintf_s(INIValue, "Barter|%d", BarterNumber);
		WritePrivateProfileString(INISection, pItem->Name, INIValue, szLootINI);
		WriteChatf(PLUGIN_MSG ":: Setting \ag%s\ax to \ag%s\ax", pItem->Name, INIValue);
	}
	else if (!_stricmp(szAction, "Gear"))
	{
		sprintf_s(INIValue, "Gear|Classes|");
		for (int playerclass = Warrior; playerclass <= Berserker; playerclass++)
		{
			if (pItem->Classes & (1 << (playerclass - 1)))
			{
				strcat_s(INIValue, ClassInfo[playerclass].UCShortName);
				strcat_s(INIValue, "|");
			}
		}
		int matching = 0;
		if (pItem->Lore == 0)
		{
			for (int itemslot = 0; itemslot < BAG_SLOT_START; itemslot++)
			{
				if (pItem->EquipSlots & (1 << itemslot))
				{
					matching++;
				}
			}
		}
		else
		{
			matching = 1;
		}
		sprintf_s(INIValue, "%sNumberToLoot|%i|", INIValue, matching);
		WritePrivateProfileString(INISection, pItem->Name, INIValue, szLootINI);
		WriteChatf(PLUGIN_MSG ":: Setting \ag%s\ax to:", pItem->Name);
		WriteChatf(PLUGIN_MSG ":: \ag%s\ax", INIValue);
	}
	else if (!_stricmp(szAction, "Status"))
	{
		CHAR Value[MAX_STRING] = { 0 };
		if (GetPrivateProfileString(INISection, pItem->Name, 0, Value, MAX_STRING, szLootINI) == 0)
		{
			WriteChatf(PLUGIN_MSG ":: \ag%s\ax is not in your loot.ini", pItem->Name);
		}
		else
		{
			WriteChatf(PLUGIN_MSG ":: \ag%s\ax is set to \ag%s\ax", pItem->Name, Value);
		}
	}
	else
	{
		WriteChatf(PLUGIN_MSG ":: Invalid command.  The accepted commands are [Quest #n|Gear|Keep|Sell|Ignore|Destroy]");
	}

	if (PCHARINFO2 pChar2 = GetCharInfo2())
	{
		if (pChar2->pInventoryArray && pChar2->pInventoryArray->Inventory.Cursor)
		{
			if (PCONTENTS pItem = pChar2->pInventoryArray->Inventory.Cursor)
			{
				if (!_stricmp(szAction, "Destroy"))
				{
					if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: Destroying \ar%s\ax", pItem->Item2->Name); }
					DoCommand(GetCharInfo()->pSpawn, "/destroy");
				}
				else
				{
					if (FitInInventory(pItem->Item2->Size))
					{
						if (iSpamLootInfo) { WriteChatf(PLUGIN_MSG ":: Putting \ag%s\ax into my inventory", pItem->Item2->Name); }
						DoCommand(GetCharInfo()->pSpawn, "/autoinventory");
					}
				}
			}
		}
	}
}

/********************************************************************************
****
**     Functions necessary for TLO
****
********************************************************************************/

class MQ2AutoLootType* pAutoLootType = nullptr;
class MQ2AutoLootType : public MQ2Type
{
public:
	enum AutoLootMembers
	{
		Active = 1,
		SellActive = 2,
		BuyActive = 3,
		DepositActive = 4,
		BarterActive = 5,
		FreeInventory = 6,
	};

	MQ2AutoLootType() :MQ2Type("AutoLoot")
	{
		TypeMember(Active);
		TypeMember(SellActive);
		TypeMember(BuyActive);
		TypeMember(DepositActive);
		TypeMember(BarterActive);
		TypeMember(FreeInventory);
	}

	bool GetMember(MQ2VARPTR VarPtr, char* Member, char* Index, MQ2TYPEVAR &Dest)
	{
		PMQ2TYPEMEMBER pMember = MQ2AutoLootType::FindMember(Member);
		if (pMember)
		{
			switch ((AutoLootMembers)pMember->ID)
			{
			case Active:
				Dest.DWord = iUseAutoLoot;
				Dest.Type = pBoolType;
				return true;
			case SellActive:
				Dest.DWord = bSellActive;
				Dest.Type = pBoolType;
				return true;
			case BuyActive:
				Dest.DWord = bBuyActive;
				Dest.Type = pBoolType;
				return true;
			case DepositActive:
				Dest.DWord = bDepositActive;
				Dest.Type = pBoolType;
				return true;
			case BarterActive:
				Dest.DWord = bBarterActive;
				Dest.Type = pBoolType;
				return true;
			case FreeInventory:
				Dest.DWord = AutoLootFreeInventory();
				Dest.Type = pIntType;
				return true;
			}
			return false;
		}
		return false;
	}

	bool ToString(MQ2VARPTR VarPtr, PCHAR Destination)
	{
		strcpy_s(Destination, MAX_STRING, "TRUE");
		return true;
	}
	bool FromData(MQ2VARPTR &VarPtr, MQ2TYPEVAR &Source)
	{
		return false;
	}
	bool FromString(MQ2VARPTR &VarPtr, PCHAR Source)
	{
		return false;
	}
	~MQ2AutoLootType() {}
};

int dataAutoLoot(char* szName, MQ2TYPEVAR &Ret)
{
	Ret.DWord = 1;
	Ret.Type = pAutoLootType;
	return true;
}

int AutoLootFreeInventory(void)
{
	int freeslots = 0;
	if (PCHARINFO2 pChar2 = GetCharInfo2())
	{
		for (DWORD slot = BAG_SLOT_START; slot < NUM_INV_SLOTS; slot++)
		{
			if (!HasExpansion(EXPANSION_HoT) && slot > BAG_SLOT_START + 7)
			{
				break;
			}
			if (pChar2->pInventoryArray && pChar2->pInventoryArray->InventoryArray && pChar2->pInventoryArray->InventoryArray[slot])
			{
				if (PCONTENTS pItem = pChar2->pInventoryArray->InventoryArray[slot])
				{
					if (GetItemFromContents(pItem)->Type == ITEMTYPE_PACK && (_stricmp(GetItemFromContents(pItem)->Name, szExcludedBag1) || _stricmp(GetItemFromContents(pItem)->Name, szExcludedBag2)))
					{
						if (!pItem->Contents.ContainedItems.pItems)
						{
							freeslots += GetItemFromContents(pItem)->Slots;
						}
						else
						{
							for (DWORD pslot = 0; pslot < (GetItemFromContents(pItem)->Slots); pslot++)
							{
								if (!pItem->GetContent(pslot))
								{
									freeslots++;
								}
							}
						}
					}
				}
				else
				{
					freeslots++;
				}
			}
			else
			{
				freeslots++;
			}
		}
	}
	freeslots = freeslots - iSaveBagSlots;
	if (freeslots < 0) return 0;
	return freeslots;
}

/********************************************************************************
****
**     Functions necessary for interacting with MQ2
****
********************************************************************************/

PLUGIN_API VOID InitializePlugin()
{
	// commands
	AddCommand("/autoloot", AutoLootCommand);
	AddCommand("/setitem", SetItemCommand);

	// TLOs
	AddMQ2Data("AutoLoot", dataAutoLoot);

	pAutoLootType = new MQ2AutoLootType;
}

PLUGIN_API VOID ShutdownPlugin()
{
	forget_loot_patterns();
	// remove commands
	RemoveCommand("/autoloot");
	RemoveCommand("/setitem");

	// remove TLOs
	RemoveMQ2Data("AutoLoot");

	delete pAutoLootType;
}

PLUGIN_API VOID SetGameState(DWORD GameState)
{
	if (!InGameOK()) return;
	if (GameState == GAMESTATE_INGAME)
	{
		if (!Initialized)
		{
			SetAutoLootVariables();
			Initialized = true;
		}
	}
	else if (GameState != GAMESTATE_LOGGINGIN)
	{
		if (Initialized)
		{
			Initialized = 0;
		}
	}
}

PLUGIN_API DWORD OnIncomingChat(PCHAR Line, DWORD Color)
{
	if (!InGameOK()) return 0;

	CHAR PassingOutLootText[MAX_STRING] = { 0 };
	sprintf_s(PassingOutLootText, "It is either on their never list or they have selected No."); // Confirmed 02/15/2018
	if (strstr(Line, PassingOutLootText))
	{
		bDistributeItemFailed = true;
		return(0);
	}
	sprintf_s(PassingOutLootText, "already has"); // Confirmed 02/19/2018
	if (strstr(Line, PassingOutLootText))
	{
		sprintf_s(PassingOutLootText, "and it is lore.");  // Confirmed 04/27/2017
		if (strstr(Line, PassingOutLootText))
		{
			bDistributeItemFailed = true;
			return(0);
		}
	}
	sprintf_s(PassingOutLootText, "It has been removed from your Shared Loot List."); // Confirmed 02/15/2018
	if (strstr(Line, PassingOutLootText))
	{
		bDistributeItemSucceeded = true;
		return(0);
	}

	if (WinState((CXWnd*)pMerchantWnd))
	{
		if (pMerchantWnd->pFirstChildWnd)
		{
			if (pMerchantWnd->pFirstChildWnd->pNextSiblingWnd)
			{
				CHAR MerchantText[MAX_STRING] = { 0 };
				CHAR MerchantName[MAX_STRING] = { 0 };
				GetCXStr(pMerchantWnd->pFirstChildWnd->WindowText, MerchantName, MAX_STRING);
				sprintf_s(MerchantText, "%s says, 'Hi there, %s. Just browsing?  Have you seen the ", MerchantName, GetCharInfo()->Name); // Confirmed 04/15/2017
				if (strstr(Line, MerchantText)) // Merchant window is populated
				{
					LootThreadTimer = pluginclock::now();
					return(0);
				}
				sprintf_s(MerchantText, "%s says, 'Welcome to my shop, %s. You would probably find a ", MerchantName, GetCharInfo()->Name); // Confirmed 04/15/2017
				if (strstr(Line, MerchantText)) // Merchant window is populated
				{
					LootThreadTimer = pluginclock::now();
					return(0);
				}
				sprintf_s(MerchantText, "%s says, 'Hello there, %s. How about a nice ", MerchantName, GetCharInfo()->Name); // Confirmed 04/15/2017
				if (strstr(Line, MerchantText)) // Merchant window is populated
				{
					LootThreadTimer = pluginclock::now();
					return(0);
				}
				sprintf_s(MerchantText, "%s says, 'Greetings, %s. You look like you could use a ", MerchantName, GetCharInfo()->Name); // Confirmed 04/15/2017
				if (strstr(Line, MerchantText)) // Merchant window is populated
				{
					LootThreadTimer = pluginclock::now();
					return(0);
				}
			}
		}
	}

	CHAR BankerText[MAX_STRING] = { 0 };
	sprintf_s(BankerText, "tells you, 'Welcome to my bank!"); // Confirmed 04/21/2017
	if (strstr(Line, BankerText)) // Banker window is populated
	{
		LootThreadTimer = pluginclock::now() + std::chrono::milliseconds(2000); // TODO determine what is an appropriate delay
		return(0);
	}

	if (WinState((CXWnd*)FindMQ2Window("BarterSearchWnd")))
	{
		CHAR BarterText[MAX_STRING] = { 0 };
		sprintf_s(BarterText, "You've sold ");  // Confirmed 04/27/2017
		if (strstr(Line, BarterText))
		{
			sprintf_s(BarterText, " to ");  // Confirmed 04/27/2017
			if (PCHAR pItemStart = strstr(Line, BarterText))
			{
				sprintf_s(BarterText, ".");  // Confirmed 04/27/2017
				if (PCHAR pItemEnd = strstr(Line, BarterText))
				{
					bBarterItemSold = true;
					return(0);
				}
			}
		}
		sprintf_s(BarterText, "Your transaction failed because your barter data is out of date.");  // Taken from HoosierBilly, has not beem checked
		if (strstr(Line, BarterText)) //Need to research to refresh the barter search window
		{
			bBarterReset = true;
			return(0);
		}
	}

	return(0);
}

PLUGIN_API VOID OnPulse(VOID)
{
	if (!iUseAutoLoot || !InGameOK()) { return; }
	PCHARINFO pChar = GetCharInfo();
	if (!pChar->UseAdvancedLooting) { return; }
	PCHARINFO2 pChar2 = GetCharInfo2();
	bool ItemOnCursor = CheckCursor(); 	//check cursor for items, and will put in inventory if it fits after CursorDelay has been exceed
	if (pluginclock::now() < LootTimer) { return; }// Ok, LootTimer isn't  counted down yet
	if (DestroyStuff()) { return; } // When you loot an item marked Destroy it will set the DestroyID to that item's ID and proceed to pick that item from inventory and destroy before resetting DestroyID to 0
	if (CheckWindows(ItemOnCursor)) { return; } // Returns true if your attempting to accept trade requests or click the confirmation box for no drop items
	// Ok, we have done all our prechecks lets do looting things now
	if (pRaid && pRaid->RaidMemberCount > 0 && !iRaidLoot) return; // Your in a raid, turning mq2autoloot off
	if (!WinState((CXWnd*)pAdvancedLootWnd)) { return; }
	PEQADVLOOTWND pAdvLoot = (PEQADVLOOTWND)pAdvancedLootWnd;
	if (!pAdvLoot) { return; }
	if (SetLootSettings()) { return; } // Turn off Auto Loot All
	CListWnd *pPersonalList = (CListWnd *)pAdvancedLootWnd->GetChildItem("ADLW_PLLList");
	CListWnd *pSharedList = (CListWnd *)pAdvLoot->pCLootList->SharedLootList;
	if (LootInProgress(pAdvLoot, pPersonalList, pSharedList)) { return; }
	if (HandlePersonalLoot(ItemOnCursor, pChar, pAdvLoot, pPersonalList, pSharedList)) { return; }
	if (HandleSharedLoot(ItemOnCursor, pChar, pChar2, pAdvLoot, pPersonalList, pSharedList)) { return; }
}

