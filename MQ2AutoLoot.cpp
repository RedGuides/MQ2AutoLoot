// MQ2AutoLoot.cpp : Defines the entry point for the DLL application.
//Author: Plure
//
// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.
#include <mq/Plugin.h>

#if !defined(ROF2EMU) && !defined(UFEMU)
#include "../MQ2AutoLootSort/LootSort.h"
#include "MQ2AutoLoot.h"
#include "ItemActions.h"

PreSetup("MQ2AutoLoot");
PLUGIN_VERSION(1.12);

//MQ2EQBC shit
typedef bool (*fAreTheyConnected)(const char* szName);
fAreTheyConnected EQBC_AreTheyConnected;

// Variables that are setable through the /AutoLoot command
int                     iUseAutoLoot = 1;
int                     iSpamLootInfo = 1;
int                     iCursorDelay = 10;
int                     iNewItemDelay = 0;
int                     iDistributeLootDelay = 5;
int                     iSaveBagSlots = 0;
int                     iQuestKeep = 0;
int                     iBarMinSellPrice = 1;
int                     iLogLoot = 1;
int                     iRaidLoot = 0;
char                    szNoDropDefault[MAX_STRING];
char                    szLootINI[MAX_STRING];
char                    szExcludeBag1[MAX_STRING];
char                    szExcludeBag2[MAX_STRING];
char                    szGuildItemPermission[MAX_STRING];
// Variables that are used within the plugin, but not setable
bool                    Initialized = false;
bool                    CheckedAutoLootAll = false;  // used to check if Auto Loot All is checks
bool                    StartCursorTimer = true;  //
bool                    bDistributeItemSucceeded = false; // Set true when you successfully distribute an item to a raid/group member
bool                    bDistributeItemFailed = false; // Set true when you fail distribute an item to a raid/group member
int64_t                 DestroyID;
DWORD                   CursorItemID;
std::map<std::string, std::string> LootEntries; // Will be used by non-ML to make sure new loot entries are eventually added, when iNewItemDelay > 0 only the ML can add entries to loot.ini file... this can cause an issue when the ML/non-ML use different loot files
char                    szTemp[MAX_STRING];
char                    szCommand[MAX_STRING];
char                    szLogPath[MAX_PATH];
char                    szLogFileName[MAX_PATH];
HANDLE                  hPassOutLootThread = 0;
HANDLE                  hBarterItemsThread = 0;
HANDLE                  hBuyItemThread = 0;
HANDLE                  hSellItemsThread = 0;
HANDLE                  hDepositPersonalBankerThread = 0;
HANDLE                  hDepositGuildBankerThread = 0;

pluginclock::time_point LootTimer = pluginclock::now();
pluginclock::time_point CursorTimer = pluginclock::now();
pluginclock::time_point DestroyStuffCancelTimer = pluginclock::now();

/********************************************************************************
****
**     Functions necessary for OnPulse
****
********************************************************************************/

bool InGameOK() // Returns TRUE if character is in game and has valid character data structures
{
	return(GetGameState() == GAMESTATE_INGAME && GetCharInfo() && GetCharInfo()->pSpawn && GetCharInfo()->pSpawn->StandState != STANDSTATE_DEAD && GetPcProfile());
}

void SetAutoLootVariables()
{
	sprintf_s(szLogPath, "%s\\", gPathLogs);
	sprintf_s(szLogFileName, "%sLoot_Log.log", szLogPath);
	iUseAutoLoot = GetPrivateProfileInt(GetCharInfo()->Name, "UseAutoLoot", 1, INIFileName);
	if (GetPrivateProfileString(GetCharInfo()->Name, "lootini", nullptr, szLootINI, MAX_STRING, INIFileName) == 0)
	{
		sprintf_s(szLootINI, "%s\\Loot.ini", gPathConfig);
		WritePrivateProfileString(GetCharInfo()->Name, "lootini", szLootINI, INIFileName);
	}
	float fVersion;
	CHAR szVersion[MAX_STRING] = { 0 };
	if (GetPrivateProfileString("Settings", "Version", 0, szVersion, MAX_STRING, szLootINI) == 0)
	{
		CreateLootINI();
	}
	if (IsNumber(szVersion))
	{
		fVersion = std::stof(szVersion);
	}
	else
	{
		fVersion = 0.0;
	}
	if (!fVersion || fVersion != MQ2Version) // There is a version mismatch, lets update the loot.ini
	{
		sprintf_s(szVersion, "%1.2f", MQ2Version);
		WritePrivateProfileString("Settings", "Version", szVersion, szLootINI);
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
	iNewItemDelay = GetPrivateProfileInt("Settings", "NewItemDelay", -1, szLootINI);
	if (iNewItemDelay == -1)
	{
		iNewItemDelay = 0;
		WritePrivateProfileString("Settings", "NewItemDelay", "0", szLootINI);
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
	if (GetPrivateProfileString("Settings", "ExcludeBag1", 0, szExcludeBag1, MAX_STRING, szLootINI) == 0)
	{
		sprintf_s(szExcludeBag1, "Extraplanar Trade Satchel");
		WritePrivateProfileString("Settings", "ExcludeBag1", szExcludeBag1, szLootINI);
	}
	if (GetPrivateProfileString("Settings", "ExcludeBag2", 0, szExcludeBag2, MAX_STRING, szLootINI) == 0)
	{
		sprintf_s(szExcludeBag2, "Extraplanar Trade Satchel");
		WritePrivateProfileString("Settings", "ExcludeBag2", szExcludeBag2, szLootINI);
	}
	if (GetPrivateProfileString("Settings", "GuildItemPermission", 0, szGuildItemPermission, MAX_STRING, szLootINI) == 0)
	{
		sprintf_s(szGuildItemPermission, "View Only");
		WritePrivateProfileString("Settings", "GuildItemPermission", szGuildItemPermission, szLootINI);
	}

	if (Initialized) // Won't spam this on start up of plugin, will only spam if someone reloads their settings
	{
		ShowInfo();
	}
}

void CreateLootINI()
{
	const auto sections = { "Settings","Global","A","B","C","D","E",
		"F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z" };
	for (const auto section : sections)
	{
		WritePrivateProfileString(section, "|", "=====================================================================|", szLootINI);
	}
}

void ShowInfo() // Outputs the variable settings into your MQ2 Window
{
	WriteChatf("%s:: AutoLoot is %s", PLUGIN_CHAT_MSG, iUseAutoLoot ? "\agON\ax" : "\arOFF\ax");
	WriteChatf("%s:: Stop looting when \ag%d\ax slots are left", PLUGIN_CHAT_MSG, iSaveBagSlots);
	WriteChatf("%s:: Spam looting actions %s", PLUGIN_CHAT_MSG, iSpamLootInfo ? "\agON\ax" : "\arOFF\ax");
	WriteChatf("%s:: The master looter will wait \ag%d\ax seconds before trying to distribute loot", PLUGIN_CHAT_MSG, iDistributeLootDelay);
	WriteChatf("%s:: The master looter will wait \ag%d\ax seconds when a new item drops before looting that item", PLUGIN_CHAT_MSG, iNewItemDelay);
	WriteChatf("%s:: You will wait \ag%d\ax seconds before trying to autoinventory items on your cursor", PLUGIN_CHAT_MSG, iCursorDelay);
	WriteChatf("%s:: The minimum price for all items to be bartered is: \ag%d\ax", PLUGIN_CHAT_MSG, iBarMinSellPrice);
	WriteChatf("%s:: Logging loot actions for master looter is %s", PLUGIN_CHAT_MSG, iLogLoot ? "\agON\ax" : "\arOFF\ax");
	WriteChatf("%s:: Raid looting is turned: %s", PLUGIN_CHAT_MSG, iRaidLoot ? "\agON\ax" : "\arOFF\ax");
	WriteChatf("%s:: Your default number to keep of new quest items is: \ag%d\ax", PLUGIN_CHAT_MSG, iQuestKeep);
	WriteChatf("%s:: Your default for new no drop items is: \ag%s\ax", PLUGIN_CHAT_MSG, szNoDropDefault);
	WriteChatf("%s:: Will exclude \ar%s\ax when checking for free slots", PLUGIN_CHAT_MSG, szExcludeBag1);
	WriteChatf("%s:: Will exclude \ar%s\ax when checking for free slots", PLUGIN_CHAT_MSG, szExcludeBag2);
	WriteChatf("%s:: Your default permission for items put into your guild bank is: \ag%s\ax", PLUGIN_CHAT_MSG, szGuildItemPermission);
	WriteChatf("%s:: The location for your loot ini is:\n \ag%s\ax", PLUGIN_CHAT_MSG, szLootINI);
}

bool DoThreadAction() // Do actions from threads that need to be in the pulse to stop crashing to desktop
{
	if (pItemToPickUp)
	{
		if (PickupItem(eItemContainerPossessions, pItemToPickUp))
		{
			// Ok we succeeded in select the item to sell
			DebugSpew("%s:: We suceeded in picking up \ar%s\ax", PLUGIN_DEBUG_MSG, pItemToPickUp->GetName());
		}
		else
		{
			// Ok we failed to select the item to sell
			WriteChatf("%s:: We failed to pick up \ar%s\ax", PLUGIN_CHAT_MSG, pItemToPickUp->GetName());
			DebugSpew("%s:: We failed to pick up \ar%s\ax", PLUGIN_DEBUG_MSG, pItemToPickUp->GetName());
		}
		pItemToPickUp = nullptr;
		return true;
	}
	if (pWndLeftMouseUp)
	{
		if (SendWndClick2(pWndLeftMouseUp, "leftmouseup"))
		{
			// Ok we succeeded in select the item to sell
			DebugSpew("%s:: We suceeded in left clicking a button", PLUGIN_DEBUG_MSG);
		}
		else
		{
			// Ok we failed to select the item to sell
			WriteChatf("%s:: We failed to left clicking a button", PLUGIN_CHAT_MSG);
			DebugSpew("%s:: We failed to left clicking a button", PLUGIN_DEBUG_MSG);
		}
		pWndLeftMouseUp = nullptr;
		return true;
	}
	return false;
}

bool CheckCursor()  // Returns true if an item is on your cursor
{
	auto pChar2 = GetPcProfile();
	if (ItemClient* pItem = pChar2->GetInventorySlot(InvSlot_Cursor))
	{
		if (!WinState(pTradeWnd) && !WinState(pGiveWnd) && !WinState(pMerchantWnd) && !WinState(pBankWnd) && !WinState(FindMQ2Window("GuildBankWnd")) && !WinState(FindMQ2Window("TradeskillWnd")))
		{
			if (StartCursorTimer)  // Going to set CursorItemID and CursorTimer
			{
				StartCursorTimer = false;
				CursorItemID = pItem->GetID();
				CursorTimer = pluginclock::now() + std::chrono::seconds(iCursorDelay); // Wait CursorDelay in seconds before attempting to autoinventory the item on your cursor
			}
			else if (CursorItemID != pItem->GetID()) // You changed items on your cursor, time to reset CursorItemID and CursorTimer
			{
				CursorItemID = pItem->GetID();
				CursorTimer = pluginclock::now() + std::chrono::seconds(iCursorDelay); // Wait CursorDelay in seconds before attempting to autoinventory the item on your cursor
			}
			else if (pluginclock::now() > CursorTimer)  // Waited CursorDelay, now going to see if you have room to autoinventory the item on your cursor
			{
				if (FitInInventory(pItem->GetItemDefinition()->Size))
				{
					if (iSpamLootInfo)
					{
						WriteChatf("%s:: Putting \ag%s\ax into my inventory", PLUGIN_CHAT_MSG, pItem->GetName());
					}
					DoCommand(GetCharInfo()->pSpawn, "/autoinventory");
				}
				else
				{
					if (iSpamLootInfo)
					{
						WriteChatf("%s:: \ag%s\ax doesn't fit into my inventory", PLUGIN_CHAT_MSG, pItem->GetName());
					}
				}
				StartCursorTimer = true;
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

bool DestroyStuff()
{
	if (DestroyID == 0)
	{
		return false;
	}
	PCHARINFO pChar = GetCharInfo();
	auto pChar2 = GetPcProfile();
	if (pluginclock::now() > DestroyStuffCancelTimer)
	{
		if (iSpamLootInfo)
		{
			WriteChatf("%s:: Destroying timer ran out, moving on!", PLUGIN_CHAT_MSG);
		}
		DestroyID = 0;
		return true;
	}
	// Looping through the items in my inventory and seening if I want to sell/deposit them based on which window was open
	if (ItemPtr pItem = pChar2->GetInventorySlot(InvSlot_Cursor))
	{
		if (pItem->GetID() == DestroyID)
		{
			if (iSpamLootInfo)
			{
				WriteChatf("%s:: Destroying \ar%s\ax!", PLUGIN_CHAT_MSG, pItem->GetName());
			}
			DoCommand(GetCharInfo()->pSpawn, "/destroy");
			LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
			DestroyID = 0;
			return true;
		}
	}

	//check my inventory slots
	for (int nSlot = InvSlot_FirstBagSlot; nSlot < GetHighestAvailableBagSlot(); nSlot++)
	{
		if (CONTENTS* pItem = pChar2->GetInventorySlot(nSlot))
		{
			if (pItem->GetID() == DestroyID)
			{
				PickupItem(eItemContainerPossessions, pItem);
				LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
				return true;
			}
		}
	}

	//Checking my bags
	for (int nPack = InvSlot_FirstBagSlot; nPack < GetHighestAvailableBagSlot(); nPack++)
	{
		if (CONTENTS* pPack = pChar2->GetInventorySlot(nPack))
		{
			if (pPack->IsContainer())
			{
				for (const ItemPtr& pItem : pPack->GetHeldItems())
				{
					if (pItem)
					{
						if (pItem->GetID() == DestroyID)
						{
							PickupItem(eItemContainerPossessions, pItem.get());
							LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
							return true;
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
	// Whenever pConfirmationDialog is fixed this change needs to be pushed the repository, currently this crashes when you try and use it
	//if (CSidlScreenWnd *pWnd = (CSidlScreenWnd *)pConfirmationDialog)
	{
		if (pWnd->IsVisible())
		{
			if (CStmlWnd *Child = (CStmlWnd*)pWnd->GetChildItem("CD_TextOutput"))
			{
				std::string_view ConfirmationText{ Child->STMLText };
				if (ConfirmationText.find("are you sure you wish to loot it?") != std::string_view::npos)
				{
					if (WinState(pLootWnd))
					{
						for (int nLootSlots = 0; nLootSlots < (int)pLootWnd->NumOfSlots; nLootSlots++)
						{
							if (CONTENTS* pItem = pLootWnd->GetLootItem(nLootSlots))
							{
								if (ConfirmationText.find(pItem->GetName()) != std::string_view::npos)
								{
									if (!CheckIfItemIsLoreByID(pItem->GetID()))
									{
										//Ok so I don't have the item and it is lore or it is not lore and I can accept it
										if (CXWnd *pWndButton = pWnd->GetChildItem("CD_Yes_Button"))
										{
											if (iSpamLootInfo)
											{
												WriteChatf("%s:: Accepting a no drop item", PLUGIN_CHAT_MSG);
											}
											SendWndClick2(pWndButton, "leftmouseup");
											LootTimer = pluginclock::now() + std::chrono::milliseconds(1000);
											return true;
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
							if (iSpamLootInfo)
							{
								WriteChatf("%s:: Accepting a no drop item", PLUGIN_CHAT_MSG);
							}
							SendWndClick2(pWndButton, "leftmouseup");
							LootTimer = pluginclock::now() + std::chrono::milliseconds(1000);
							return true;
						}
					}
				}
			}
		}
	}

	if (pTradeWnd)
	{
		if (pTradeWnd->bHisReadyTrade && !pTradeWnd->bMyReadyTrade && !ItemOnCursor)
		{
			if (CXWnd* pTRDW_HisName = pTradeWnd->GetChildItem("TRDW_HisName"))
			{
				auto hisName = pTRDW_HisName->GetWindowText();
				if (!hisName.empty())
				{
					if (HandleEQBC())
					{
						if (fAreTheyConnected(hisName.c_str()))
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

void ClearLootEntries() // Clears out the LootEntries from the non-ML toons
{
	while (!LootEntries.empty())
	{
		WriteChatf("%s:: The \ag%s\ax is not in the database, setting it to \ag%s\ax now that the advanced loot window is gone", PLUGIN_CHAT_MSG, LootEntries.begin()->first.c_str(), LootEntries.begin()->second.c_str());
		CHAR Value[MAX_STRING];
		CHAR ItemName[MAX_STRING];
		CHAR INISection[MAX_STRING];
		CHAR LootEntry[MAX_STRING];
		Value[0] = '\0';
		sprintf_s(ItemName, "%s", LootEntries.begin()->first.c_str());
		sprintf_s(INISection, "%c", LootEntries.begin()->first.at(0));
		sprintf_s(LootEntry, "%s", LootEntries.begin()->second.c_str());
		if (GetPrivateProfileString(INISection, ItemName, 0, Value, MAX_STRING, szLootINI) == 0)
		{
			WritePrivateProfileString(INISection, ItemName, LootEntry, szLootINI);
		}
		LootEntries.erase(LootEntries.begin());
	}
}

bool SetLootSettings() // Turn off Auto Loot All
{
	if (CheckedAutoLootAll)
	{
		return false;
	}
	if (!WinState(FindMQ2Window("LootSettingsWnd")))
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
		if (pWndButton->IsActive())
		{
			if (pWndButton->bChecked)
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

bool HandlePersonalLoot(bool ItemOnCursor, PCHARINFO pChar, PcProfile* pChar2, CAdvancedLootWnd* pAdvLoot, CListWnd *pPersonalList, CListWnd *pSharedList) // Handle items in your personal loot window
{
	if (pAdvLoot->pPLootList)
	{
		for (LONG k = 0; k < pPersonalList->ItemsArray.Count; k++)
		{
			LONG listindex = (LONG)pPersonalList->GetItemData(k);
			if (listindex != -1)
			{
				if (auto pPersonalItem = &pAdvLoot->pPLootList->Items[listindex])
				{
					bool IWant = false;  // Will be set true if you want and can accept the item
					bool CheckIfOthersWant = false;  // Will be set true if I am ML and I can't accept or don't need
					char szLootAction[MAX_STRING];
					if (ParseLootEntry(ItemOnCursor, pChar, pChar2, pPersonalItem, true, szLootAction, &IWant, &CheckIfOthersWant))
					{
						// Ok we had a valid loot entry, lets do shit
						if (LootInProgress(pAdvLoot, pPersonalList, pSharedList))
						{
							return true;
						}
						if (IWant)
						{
							if (!_stricmp(szLootAction, "Destroy"))
							{
								if (iSpamLootInfo)
								{
									WriteChatf("%s:: PList: looting \ar%s\ax to destoy it", PLUGIN_CHAT_MSG, pPersonalItem->Name);
								}
								if (iLogLoot)
								{
									sprintf_s(szTemp, "%s :: PList: looting %s to destroy it", pChar->Name, pPersonalItem->Name);
									CreateLogEntry(szTemp);
								}
								DestroyID = pPersonalItem->ItemID;
								DestroyStuffCancelTimer = pluginclock::now() + std::chrono::seconds(10);
							}
							else
							{
								if (iSpamLootInfo)
								{
									WriteChatf("%s:: PList: Setting \ag%s\ax to loot", PLUGIN_CHAT_MSG, pPersonalItem->Name);
								}
								if (iLogLoot)
								{
									sprintf_s(szTemp, "%s :: PList: looting %s", pChar->Name, pPersonalItem->Name);
									CreateLogEntry(szTemp);
								}
							}
							if (pPersonalItem->NoDrop) // Adding a 1 second delay to click accept on no drop items
							{
								LootTimer = pluginclock::now() + std::chrono::milliseconds(1000);
							}
							else // Adding a small delay for regular items of 0.2 seconds
							{
								LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
							}
							if (CXWnd *pwnd = GetAdvLootPersonalListItem(listindex, 2))
							{
								SendWndClick2(pwnd, "leftmouseup"); // Loot the item
							}
							return true;
						}
						else
						{
							if (iSpamLootInfo)
							{
								WriteChatf("%s:: PList: Setting \ag%s\ax to leave", PLUGIN_CHAT_MSG, pPersonalItem->Name);
							}
							if (iLogLoot)
							{
								sprintf_s(szTemp, "%s :: PList: leaving %s", pChar->Name, pPersonalItem->Name);
								CreateLogEntry(szTemp);
							}
							LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
							if (CXWnd *pwnd = GetAdvLootPersonalListItem(listindex, 3))
							{
								SendWndClick2(pwnd, "leftmouseup"); // Leave the item
							}
							return true;
						}
					}
					else
					{
						// Ok, we did not have a valid entry lets bug out
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool HandleSharedLoot(bool ItemOnCursor, PCHARINFO pChar, PcProfile* pChar2, CAdvancedLootWnd* pAdvLoot, CListWnd *pPersonalList, CListWnd *pSharedList) // Handle items in your shared loot window
{
	if (pSharedList)
	{
		bool bMasterLooter = false;
		if (!AmITheMasterLooter(pChar, &bMasterLooter)) // if it returns false, the plugin should stop doing loot stuff this pulse
		{
			return true;
		}
		//Loop over the item array to find see if I need to set something
		for (LONG k = 0; k < pSharedList->ItemsArray.Count; k++)
		{
			LONG listindex = (LONG)pSharedList->GetItemData(k);
			if (listindex != -1)
			{
				if (auto pShareItem = &pAdvLoot->pCLootList->Items[listindex])
				{
					if (pAdvancedLootWnd && pShareItem && !pShareItem->LootDetails.IsEmpty())
					{
						if (!pShareItem->AutoRoll && !pShareItem->No && !pShareItem->Need && !pShareItem->Greed)
						{ // Ok lets check to see if we have any of the boxes check
							if (pShareItem->bAutoRoll && pShareItem->AskTimer > 0 || !pShareItem->bAutoRoll)
							{ // Ok if the master looter has set the item to AutoRoll and the AskTimer is greater then 0 I can proceed, otherwise it cause some annoying spamming of the everquest... Thanks to Chatwiththisname
								bool IWant = false;  // Will be set true if you want and can accept the item
								bool CheckIfOthersWant = false;  // Will be set true if I am ML and I can't accept or don't need
								char szLootAction[MAX_STRING];
								if (ParseLootEntry(ItemOnCursor, pChar, pChar2, pShareItem, bMasterLooter, szLootAction, &IWant, &CheckIfOthersWant))
								{
									// Ok we had a valid loot entry, lets do shit
									if (LootInProgress(pAdvLoot, pPersonalList, pSharedList))
									{
										return true;
									}
									if (bMasterLooter)
									{
										DebugSpew("bMasterLooter = %d", bMasterLooter);
										if (bDistributeItemFailed)
										{
											DebugSpew("bDistributeItemFailed = %d", bDistributeItemFailed);
											WriteChatf("%s:: SList: Attempting to pass out \ag%s\ax since apparently it is lore and I have it in my parcels... please stop storing shit in parcels like Tone", PLUGIN_CHAT_MSG, pShareItem->Name);
											CheckIfOthersWant = true;
											bDistributeItemFailed = false;
										}
										if (CheckIfOthersWant)
										{
											DebugSpew("CheckIfOthersWant = %d", CheckIfOthersWant);
											DWORD nThreadID = 0;
											hPassOutLootThread = CreateThread(NULL, NULL, PassOutLoot, (PVOID)k, 0, &nThreadID);
											LootTimer = pluginclock::now() + std::chrono::seconds(iDistributeLootDelay) + std::chrono::seconds(30); // Lets lock out the plugin from doing loot actions while we attempt to pass out items
											return true;
										}
										if (IWant)
										{
											//I want and I am the master looter
											DebugSpew("IWant = %d", IWant);
											if (iSpamLootInfo)
											{
												WriteChatf("%s:: SList: Giving \ag%s\ax to me", PLUGIN_CHAT_MSG, pShareItem->Name);
											}
											if (iLogLoot)
											{
												sprintf_s(szTemp, "%s :: SList: looting %s", pChar->Name, pShareItem->Name);
												CreateLogEntry(szTemp);
											}
											bDistributeItemFailed = false; // This is needed to check if I have the item in parcels and it is lore due to that information being kept server side
											DebugSpew("Using pAdvancedLootWnd->DoSharedAdvLootAction(....) to loot the item on the corpse");
											pAdvancedLootWnd->DoSharedAdvLootAction(*pShareItem, pChar->Name, 0, pShareItem->LootDetails[0].StackCount);
										}
										else
										{
											//I don't want and am the master looter
											DebugSpew("IWant = %d", IWant);
											if (iSpamLootInfo)
											{
												WriteChatf("%s:: SList: Setting \ag%s\ax to leave", PLUGIN_CHAT_MSG, pShareItem->Name);
											}
											if (iLogLoot)
											{
												sprintf_s(szTemp, "%s :: SList: leaving %s", pChar->Name, pShareItem->Name);
												CreateLogEntry(szTemp);
											}
											DebugSpew("Using pAdvancedLootWnd->DoSharedAdvLootAction(....) to leave the item on the corpse");
											pAdvancedLootWnd->DoSharedAdvLootAction(*pShareItem, pChar->Name, 1, pShareItem->LootDetails[0].StackCount);
										}
										LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
										return true;
									}
									else
									{
										DebugSpew("bMasterLooter = %d", bMasterLooter);
										if (IWant)
										{
											//I want and i am not the master looter
											DebugSpew("IWant = %d", IWant);
											if (iSpamLootInfo)
											{
												WriteChatf("%s:: SList: Setting \ag%s\ax to greed", PLUGIN_CHAT_MSG, pShareItem->Name);
											}
											if (CXWnd *pwnd = GetAdvLootSharedListItem(listindex, 10)) // 9 = need, 10 = greed
											{
												DebugSpew("Found the CXWnd for setting the item to greed");
												SendWndClick2(pwnd, "leftmouseup"); // Setting to greed
											}
										}
										else
										{
											//I don't want and i am not the master looter
											DebugSpew("IWant = %d", IWant);
											if (iSpamLootInfo)
											{
												WriteChatf("%s:: SList: Setting \ag%s\ax to no", PLUGIN_CHAT_MSG, pShareItem->Name);
											}
											if (CXWnd *pwnd = GetAdvLootSharedListItem(listindex, 11)) // Setting to no
											{
												DebugSpew("Found the CXWnd for setting the item to no");
												SendWndClick2(pwnd, "leftmouseup");
											}
										}
										LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
										return true;
									}
								}
								else
								{
									// Ok, we did not have a valid entry lets bug out
									return true;
								}
							}
						}
					}
					else
					{
						DebugSpew("%s:: SList: Attempted to leave on an item but couldn't due to bad pShareItem->LootDetails", PLUGIN_DEBUG_MSG);
					}
				}
				else
				{
					DebugSpew("%s:: SList: Unable to access AdvancedLootItem* for item number = %d", PLUGIN_DEBUG_MSG, k);
				}
			}
			else
			{
				DebugSpew("%s:: SList: listindex = %d, for item number = %d", PLUGIN_DEBUG_MSG, listindex, k);
			}
			// Ok we need to stop non-master looter from getting items on their cursors by clicking a bunch of items that they want when there are only a few free bag slots available and then the ML passing them all to them putting items on their cursor with no place to put them
			if (!ContinueCheckingSharedLoot()) // TODO make this smarter by determining if items stack etc
			{
				// Ok this is an attempt to keep the non-master
				return true;
			}
		}
	}
	return false;
}

bool WinState(CXWnd *Wnd) // Returns TRUE if the specified UI window is visible
{
	return Wnd && Wnd->IsVisible();
}

bool HandleEQBC()
{
	auto fisConnected = (unsigned short(*)())GetPluginProc("mq2eqbc", "isConnected");
	EQBC_AreTheyConnected = (bool(*)(const char* szName))GetPluginProc("mq2eqbc", "AreTheyConnected");
	if (fisConnected && EQBC_AreTheyConnected && fisConnected())
	{
		return true;
	}
	return false;
}

bool AmITheMasterLooter(PCHARINFO pChar, bool* pbMasterLooter) // Determines if the character is the master looter, if it has to set a master looter it instructs the plugin to pause looting
{
	//If I don't have a masterlooter set and I am leader I will set myself master looter
	if (pRaid && pRaid->RaidMemberCount > 0) // Ok we're in a raid, lets see who should handle loot
	{
		int ml = 0;
		for (ml = 0; ml < MAX_RAID_SIZE; ml++)
		{
			if (pRaid->RaidMemberUsed[ml])
			{
				if (pRaid->RaidMember[ml].MasterLooter) // we found the ML
				{
					if (!_stricmp(pChar->Name, pRaid->RaidMember[ml].Name)) // Ok so I am the ML
					{
						*pbMasterLooter = true;
					}
					return true;
				}
			}
		}
		if (ml == MAX_RAID_SIZE)
		{
			if (!_stricmp(pChar->Name, pRaid->RaidLeaderName)) // Ok so we don't have a ML set, lets use the raid leader to handle loot
			{
				*pbMasterLooter = true;
			}
		}
		return true;
	}
	else // Ok so we're not in a raid, maybe we are in a group
	{
		if (pChar->pGroupInfo && pChar->pGroupInfo->GetGroupMember(0))
		{
			int ml = 0;
			for (ml = 0; ml < MAX_GROUP_SIZE; ml++) // Lets loop through the group and see if we can find the ML
			{
				if (auto pMember = pChar->pGroupInfo->GetGroupMember(ml))
				{
					if (pMember->MasterLooter) // Ok we found the ML
					{
						if (!pMember->Name.empty()) // and we know their name
						{
							if (ci_equals(pMember->Name, pChar->Name)) // Ok so I am the ML
							{
								*pbMasterLooter = true;
							}
							return true;
						}
					}
				}
			}
			if (ml == MAX_GROUP_SIZE) // Ok we didn't find the ML
			{
				if (pChar->pGroupInfo->pLeader)
				{
					if (pChar->pGroupInfo->pLeader->pSpawn)
					{
						if (pChar->pGroupInfo->pLeader->pSpawn->SpawnID)
						{
							if (pChar->pGroupInfo->pLeader->pSpawn->SpawnID == pChar->pSpawn->SpawnID)  // I am the leader, so lets make me the ML
							{
								WriteChatf("%s:: I am setting myself to master looter", PLUGIN_CHAT_MSG);
								sprintf_s(szCommand, "/grouproles set %s 5", pChar->Name);
								DoCommand(GetCharInfo()->pSpawn, szCommand);
								LootTimer = pluginclock::now() + std::chrono::seconds(5);  //Two seconds was too short, it attempts to set masterlooter a second time.  Setting to 5 seconds that should fix this
								return false;
							}
						}
					}
				}
			}
			return true;
		}
		else
		{
			return false; // There is no way this should happen, but incase it does I want the plugin to ignore shit
		}
	}
	return false; // no fucking way this would ever happen, since this only gets called when doing shared loot stuff
}

bool ParseLootEntry(bool ItemOnCursor, PCHARINFO pChar, PcProfile* pChar2, AdvancedLootItem* pLootItem, bool bMasterLooter, char* pszItemAction, bool* pbIWant, bool* pbCheckIfOthersWant)
{
	CHAR INISection[]{ pLootItem->Name[0],'\0' };
	CHAR Value[MAX_STRING];
	Value[0] = '\0';
	if (GetPrivateProfileString(INISection, pLootItem->Name, 0, Value, MAX_STRING, szLootINI) == 0)
	{
		// Ok, so this item isn't in the loot.ini file.
		if (iNewItemDelay > 0)
		{
			if (bMasterLooter) // Ok so we are only going to let the ML write to the file for new entries, this way they know to delay
			{
				InitialLootEntry(pLootItem, true);
				LootTimer = pluginclock::now() + std::chrono::seconds(iNewItemDelay); // Lets lock out the plugin from doing loot actions till newitemdelay is up
				if (iSpamLootInfo)
				{
					WriteChatf("%s:: The \ag%s\ax is not in the database, you have \ar%d\ax seconds to deal with it manually", PLUGIN_CHAT_MSG, pLootItem->Name, iNewItemDelay);
				}
			}
			else
			{ // Ok lets populate the loot entry after the advanced loot window is not open
				InitialLootEntry(pLootItem, false);
				// Ok so lets handle this item like we would if we a loot entry
				if (!ItemOnCursor && !pLootItem->LootDetails[0].Locked && DoIHaveSpace(pLootItem->Name, pLootItem->MaxStack, pLootItem->LootDetails[0].StackCount, true) && !CheckIfItemIsLoreByID(pLootItem->ItemID))
				{
					if (pLootItem->NoDrop)
					{
						CHAR *pParsedToken = NULL;
						CHAR *pParsedValue = strtok_s(szNoDropDefault, "|", &pParsedToken);
						if (!_stricmp(pParsedValue, "Ignore"))
						{
							return true;
						}
					}
					*pbIWant = true;
				}
				return true;
			}
		}
		else
		{
			InitialLootEntry(pLootItem, true);
		}
		return false;
	}
	else
	{
		// Ok, this item has a loot entry
		char *pParsedToken = NULL;
		char *pParsedValue = strtok_s(Value, "|", &pParsedToken);
		strcpy_s(pszItemAction, sizeof(char[MAX_STRING]), pParsedValue);
		if (!_stricmp(pParsedValue, "Keep") || !_stricmp(pParsedValue, "Deposit") || !_stricmp(pParsedValue, "Sell") || !_stricmp(pParsedValue, "Barter"))
		{
			if ((ItemOnCursor || pLootItem->LootDetails[0].Locked || !DoIHaveSpace(pLootItem->Name, pLootItem->MaxStack, pLootItem->LootDetails[0].StackCount, true) || CheckIfItemIsLoreByID(pLootItem->ItemID)))
			{
				if (bMasterLooter)
				{
					*pbCheckIfOthersWant = true;
				}
			}
			else
			{
				*pbIWant = true;
			}
			return true;
		}
		else if (!_stricmp(Value, "Destroy"))
		{
			if ((ItemOnCursor || pLootItem->LootDetails[0].Locked || !DoIHaveSpace(pLootItem->Name, pLootItem->MaxStack, pLootItem->LootDetails[0].StackCount, false) || CheckIfItemIsLoreByID(pLootItem->ItemID)))
			{
			}
			else
			{
				if (bMasterLooter)
				{
					*pbIWant = true;
				}
			}
			return true;
		}
		else if (!_stricmp(pParsedValue, "Ignore"))
		{
			return true;
		}
		else if (!_stricmp(pParsedValue, "Quest"))
		{
			int QuestNumber = 1;
			pParsedValue = strtok_s(NULL, "|", &pParsedToken);
			if (pParsedValue == NULL)
			{
				if (iSpamLootInfo)
				{
					WriteChatf("%s:: You did not set the quest number for \ag%s\ax, changing it to Quest|1", PLUGIN_CHAT_MSG, pLootItem->Name);
				}
				WritePrivateProfileString(INISection, pLootItem->Name, "Quest|1", szLootINI);
			}
			else
			{
				if (IsNumber(pParsedValue))
				{
					QuestNumber = GetIntFromString(pParsedValue, QuestNumber);
				}
			}
			if ((ItemOnCursor || pLootItem->LootDetails[0].Locked || QuestNumber <= FindItemCount(pLootItem->Name) || !DoIHaveSpace(pLootItem->Name, pLootItem->MaxStack, pLootItem->LootDetails[0].StackCount, true) || CheckIfItemIsLoreByID(pLootItem->ItemID)))
			{
				if (bMasterLooter)
				{
					*pbCheckIfOthersWant = true;
				}
			}
			else
			{
				*pbIWant = true;
			}
			return true;
		}
		else if (!_stricmp(pParsedValue, "Gear"))
		{
			DebugSpew("We found an item (%s) with the gear tag!", pLootItem->Name);
			bool RightClass = false; // Will set true if your class is one of the entries after Gear|Classes|...
			int GearNumber = 0;  // The number of this item to loot
			pParsedValue = strtok_s(NULL, "|", &pParsedToken);
			if (pParsedValue == NULL)
			{
				DebugSpew("iSpamLootInfo = %d", iSpamLootInfo);
				if (CONTENTS* pItem = FindBankItemByID((DWORD)pLootItem->ItemID))
				{
					WriteChatf("%s:: Found:\ag%s\ax, in my bank!", PLUGIN_CHAT_MSG, pLootItem->Name);
					CreateLootEntry("Gear", "", GetItemFromContents(pItem));
					return false;
				}
				else if (CONTENTS* pItem = FindItemByID((DWORD)pLootItem->ItemID))
				{
					WriteChatf("%s:: Found:\ag%s\ax, in my packs!", PLUGIN_CHAT_MSG, pLootItem->Name);
					CreateLootEntry("Gear", "", GetItemFromContents(pItem));
					return false;
				}
				else
				{
					WriteChatf("%s:: \ag%s\ax hasn't ever had classes set, setting it to loot!", PLUGIN_CHAT_MSG, pLootItem->Name);
					*pbIWant = true;
					return true;
				}
			}
			while (pParsedValue != NULL)
			{
				DebugSpew("Comparing %s to my class %s and seening if they match", pParsedValue, ClassInfo[pChar2->Class].UCShortName);
				if (!_stricmp(pParsedValue, ClassInfo[pChar2->Class].UCShortName))
				{
					RightClass = true;
					DebugSpew("They do match setting, setting RightClass to %d", RightClass);
				}
				if (!_stricmp(pParsedValue, "NumberToLoot"))
				{
					DebugSpew("We have entered the NumberToLoot portion of the entry");
					pParsedValue = strtok_s(NULL, "|", &pParsedToken);
					DebugSpew("The number we want to loot = %s", pParsedValue);
					if (pParsedValue == NULL)
					{
						GearNumber = 1;
						if (iSpamLootInfo)
						{
							WriteChatf("%s:: You did not set the Gear Number for \ag%s\ax, Please change your loot ini entry", PLUGIN_CHAT_MSG, pLootItem->Name);
						}
					}
					else
					{
						if (IsNumber(pParsedValue))
						{
							GearNumber = GetIntFromString(pParsedValue, GearNumber);
							DebugSpew("Setting GearNumber to %d", GearNumber);
						}
						break;
					}
				}
				pParsedValue = strtok_s(NULL, "|", &pParsedToken);
			}
			DebugSpew("We are checking the 6 statements to see if we should loot");
			DebugSpew("!RightClass = !%d", RightClass);
			DebugSpew("GearNumber <= FindItemCount(pLootItem->Name) = %d <= %d", GearNumber, FindItemCount(pLootItem->Name));
			DebugSpew("ItemOnCursor = %d", ItemOnCursor);
			DebugSpew("pLootItem->LootDetails.m_array[0].Locked = %u", pLootItem->LootDetails[0].Locked);
			DebugSpew("!DoIHaveSpace(pLootItem->Name, pLootItem->MaxStack, pLootItem->LootDetails.m_array[0].StackCount, true) = !%d", DoIHaveSpace(pLootItem->Name, pLootItem->MaxStack, pLootItem->LootDetails[0].StackCount, true));
			if ((!RightClass || ItemOnCursor || pLootItem->LootDetails[0].Locked || GearNumber <= FindItemCount(pLootItem->Name) || !DoIHaveSpace(pLootItem->Name, pLootItem->MaxStack, pLootItem->LootDetails[0].StackCount, true) || CheckIfItemIsLoreByID(pLootItem->ItemID)))
			{
				DebugSpew("We passed all the previous checks");
				if (bMasterLooter)
				{
					*pbCheckIfOthersWant = true;
					DebugSpew("Setting pbCheckIfOthersWant to %d", pbCheckIfOthersWant);
				}
			}
			else
			{
				DebugSpew("We failed one of the previous checks");
				*pbIWant = true;
				DebugSpew("Setting pbIWant to %d", pbIWant);
			}
			return true;
		}
		else
		{
			// Oh shit this isn't a valid loot entry, lets remake it using the default
			InitialLootEntry(pLootItem, true);
		}
	}
	return false;
}

void InitialLootEntry(AdvancedLootItem* pLootItem, bool bCreateImmediately)
{
	if (pLootItem)
	{
		if (pLootItem->Name)
		{
			CHAR INISection[]{ pLootItem->Name[0],'\0' };
			CHAR LootEntry[MAX_STRING] = { 0 };
			if (pLootItem->NoDrop)
			{
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
			}
			else
			{
				sprintf_s(LootEntry, "Keep");
			}
			if (bCreateImmediately)
			{
				WritePrivateProfileString(INISection, pLootItem->Name, LootEntry, szLootINI);
				WriteChatf("%s:: The \ag%s\ax is not in the database, setting it to \ag%s\ax", PLUGIN_CHAT_MSG, pLootItem->Name, LootEntry);
			}
			else
			{
				if (LootEntries.empty())
				{
					WriteChatf("%s:: LootEntries is emtpy, saving \ag%s\ax to LootEntries", PLUGIN_CHAT_MSG, pLootItem->Name);
					LootEntries[pLootItem->Name] = LootEntry;
					WriteChatf("%s:: The \ag%s\ax is not in the database, setting it to \ag%s\ax once the advanced loot window is gone", PLUGIN_CHAT_MSG, LootEntries.begin()->first.c_str(), LootEntries.begin()->second.c_str());
				}
				else
				{
					auto it = LootEntries.find(pLootItem->Name);
					if (it == LootEntries.end())
					{
						WriteChatf("%s:: LootEntries doesn't have \ag%s\ax saving it to LootEntries", PLUGIN_CHAT_MSG, pLootItem->Name);
						LootEntries[pLootItem->Name] = LootEntry;
					}
				}
			}
		}
	}
	return;
}

bool DoIHaveSpace(CHAR* pszItemName, DWORD pdMaxStackSize, DWORD pdStackSize, bool bSaveBagSlots)
{
	bool FitInStack = false;
	LONG nPack = 0;
	LONG Count = 0;
	PCHARINFO pCharInfo = GetCharInfo();
	auto pChar2 = GetPcProfile();

	if (pChar2)
	{
		//check my inventory slots
		for (int nSlot = InvSlot_FirstBagSlot; nSlot < GetHighestAvailableBagSlot(); nSlot++)
		{
			if (ItemPtr pItem = pChar2->GetInventorySlot(nSlot))
			{
				if (!_stricmp(pszItemName, pItem->GetName()))
				{
					if (pItem->IsStackable())
					{
						if (pdStackSize + pItem->GetItemCount() <= pdMaxStackSize)
						{
							FitInStack = true;
						}
					}
				}
			}
			else
			{
				Count++;
			}
		}

		//Checking my bags
		for (int nPack = InvSlot_FirstBagSlot; nPack < GetHighestAvailableBagSlot(); nPack++)
		{
			if (CONTENTS* pPack = pChar2->GetInventorySlot(nPack))
			{
				if (pPack->IsContainer())
				{
					for (const ItemPtr& pItem : pPack->GetHeldItems())
					{
						if (pItem)
						{
							if (!_stricmp(pszItemName, pItem->GetName()))
							{
								if (pItem->IsStackable())
								{
									if (pdStackSize + pItem->GetItemCount() <= pdMaxStackSize)
									{
										FitInStack = true;
									}
								}
							}
						}
						else
						{
							if (_stricmp(pPack->GetName(), szExcludeBag1) && _stricmp(pPack->GetName(), szExcludeBag2))
							{
								Count++;
							}
						}
					}
				}
			}
		}
	}
	if (Count > iSaveBagSlots && bSaveBagSlots)
	{
		return true;
	}
	else if (Count > 0 && !bSaveBagSlots)
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
	auto pChar2 = GetPcProfile();
	if (pChar2)
	{
		//check my inventory slots
		for (int nSlot = InvSlot_FirstBagSlot; nSlot < GetHighestAvailableBagSlot(); nSlot++)
		{
			if (!pChar2->GetInventorySlot(nSlot))
				return true;
		}

		//Checking my bags
		for (int nPack = InvSlot_FirstBagSlot; nPack < GetHighestAvailableBagSlot(); nPack++)
		{
			if (CONTENTS* pPack = pChar2->GetInventorySlot(nPack))
			{
				if (pPack->IsContainer()
					&& _stricmp(pPack->GetName(), szExcludeBag1) != 0
					&& _stricmp(pPack->GetName(), szExcludeBag2) != 0)
				{
					for (const ItemPtr& pItem : pPack->GetHeldItems())
					{
						if (!pItem && pPack->GetItemDefinition()->SizeCapacity >= pdItemSize)
							return true;
					}
				}
			}
		}
	}
	return false;
}

int CheckIfItemIsLoreByID(int64_t ID)
{
	DWORD ItemID = (DWORD)ID;
	if (CONTENTS* pItem = FindItemByID(ItemID))
	{
		return GetItemFromContents(FindItemByID(ItemID))->Lore;
	}
	if (CONTENTS* pItem = FindBankItemByID(ItemID))
	{
		return GetItemFromContents(FindBankItemByID(ItemID))->Lore;
	}
	return 0;
}

// FIXME: Items - De-duplicate with core
static int CountContainerItems(ItemContainer& container, int fromSlot, int toSlot, const char* name)
{
	int count = 0;
	auto predicatedCountVisitor = [&](const ItemPtr& pItem, const ItemIndex& index)
	{
		if (ci_equals(pItem->GetName(), name))
			count += pItem->GetItemCount();
	};

	container.VisitItems(fromSlot, toSlot, -1, predicatedCountVisitor);
	return count;
}

int FindItemCount(const char* pszItemName)
{
	PcProfile* pProfile = GetPcProfile();
	if (!pProfile) return 0;

	// Count worn items
	int count = CountContainerItems(pProfile->GetItemPosessions(), InvSlot_FirstWornItem, InvSlot_LastWornItem, pszItemName);
	// count items in bag slots
	count += CountContainerItems(pProfile->GetItemPosessions(), InvSlot_FirstBagSlot, InvSlot_LastBagSlot, pszItemName);
	// count items in bank slots
	count += CountContainerItems(pCharData->BankItems, -1, -1, pszItemName);
	// count items in shared bank slots
	count += CountContainerItems(pCharData->SharedBankItems, -1, -1, pszItemName);

	return count;
}

DWORD __stdcall PassOutLoot(PVOID pData)
{
	if (!InGameOK())
	{
		LootTimer = pluginclock::now();
		hPassOutLootThread = 0;
		return 0;
	}
	if (!WinState(pAdvancedLootWnd))
	{
		LootTimer = pluginclock::now();
		hPassOutLootThread = 0;
		return 0;
	}
	CListWnd *pPersonalList = (CListWnd *)pAdvancedLootWnd->GetChildItem("ADLW_PLLList");
	CListWnd *pSharedList = pAdvancedLootWnd->pCLootList->SharedLootList;
	if (LootInProgress(pAdvancedLootWnd, pPersonalList, pSharedList))
	{
		LootTimer = pluginclock::now();
		hPassOutLootThread = 0;
		return 0;
	}
	LONG k = (LONG)pData;
	if (pSharedList->GetItemData(k) != -1)
	{
		auto pShareItem = &pAdvancedLootWnd->pCLootList->Items[(int)pSharedList->GetItemData(k)];
		if (iLogLoot)
		{
			if (PCHARINFO pChar = GetCharInfo())
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
		}
		if (iSpamLootInfo)
		{
			WriteChatf("%s:: SList: Setting the DistributeLootTimer to \ag%d\ax seconds", PLUGIN_CHAT_MSG, iDistributeLootDelay);
		}
		pluginclock::time_point DistributeLootTimer = pluginclock::now() + std::chrono::seconds(iDistributeLootDelay);
		while (pluginclock::now() < DistributeLootTimer) // While loop to wait for DistributeLootDelay to time out
		{
			Sleep(100);
		}
		if (!InGameOK() || !WinState(pAdvancedLootWnd))
		{
			LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
			hPassOutLootThread = 0;
			return 0;
		}
		if (pRaid && pRaid->RaidMemberCount > 0) // Ok we're in a raid, lets see who should handle loot
		{
			for (int nMember = 0; nMember < 72; nMember++)
			{
				if (pAdvancedLootWnd && pShareItem && pShareItem->LootDetails.GetSize() > 0)
				{
					if (PCHARINFO pChar = GetCharInfo())
					{
						if (pRaid->RaidMemberUsed[nMember]) // Ok this raid slot has a character in it
						{
							if (_stricmp(pChar->Name, pRaid->RaidMember[nMember].Name)) // The character isn't me
							{
								if (GetSpawnByName(pRaid->RaidMember[nMember].Name)) // The character is in the zone
								{
									if (DistributeLoot(pRaid->RaidMember[nMember].Name, pShareItem, k))
									{
										LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
										hPassOutLootThread = 0;
										return 0;
									}

								}
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
				if (pAdvancedLootWnd && pShareItem && pShareItem->LootDetails.GetSize() > 0)
				{
					if (PCHARINFO pChar = GetCharInfo())
					{
						if (pChar->pGroupInfo)
						{
							if (auto pMember = pChar->pGroupInfo->GetGroupMember(nMember))
							{
								if (!pMember->Type == EQP_NPC && !pMember->Offline && pMember->pSpawn) // They aren't offline
								{
									if (DistributeLoot(pMember->pSpawn->Name, pShareItem, k))
									{
										LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
										hPassOutLootThread = 0;
										return 0;
									}
								}
							}
						}
					}
				}
			}
		}
		if (PCHARINFO pChar = GetCharInfo())
		{
			if (pAdvancedLootWnd && pShareItem && pShareItem->LootDetails.GetSize() > 0)
			{
				if (iSpamLootInfo)
				{
					WriteChatf("%s:: SList: No one wanted \ag%s\ax setting to leave", PLUGIN_CHAT_MSG, pShareItem->Name);
				}
				if (iLogLoot)
				{
					sprintf_s(szTemp, "%s :: SList: Attempted to pass out %s and no one wanted, leaving it on the corpse", pChar->Name, pShareItem->Name);
					CreateLogEntry(szTemp);
				}
				pAdvancedLootWnd->DoSharedAdvLootAction(*pShareItem, pChar->Name, 1, pShareItem->LootDetails[0].StackCount); // Leaving the item on the corpse
				bDistributeItemFailed = false;
			}
			else
			{
				DebugSpew("%s:: SList: After no one wanted the item, the attempt to leave on an item failed due to bad pShareItem->LootDetails", PLUGIN_DEBUG_MSG);
			}
		}
		else
		{
			DebugSpew("%s:: SList: After no one wanted the item, the attempt to leave on an item failed due not being able to get PCHARINFO", PLUGIN_DEBUG_MSG);
		}
	}
	LootTimer = pluginclock::now() + std::chrono::milliseconds(200);
	hPassOutLootThread = 0;
	return 0;
}

bool DistributeLoot(CHAR* szName, AdvancedLootItem* pShareItem, LONG ItemIndex)
{
	bDistributeItemSucceeded = false;
	bDistributeItemFailed = false;
	if (!InGameOK())
	{
		return true;
	}
	PCHARINFO pChar = GetCharInfo();
	if (iSpamLootInfo)
	{
		WriteChatf("%s:: SList: Attempting to give \ag%s\ax to \ag%s\ax", PLUGIN_CHAT_MSG, pShareItem->Name, szName);
	}
	if (iLogLoot)
	{
		sprintf_s(szTemp, "%s :: SList: Attempting to distribute %s to %s", pChar->Name, pShareItem->Name, szName);
		CreateLogEntry(szTemp);
	}
	if (!WinState(pAdvancedLootWnd))
	{
		return true;
	}
	CListWnd *pPersonalList = (CListWnd *)pAdvancedLootWnd->GetChildItem("ADLW_PLLList");
	CListWnd *pSharedList = pAdvancedLootWnd->pCLootList->SharedLootList;
	LootTimer = pluginclock::now() + std::chrono::seconds(30); // Lets lock out the plugin from doing loot actions while we attempt to pass out items
	pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until I have an item in my cursor
	while (pluginclock::now() < WhileTimer) // While loop to wait till we are done with the previous looting command
	{
		if (!InGameOK() || !WinState(pAdvancedLootWnd))
		{
			return true;
		}
		if (!LootInProgress(pAdvancedLootWnd, pPersonalList, pSharedList))
		{
			WhileTimer = pluginclock::now();
		}
		Sleep(10);  // Sleep for 10 ms and lets check the previous conditions again
	}
	if (LootInProgress(pAdvancedLootWnd, pPersonalList, pSharedList))
	{
		return true;
	}
	if (pAdvancedLootWnd && pShareItem && pShareItem->LootDetails.GetSize() > 0)
	{
		if (pSharedList->GetItemData(ItemIndex) != -1)
		{
			auto pShareItemNew = &pAdvancedLootWnd->pCLootList->Items[(int)pSharedList->GetItemData(ItemIndex)];
			if (pShareItemNew && pShareItemNew->LootDetails.GetSize() > 0)
			{
				if (pShareItemNew->ItemID == pShareItem->ItemID)
				{
					bDistributeItemSucceeded = false;
					bDistributeItemFailed = false;
					pAdvancedLootWnd->DoSharedAdvLootAction(*pShareItemNew, szName, 0, pShareItemNew->LootDetails[0].StackCount);
					pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until I have an item in my cursor
					while (pluginclock::now() < WhileTimer) // While loop to wait till we are done with the previous looting command
					{
						if (!InGameOK() || !WinState(pAdvancedLootWnd))
						{
							return true;
						}
						if (bDistributeItemSucceeded)
						{
							return true;
						}
						if (bDistributeItemFailed)
						{
							return false;
						}
						Sleep(10);  // Sleep for 10 ms and lets check the previous conditions again
					}
				}
				else
				{
					DebugSpew("%s:: SList: Attempted to pass out an item but couldn't due to bad pShareItem->LootDetails", PLUGIN_DEBUG_MSG);
					return true;
				}
			}
		}
	}
	return true;
}

bool ContinueCheckingSharedLoot() // Will stop looping through the shared loot list when the number of items clicked need/greed is equal or exceeds the number of free inventory slots
{
	int iItemSelected = 0;
	if (pAdvancedLootWnd)
	{
		if (CListWnd *pSharedList = (CListWnd *)pAdvancedLootWnd->pCLootList->SharedLootList)
		{
			for (LONG k = 0; k < pSharedList->ItemsArray.Count; k++)
			{
				LONG listindex = (LONG)pSharedList->GetItemData(k);
				if (listindex != -1)
				{
					auto pShareItem = &pAdvancedLootWnd->pCLootList->Items[listindex];
					if (pShareItem->Need || pShareItem->Greed) // Ok lets check to see if we have need or greed boxes check
					{
						iItemSelected++;
					}
				}
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	if (iSaveBagSlots >= AutoLootFreeInventory() - iItemSelected)
	{
		return false;
	}
	return true;
}

int AutoLootFreeInventory()
{
	int freeslots = 0;
	if (auto pChar2 = GetPcProfile())
	{
		for (int slot = InvSlot_FirstBagSlot; slot < GetHighestAvailableBagSlot(); slot++)
		{
			if (ItemPtr pItem = pChar2->GetInventorySlot(slot))
			{
				if (pItem->IsContainer()
					&& _stricmp(pItem->GetName(), szExcludeBag1) != 0
					&& _stricmp(pItem->GetName(), szExcludeBag2) != 0)
				{
					freeslots += pItem->GetHeldItems().GetSize() - pItem->GetHeldItems().GetCount();
				}
			}
			else
			{
				freeslots++;
			}
		}
	}
	freeslots = freeslots - iSaveBagSlots;
	if (freeslots < 0)
	{
		return 0;
	}
	return freeslots;
}

bool DirectoryExists(LPCTSTR lpszPath)
{
	DWORD dw = ::GetFileAttributes(lpszPath);
	return (dw != INVALID_FILE_ATTRIBUTES && (dw & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

void CreateLogEntry(PCHAR szLogEntry)
{
	if (!DirectoryExists(szLogPath))
	{
		CreateDirectory(szLogPath, NULL);
		if (!DirectoryExists(szLogPath))
		{
			return;  //Shit i tried to create the directory and failed for some reason
		}
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
		WriteChatf("%s:: Couldn't open log file:", PLUGIN_CHAT_MSG);
		WriteChatf("%s:: \ar%s\ax", PLUGIN_CHAT_MSG, szLogFileName);
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
	if (!InGameOK())
	{
		return;
	}
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
		if (!_stricmp(Parm2, "on"))
		{
			iUseAutoLoot = SetBOOL(iUseAutoLoot, Parm2, GetCharInfo()->Name, "UseAutoLoot", INIFileName);
		}
		if (!_stricmp(Parm2, "off"))
		{
			iUseAutoLoot = SetBOOL(iUseAutoLoot, Parm2, GetCharInfo()->Name, "UseAutoLoot", INIFileName);
			if (!bEndThreads)
			{
				bEndThreads = true;
				return;
			}
		}
		WriteChatf("%s:: Set %s", PLUGIN_CHAT_MSG, iUseAutoLoot ? "\agON\ax" : "\arOFF\ax");
	}
	else if (!_stricmp(Parm1, "spamloot"))
	{
		if (!_stricmp(Parm2, "on"))
		{
			iSpamLootInfo = SetBOOL(iSpamLootInfo, Parm2, "Settings", "SpamLootInfo", szLootINI);
		}
		if (!_stricmp(Parm2, "off"))
		{
			iSpamLootInfo = SetBOOL(iSpamLootInfo, Parm2, "Settings", "SpamLootInfo", szLootINI);
		}
		WriteChatf("%s:: Spam looting actions %s", PLUGIN_CHAT_MSG, iSpamLootInfo ? "\agON\ax" : "\arOFF\ax");
	}
	else if (!_stricmp(Parm1, "logloot"))
	{
		if (!_stricmp(Parm2, "on"))
		{
			iLogLoot = SetBOOL(iLogLoot, Parm2, "Settings", "LogLoot", szLootINI);
		}
		if (!_stricmp(Parm2, "off"))
		{
			iLogLoot = SetBOOL(iLogLoot, Parm2, "Settings", "LogLoot", szLootINI);
		}
		WriteChatf("%s:: Logging loot actions for master looter is %s", PLUGIN_CHAT_MSG, iLogLoot ? "\agON\ax" : "\arOFF\ax");
	}
	else if (!_stricmp(Parm1, "raidloot"))
	{
		if (!_stricmp(Parm2, "on"))
		{
			iRaidLoot = SetBOOL(iRaidLoot, Parm2, "Settings", "RaidLoot", szLootINI);
		}
		if (!_stricmp(Parm2, "off"))
		{
			iRaidLoot = SetBOOL(iRaidLoot, Parm2, "Settings", "RaidLoot", szLootINI);
		}
		WriteChatf("%s:: Raid looting is turned: %s", PLUGIN_CHAT_MSG, iRaidLoot ? "\agON\ax" : "\arOFF\ax");
	}
	else if (!_stricmp(Parm1, "barterminimum"))
	{
		if (IsNumber(Parm2))
		{
			iBarMinSellPrice = GetIntFromString(Parm2, iBarMinSellPrice);
			WritePrivateProfileString("Settings", "BarMinSellPrice", Parm2, szLootINI);
			WriteChatf("%s:: Stop looting when \ag%d\ax slots are left", PLUGIN_CHAT_MSG, iBarMinSellPrice);
		}
		else
		{
			WriteChatf("%s:: Please send a valid number for your minimum barter price", PLUGIN_CHAT_MSG);
		}
	}
	else if (!_stricmp(Parm1, "saveslots"))
	{
		if (IsNumber(Parm2))
		{
			iSaveBagSlots = GetIntFromString(Parm2, iSaveBagSlots);
			WritePrivateProfileString("Settings", "SaveBagSlots", Parm2, szLootINI);
			WriteChatf("%s:: Stop looting when \ag%d\ax slots are left", PLUGIN_CHAT_MSG, iSaveBagSlots);
		}
		else
		{
			WriteChatf("%s:: Please send a valid number for the number of bag slots to save", PLUGIN_CHAT_MSG);
		}
	}
	else if (!_stricmp(Parm1, "distributedelay"))
	{
		if (IsNumber(Parm2))
		{
			iDistributeLootDelay = GetIntFromString(Parm2, iDistributeLootDelay);
			WritePrivateProfileString("Settings", "DistributeLootDelay", Parm2, szLootINI);
			WriteChatf("%s:: The master looter will wait \ag%d\ax seconds before trying to distribute loot", PLUGIN_CHAT_MSG, iDistributeLootDelay);
		}
		else
		{
			WriteChatf("%s:: Please send a valid number for the distribute loot delay", PLUGIN_CHAT_MSG);
		}
	}
	else if (!_stricmp(Parm1, "newitemdelay"))
	{
		if (IsNumber(Parm2))
		{
			iNewItemDelay = GetIntFromString(Parm2, iNewItemDelay);
			WritePrivateProfileString("Settings", "NewItemDelay", Parm2, szLootINI);
			WriteChatf("%s:: The master looter will wait \ag%d\ax seconds when a new item drops before looting that item", PLUGIN_CHAT_MSG, iNewItemDelay);
		}
		else
		{
			WriteChatf("%s:: Please send a valid number for the distribute loot delay", PLUGIN_CHAT_MSG);
		}
	}
	else if (!_stricmp(Parm1, "cursordelay"))
	{
		if (IsNumber(Parm2))
		{
			iCursorDelay = GetIntFromString(Parm2, iCursorDelay);
			WritePrivateProfileString("Settings", "CursorDelay", Parm2, szLootINI);
			WriteChatf("%s:: You will wait \ag%d\ax seconds before trying to autoinventory items on your cursor", PLUGIN_CHAT_MSG, iCursorDelay);
		}
		else
		{
			WriteChatf("%s:: Please send a valid number for the cursor delay", PLUGIN_CHAT_MSG);
		}
	}
	else if (!_stricmp(Parm1, "questkeep"))
	{
		if (IsNumber(Parm2))
		{
			iQuestKeep = GetIntFromString(Parm2, iQuestKeep);
			WritePrivateProfileString("Settings", "QuestKeep", Parm2, szLootINI);
			WriteChatf("%s:: Your default number to keep of new no drop items is: \ag%d\ax", PLUGIN_CHAT_MSG, iQuestKeep);
		}
		else
		{
			WriteChatf("%s:: Please send a valid number for default number of quest items to keep", PLUGIN_CHAT_MSG);
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
			WriteChatf("%s:: \ar%s\ax is an invalid entry, please use [quest|keep|ignore]", PLUGIN_CHAT_MSG, szNoDropDefault);
			return;
		}
		WritePrivateProfileString("Settings", "NoDropDefault", szNoDropDefault, szLootINI);
		WriteChatf("%s:: Your default for new no drop items is: \ag%s\ax", PLUGIN_CHAT_MSG, szNoDropDefault);
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
			WriteChatf("%s:: \ar%s\ax is an invalid entry, please use [view only|public if usable|public]", PLUGIN_CHAT_MSG, szGuildItemPermission);
			return;
		}
		WritePrivateProfileString("Settings", "GuildItemPermission", szGuildItemPermission, szLootINI);
		WriteChatf("%s:: Your default permission for items put into your guild bank is: \ag%s\ax", PLUGIN_CHAT_MSG, szGuildItemPermission);
	}
	else if (!_stricmp(Parm1, "excludebag1"))
	{
		WritePrivateProfileString("Settings", "ExcludeBag1", Parm2, szLootINI);
		sprintf_s(szExcludeBag1, "%s", Parm2);
		WriteChatf("%s:: Will exclude \ar%s\ax when checking for free slots", PLUGIN_CHAT_MSG, szExcludeBag1);
	}
	else if (!_stricmp(Parm1, "excludebag2"))
	{
		WritePrivateProfileString("Settings", "ExcludeBag2", Parm2, szLootINI);
		sprintf_s(szExcludeBag2, "%s", Parm2);
		WriteChatf("%s:: Will exclude \ar%s\ax when checking for free slots", PLUGIN_CHAT_MSG, szExcludeBag2);
	}
	else if (!_stricmp(Parm1, "lootini"))
	{
		sprintf_s(szLootINI, "%s\\%s.ini", gPathConfig, Parm2);
		WritePrivateProfileString(GetCharInfo()->Name, "lootini", szLootINI, INIFileName);
		WriteChatf("%s:: The location for your loot ini is:\n \ag%s\ax", PLUGIN_CHAT_MSG, szLootINI);
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
		hBarterItemsThread = CreateThread(NULL, NULL, BarterItems, (PVOID)0, 0, &nThreadID);
	}
	else if (!_stricmp(Parm1, "buy"))
	{
		if (!bEndThreads)
		{
			bEndThreads = true;
			return;
		}
		CHAR szTemp1[MAX_STRING] = { 0 };
		sprintf_s(szTemp1, "%s|%s", Parm2, Parm3);
		if (IsNumber(Parm3))
		{
			DWORD nThreadID = 0;
			hBuyItemThread = CreateThread(NULL, NULL, BuyItem, _strdup(szTemp1), 0, &nThreadID);
		}
		else
		{
			WriteChatf("%s:: Invalid buy command", PLUGIN_CHAT_MSG);
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
		hSellItemsThread = CreateThread(NULL, NULL, SellItems, (PVOID)0, 0, &nThreadID);
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
				hDepositPersonalBankerThread = CreateThread(NULL, NULL, DepositPersonalBanker, (PVOID)0, 0, &nThreadID);
			}
			else if (psTarget->mActorClient.Class == GUILDBANKER_CLASS)
			{
				DWORD nThreadID = 0;
				hDepositGuildBankerThread = CreateThread(NULL, NULL, DepositGuildBanker, (PVOID)0, 0, &nThreadID);
			}
			else
			{
				WriteChatf("%s:: Please target a guild/personal banker!", PLUGIN_CHAT_MSG);
			}
		}
		else
		{
			WriteChatf("%s:: Please target a guild/personal banker!", PLUGIN_CHAT_MSG);
		}
	}
	else if (!_stricmp(Parm1, "test"))
	{
		WriteChatf("%s:: Testing stuff, please ignore this command.  I will remove it later once plugin is done", PLUGIN_CHAT_MSG);
	}
	else if (!_stricmp(Parm1, "status"))
	{
		ShowInfo();
	}
	else if (!_stricmp(Parm1, "sort"))
	{
		sort_auto_loot(szLootINI, [](auto msg) {WriteChatf("%s:: %s", PLUGIN_CHAT_MSG, msg.c_str()); });
	}
	else
	{
		NeedHelp = true;
	}
	if (NeedHelp)
	{
		WriteChatColor("Usage:");
		WriteChatColor("/AutoLoot -> displays settings");
		WriteChatColor("/AutoLoot reload -> AutoLoot will reload variables from MQ2AutoLoot.ini");
		WriteChatColor("/AutoLoot turn [on|off] -> Toggle autoloot");
		WriteChatColor("/AutoLoot spamloot [on|off] -> Toggle loot action messages");
		WriteChatColor("/AutoLoot logloot [on|off] -> Toggle logging of loot actions for the master looter");
		WriteChatColor("/AutoLoot raidloot [on|off] -> Toggle raid looting on and off");
		WriteChatColor("/AutoLoot saveslots #n -> Stops looting when #n slots are left");
		WriteChatColor("/AutoLoot distributedelay #n -> Master looter waits #n seconds to try and distribute the loot");
		WriteChatColor("/AutoLoot newitemdelay #n -> Master looter waits #n seconds when a new item drops before looting that item");
		WriteChatColor("/AutoLoot cursordelay #n -> You will wait #n seconds before trying to autoinventory items on your cursor");
		WriteChatColor("/AutoLoot barterminimum #n -> The minimum price for all items to be bartered is #n");
		WriteChatColor("/AutoLoot questkeep #n -> If nodropdefault is set to quest your new no drop items will be set to Quest|#n");
		WriteChatColor("/AutoLoot nodropdefault [quest|keep|ignore] -> Will set new no drop items to this value");
		WriteChatColor("/AutoLoot excludebag1 XXX -> Will exclude bags named XXX when checking for how many slots you are free");
		WriteChatColor("/AutoLoot excludebag2 XXX -> Will exclude bags named XXX when checking for how many slots you are free");
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
		auto pChar2 = GetPcProfile();

		if (CONTENTS* pItem = pChar2->GetInventorySlot(InvSlot_Cursor))
		{
			CreateLootEntry(Parm1, Parm2, GetItemFromContents(pItem));
		}
		else
		{
			WriteChatf("%s:: There is no item on your cursor, please pick up the item and resend the command", PLUGIN_CHAT_MSG);
		}
	}
	else
	{
		NeedHelp = TRUE;
	}
	if (NeedHelp)
	{
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
		WriteChatf("%s:: Setting \ag%s\ax to \agKeep\ax", PLUGIN_CHAT_MSG, pItem->Name);
	}
	else if (!_stricmp(szAction, "Sell"))
	{
		WritePrivateProfileString(INISection, pItem->Name, "Sell", szLootINI);
		WriteChatf("%s:: Setting \ag%s\ax to \agSell\ax", PLUGIN_CHAT_MSG, pItem->Name);
	}
	else if (!_stricmp(szAction, "Deposit"))
	{
		WritePrivateProfileString(INISection, pItem->Name, "Deposit", szLootINI);
		WriteChatf("%s:: Setting \ag%s\ax to \agDeposit\ax", PLUGIN_CHAT_MSG, pItem->Name);
	}
	else if (!_stricmp(szAction, "Ignore"))
	{
		WritePrivateProfileString(INISection, pItem->Name, "Ignore", szLootINI);
		WriteChatf("%s:: Setting \ag%s\ax to \arIgnore\ax", PLUGIN_CHAT_MSG, pItem->Name);
	}
	else if (!_stricmp(szAction, "Destroy"))
	{
		WritePrivateProfileString(INISection, pItem->Name, "Destroy", szLootINI);
		WriteChatf("%s:: Setting \ag%s\ax to \arDestroy\ax", PLUGIN_CHAT_MSG, pItem->Name);
	}
	else if (!_stricmp(szAction, "Quest"))
	{
		int QuestNumber = GetIntFromString(szEntry, 0);
		sprintf_s(INIValue, "Quest|%d", QuestNumber);
		WritePrivateProfileString(INISection, pItem->Name, INIValue, szLootINI);
		WriteChatf("%s:: Setting \ag%s\ax to \ag%s\ax", PLUGIN_CHAT_MSG, pItem->Name, INIValue);
	}
	else if (!_stricmp(szAction, "Barter"))
	{
		int BarterNumber = GetIntFromString(szEntry, 0);
		sprintf_s(INIValue, "Barter|%d", BarterNumber);
		WritePrivateProfileString(INISection, pItem->Name, INIValue, szLootINI);
		WriteChatf("%s:: Setting \ag%s\ax to \ag%s\ax", PLUGIN_CHAT_MSG, pItem->Name, INIValue);
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
		WriteChatf("%s:: Setting \ag%s\ax to:", PLUGIN_CHAT_MSG, pItem->Name);
		WriteChatf("%s:: \ag%s\ax", PLUGIN_CHAT_MSG, INIValue);
	}
	else if (!_stricmp(szAction, "Status"))
	{
		CHAR Value[MAX_STRING] = { 0 };
		if (GetPrivateProfileString(INISection, pItem->Name, 0, Value, MAX_STRING, szLootINI) == 0)
		{
			WriteChatf("%s:: \ag%s\ax is not in your loot.ini", PLUGIN_CHAT_MSG, pItem->Name);
		}
		else
		{
			WriteChatf("%s:: \ag%s\ax is set to \ag%s\ax", PLUGIN_CHAT_MSG, pItem->Name, Value);
		}
	}
	else
	{
		WriteChatf("%s:: Invalid command.  The accepted commands are [Quest #n|Gear|Keep|Sell|Ignore|Destroy]", PLUGIN_CHAT_MSG);
	}

	if (auto pChar2 = GetPcProfile())
	{
		if (CONTENTS* pItem = pChar2->GetInventorySlot(InvSlot_Cursor))
		{
			if (!_stricmp(szAction, "Destroy"))
			{
				if (iSpamLootInfo)
				{
					WriteChatf("%s:: Destroying \ar%s\ax", PLUGIN_CHAT_MSG, pItem->GetName());
				}
				DoCommand(GetCharInfo()->pSpawn, "/destroy");
			}
			else
			{
				if (FitInInventory(pItem->GetItemDefinition()->Size))
				{
					if (iSpamLootInfo)
					{
						WriteChatf("%s:: Putting \ag%s\ax into my inventory", PLUGIN_CHAT_MSG, pItem->GetName());
					}
					DoCommand(GetCharInfo()->pSpawn, "/autoinventory");
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

	virtual bool GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar &Dest) override
	{
		MQTypeMember* pMember = MQ2AutoLootType::FindMember(Member);
		if (pMember)
		{
			switch ((AutoLootMembers)pMember->ID)
			{
			case Active:
				Dest.DWord = iUseAutoLoot;
				Dest.Type = mq::datatypes::pBoolType;
				return true;
			case SellActive:
				Dest.DWord = bSellActive;
				Dest.Type = mq::datatypes::pBoolType;
				return true;
			case BuyActive:
				Dest.DWord = bBuyActive;
				Dest.Type = mq::datatypes::pBoolType;
				return true;
			case DepositActive:
				Dest.DWord = bDepositActive;
				Dest.Type = mq::datatypes::pBoolType;
				return true;
			case BarterActive:
				Dest.DWord = bBarterActive;
				Dest.Type = mq::datatypes::pBoolType;
				return true;
			case FreeInventory:
				Dest.DWord = AutoLootFreeInventory();
				Dest.Type = mq::datatypes::pIntType;
				return true;
			}
			return false;
		}
		return false;
	}

	bool ToString(MQVarPtr VarPtr, PCHAR Destination)
	{
		strcpy_s(Destination, MAX_STRING, "TRUE");
		return true;
	}
	bool FromData(MQVarPtr &VarPtr, MQTypeVar &Source)
	{
		return false;
	}
	virtual bool FromString(MQVarPtr &VarPtr, const char* Source) override
	{
		return false;
	}
	~MQ2AutoLootType()
	{

	}
};

bool dataAutoLoot(const char* szName, MQTypeVar &Ret)
{
	Ret.DWord = 1;
	Ret.Type = pAutoLootType;
	return true;
}

/********************************************************************************
****
**     Functions necessary for interacting with MQ2
****
********************************************************************************/

PLUGIN_API void InitializePlugin()
{
	// commands
	AddCommand("/autoloot", AutoLootCommand);
	AddCommand("/setitem", SetItemCommand);

	// TLOs
	AddMQ2Data("AutoLoot", dataAutoLoot);

	pAutoLootType = new MQ2AutoLootType;
}

PLUGIN_API void ShutdownPlugin()
{
	// remove commands
	RemoveCommand("/autoloot");
	RemoveCommand("/setitem");

	// remove TLOs
	RemoveMQ2Data("AutoLoot");

	// remove any threads we have
	if (hPassOutLootThread)
	{
		TerminateThread(hPassOutLootThread, 0);
	}
	if (hBarterItemsThread)
	{
		TerminateThread(hBarterItemsThread, 0);
	}
	if (hBuyItemThread)
	{
		TerminateThread(hBuyItemThread, 0);
	}
	if (hSellItemsThread)
	{
		TerminateThread(hSellItemsThread, 0);
	}
	if (hDepositPersonalBankerThread)
	{
		TerminateThread(hDepositPersonalBankerThread, 0);
	}
	if (hDepositGuildBankerThread)
	{
		TerminateThread(hDepositGuildBankerThread, 0);
	}

	delete pAutoLootType;
}

PLUGIN_API void SetGameState(int GameState)
{
	Initialized = false;
	if (!InGameOK())
	{
		return;
	}
	if (GameState == GAMESTATE_INGAME)
	{
		SetAutoLootVariables();
		Initialized = true;
	}
}

PLUGIN_API bool OnIncomingChat(const char* Line, DWORD Color)
{
	if (!InGameOK())
	{
		return false;
	}

	if (WinState(pAdvancedLootWnd))
	{
		if (strstr(Line, "It is either on their never list or they have selected No."))
		{
			bDistributeItemFailed = true;
			return false;
		}
		if (strstr(Line, "already has") && strstr(Line, "and it is lore."))
		{
			bDistributeItemFailed = true;
			return false;
		}
		if (strstr(Line, "A ") && strstr(Line, " was given to "))
		{
			bDistributeItemSucceeded = true;
			return false;
		}
	}

	if (WinState(pMerchantWnd) && pMerchantWnd->GetFirstChildWnd() && pMerchantWnd->GetFirstChildWnd()->GetNextSiblingWnd())
	{
		CHAR MerchantText[MAX_STRING] = { 0 };
		auto MerchantName = pMerchantWnd->GetFirstChildWnd()->GetWindowText();
		sprintf_s(MerchantText, "%s says, 'Hi there, %s. Just browsing?  Have you seen the ", MerchantName.c_str(), GetCharInfo()->Name); // Confirmed 04/15/2017
		if (strstr(Line, MerchantText)) // Merchant window is populated
		{
			LootThreadTimer = pluginclock::now();
			return false;
		}
		sprintf_s(MerchantText, "%s says, 'Welcome to my shop, %s. You would probably find a ", MerchantName.c_str(), GetCharInfo()->Name); // Confirmed 04/15/2017
		if (strstr(Line, MerchantText)) // Merchant window is populated
		{
			LootThreadTimer = pluginclock::now();
			return false;
		}
		sprintf_s(MerchantText, "%s says, 'Hello there, %s. How about a nice ", MerchantName.c_str(), GetCharInfo()->Name); // Confirmed 04/15/2017
		if (strstr(Line, MerchantText)) // Merchant window is populated
		{
			LootThreadTimer = pluginclock::now();
			return false;
		}
		sprintf_s(MerchantText, "%s says, 'Greetings, %s. You look like you could use a ", MerchantName.c_str(), GetCharInfo()->Name); // Confirmed 04/15/2017
		if (strstr(Line, MerchantText)) // Merchant window is populated
		{
			LootThreadTimer = pluginclock::now();
			return false;
		}
	}

	if (strstr(Line, "tells you, 'Welcome to my bank!")) // Banker window is populated
	{
		LootThreadTimer = pluginclock::now() + std::chrono::milliseconds(2000); // TODO determine what is an appropriate delay
	}
	else if (WinState(FindMQ2Window("BarterSearchWnd")))
	{
		if (strstr(Line, "You've sold ") && strstr(Line, " to ") && strstr(Line, "."))
		{
			bBarterItemSold = true;
		}
		// Taken from HoosierBilly, has not beem checked
		else if (strstr(Line, "Your transaction failed because your barter data is out of date.")) //Need to research to refresh the barter search window
		{
			bBarterReset = true;
		}
	}

	return false;
}

PLUGIN_API void OnPulse()
{
	if (!InGameOK())
	{
		return;
	}
	PCHARINFO pChar = GetCharInfo();
	auto pChar2 = GetPcProfile();
	if (DoThreadAction())
	{
		return; // Do actions from threads that need to be in the pulse to stop crashing to desktop
	}
	if (!iUseAutoLoot || !pChar->UseAdvancedLooting) // Oh shit we aren't using advanced looting or we have turned off the plugin
	{
		return;
	}
	bool ItemOnCursor = CheckCursor(); // Check cursor for items, and will put in inventory if it fits after CursorDelay has been exceed
	if (CheckWindows(ItemOnCursor)) // Need to have this before the pluginclock::now() < LootTimer check, otherwise it won't loot no drop items marked for destroy before DestroyStuffCancelTimer runs out
	{
		return; // Returns true if your attempting to accept trade requests or click the confirmation box for no drop items
	}
	if (DestroyStuff()) // When you loot an item marked Destroy it will set the DestroyID to that item's ID and proceed to pick that item from inventory and destroy before resetting DestroyID to 0
	{
		return;
	}
	if (pluginclock::now() < LootTimer)
	{
		return; // Ok, LootTimer isn't counted down yet
	}
	// Ok, we have done all our prechecks lets do looting things now
	if (pRaid && pRaid->RaidMemberCount > 0 && !iRaidLoot)
	{
		return; // Your in a raid and don't want to use mq2autoloot, turning mq2autoloot off
	}
	if (!WinState(pAdvancedLootWnd))
	{
		ClearLootEntries();
		return;
	}
	if (SetLootSettings())
	{
		return; // Turn off Auto Loot All
	}
	CListWnd *pPersonalList = (CListWnd *)pAdvancedLootWnd->GetChildItem("ADLW_PLLList");
	CListWnd *pSharedList = (CListWnd *)pAdvancedLootWnd->pCLootList->SharedLootList;
	if (LootInProgress(pAdvancedLootWnd, pPersonalList, pSharedList))
	{
		return;
	}
	if (pPersonalList)
	{
		if (HandlePersonalLoot(ItemOnCursor, pChar, pChar2, pAdvancedLootWnd, pPersonalList, pSharedList))
		{
			return;
		}
	}
	if (pSharedList)
	{
		if (HandleSharedLoot(ItemOnCursor, pChar, pChar2, pAdvancedLootWnd, pPersonalList, pSharedList))
		{
			return;
		}
	}
}
#endif
