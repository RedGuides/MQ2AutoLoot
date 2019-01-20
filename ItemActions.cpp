#include "../MQ2Plugin.h"
#if !defined(ROF2EMU) && !defined(UFEMU)
#include "MQ2AutoLoot.h"
#include "ItemActions.h"
#include <chrono>

using namespace std;
typedef std::chrono::high_resolution_clock pluginclock;

bool* pbStickOn; //MoveUtils 11.x
void(*fStickCommand)(PSPAWNINFO pChar, char* szLine); //MoveUtils 11.x

bool						bBarterActive = false;
bool						bDepositActive = false;
bool						bBuyActive = false;
bool						bSellActive = false;
bool						bBarterSearchDone = false; // Set to true when your search has finished
bool						bBarterReset = false; // Set to true when you need to refresh your barter search
bool						bBarterItemSold; // Set to true when you sell an item
bool						bEndThreads = true;  // Set to true when you want to end any threads out here, also it is used to enforce that at most a single thread is active at one time
pluginclock::time_point		LootThreadTimer = pluginclock::now();

void ResetItemActions(void)
{
	if (WinState((CXWnd*)pMerchantWnd))
	{
		HideDoCommand(GetCharInfo()->pSpawn, "/squelch /windowstate MerchantWnd close", true);
	}
	if (WinState((CXWnd*)pBankWnd))
	{
		HideDoCommand(GetCharInfo()->pSpawn, "/squelch /windowstate BigBankWnd close", true);
	}
	if (WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))
	{
		HideDoCommand(GetCharInfo()->pSpawn, "/squelch /windowstate GuildBankWnd close", true);
	}
	if (WinState((CXWnd*)FindMQ2Window("BarterSearchWnd")))
	{
		HideDoCommand(GetCharInfo()->pSpawn, "/squelch /windowstate BarterSearchWnd close", true);
	}
	if (bBarterActive)
	{
		WriteChatfSafe("%s:: Finished bartering!!", PLUGIN_MSG);
		bBarterActive = false;
	}
	if (bBuyActive)
	{
		WriteChatfSafe("%s:: Finished buying!!", PLUGIN_MSG);
		bBuyActive = false;
	}
	if (bDepositActive)
	{
		WriteChatfSafe("%s:: Finished depositing stuff!!", PLUGIN_MSG);
		bDepositActive = false;
	}
	if (bSellActive)
	{
		WriteChatfSafe("%s:: Finished selling your stuff!!", PLUGIN_MSG);
		bSellActive = false;
	}
	bEndThreads = true;
}

bool MoveToNPC(PSPAWNINFO pSpawn)
{
	if (!InGameOK() || bEndThreads)
	{
		bEndThreads = true;
		return false;
	}
	if (PCHARINFO pChar = GetCharInfo())
	{
		if (pSpawn->X && pSpawn->Y && pSpawn->Z)
		{
			FLOAT Distance = Get3DDistance(pChar->pSpawn->X, pChar->pSpawn->Y, pChar->pSpawn->Z, pSpawn->X, pSpawn->Y, pSpawn->Z);
			if (Distance >= 20)  // I am too far away from the merchant/banker and need to move closer 
			{
				if (HandleMoveUtils())
				{
					sprintf_s(szCommand, "id %d", pSpawn->SpawnID);
					fStickCommand(GetCharInfo()->pSpawn, szCommand);
					WriteChatfSafe("%s:: Moving towards: \ag%s\ax", PLUGIN_MSG, pSpawn->DisplayedName);
				}
				else
				{
					WriteChatfSafe("%s:: Hey friend, you don't have MQ2MoveUtils loaded.  Move to within 20 of the target before trying to sell or deposit", PLUGIN_MSG);
					return false;
				}
				pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::seconds(30); // Will wait up to 30 seconds or until you are within 20 of the guild banker
				while (pluginclock::now() < WhileTimer) // While loop to wait for npc to aggro me while i'm casting
				{
					if (!InGameOK() || bEndThreads)
					{
						return false; // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
					}
					if (pSpawn->X && pSpawn->Y && pSpawn->Z)
					{
						FLOAT Distance = Get3DDistance(pChar->pSpawn->X, pChar->pSpawn->Y, pChar->pSpawn->Z, pSpawn->X, pSpawn->Y, pSpawn->Z);
						if (Distance < 20)
						{
							break; // I am close enough to the banker I can stop moving towards them 
						}
					}
					else
					{
						WriteChatfSafe("%s:: For some reason I can't find the distance to my banker/merchant!", PLUGIN_MSG);
						return false;
					}
					Sleep(100);
				}
				Sleep(100); // Lets give us another 100 ms before to stick off
				if (!InGameOK() || bEndThreads)
				{
					return false; // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
				}
				if (*pbStickOn)
				{
					fStickCommand(GetCharInfo()->pSpawn, "off"); // We either got close enough or timed out, lets turn stick off
				}
				if (pSpawn->X && pSpawn->Y && pSpawn->Z)
				{
					FLOAT Distance = Get3DDistance(pChar->pSpawn->X, pChar->pSpawn->Y, pChar->pSpawn->Z, pSpawn->X, pSpawn->Y, pSpawn->Z);
					if (Distance < 20)
					{
						return true; // I am close enough to the banker/merchant to open their window
					}
					else
					{
						WriteChatfSafe("%s:: I wasn't able to get to within 20 of my banker/merchant!", PLUGIN_MSG);
						return false;
					}
				}
				else
				{
					WriteChatfSafe("%s:: For some reason I can't find the distance to !", PLUGIN_MSG);
					return false;
				}
			}
			else
			{
				return true; // Hey you already were within 20 of the spawn
			}
		}
		else
		{
			WriteChatfSafe("%s:: For some reason I can't find the distance to your npc!", PLUGIN_MSG);
			return false; // This shouldn't fail, but for some reason I wasn't able to get the location of the spawn I want to navigate to
		}
	}
	else
	{
		WriteChatfSafe("%s:: For some reason GetCharInfo() failed!", PLUGIN_MSG);
		return false; // This shouldn't fail, but for some reason I wasn't able to get the character's GetCharInfo()
	}
}

bool HandleMoveUtils(void)
{
	fStickCommand = NULL;
	pbStickOn = NULL;
	if (PMQPLUGIN pLook = Plugin("mq2moveutils"))
	{
		fStickCommand = (void(*)(PSPAWNINFO pChar, char* szLine))GetProcAddress(pLook->hModule, "StickCommand");
		pbStickOn = (bool *)GetProcAddress(pLook->hModule, "bStickOn");
	}
	if (fStickCommand && pbStickOn)
	{
		return true;
	}
	return false;
}

bool OpenWindow(PSPAWNINFO pSpawn)
{
	if (!InGameOK() || bEndThreads)
	{
		bEndThreads = true;
		return false;
	}
	if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
	{
		if (psTarget->SpawnID == pSpawn->SpawnID)
		{
			if (pSpawn->mActorClient.Class == MERCHANT_CLASS) 
			{ 
				WriteChatfSafe("%s:: Opening merchant window and waiting 30 seconds for the merchant window to populate", PLUGIN_MSG); 
			}
			if (pSpawn->mActorClient.Class == PERSONALBANKER_CLASS) 
			{ 
				WriteChatfSafe("%s:: Opening personal banking window and waiting 30 seconds for the personal banking window to populate", PLUGIN_MSG); 
			}
			if (pSpawn->mActorClient.Class == GUILDBANKER_CLASS) 
			{ 
				WriteChatfSafe("%s:: Opening guild banking window and waiting 30 seconds for the guild banking window to populate", PLUGIN_MSG); 
			}
			HideDoCommand(GetCharInfo()->pSpawn, "/face nolook",true);
			HideDoCommand(GetCharInfo()->pSpawn, "/nomodkey /click right target",true);
			pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::seconds(30); // Will wait up to 30 seconds or until the window is open
			while (pluginclock::now() < WhileTimer) // While loop to wait for something to pop onto my cursor
			{
				Sleep(100);  // Sleep for 100 ms and lets check if the window is open
				if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
				{
					bEndThreads = true;
					return false;
				}
				if (WinState((CXWnd*)FindMQ2Window("GuildBankWnd")) || WinState((CXWnd*)pBankWnd) || WinState((CXWnd*)pMerchantWnd)) 
				{ 
					return true; 
				}
			}
			return false;
		}
		else
		{
			WriteChatfSafe("%s:: Your target has changed!", PLUGIN_MSG);
			bEndThreads = true;
			return false;
		}
	}
	else
	{
		WriteChatfSafe("%s:: You don't have a target!", PLUGIN_MSG);
		bEndThreads = true;
		return false;
	}
}

bool CheckIfItemIsInSlot(short InvSlot, short BagSlot)
{
	if (PCONTENTS pTempItem = FindItemBySlot(InvSlot, BagSlot, eItemContainerPossessions))
	{
		return true; // shit still there
	}
	else
	{
		return false; // the item is gone
	}
}

bool WaitForItemToBeSelected(PCONTENTS pItem, short InvSlot, short BagSlot)
{
	Sleep(750);  // Lets see if 750 ms is sufficient of a delay, TODO figure out a smarter way to determine when a merchant is ready to buy an item
	if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pMerchantWnd))
	{
		return false;
	}
	if (!CheckIfItemIsInSlot(InvSlot, BagSlot)) // There is no item in 
	{
		return false;
	}
	if (CMerchantWnd * pMerchWnd = (CMerchantWnd *)((PEQMERCHWINDOW)pMerchantWnd))
	{
		if (PCONTENTS pSelectedItem = pMerchWnd->pSelectedItem.pObject)
		{
			if (pSelectedItem->GlobalIndex.Index.Slot1 == pItem->GlobalIndex.Index.Slot1)
			{ // This is true immediately when we first enter this function... 
				if (pSelectedItem->GlobalIndex.Index.Slot2 == pItem->GlobalIndex.Index.Slot2)
				{ // we need some delay otherwise it doesn't sell and we get the message "I am not interested in buying that."
					return true; //adding this check incase someone clicks on a different item 
				}
			}
		}
	}
	return false;
}

bool WaitForItemToBeSold(short InvSlot, short BagSlot)
{
	pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::milliseconds(5000);
	while (pluginclock::now() < WhileTimer) // Will wait up to 5 seconds or until the item is removed from that slot
	{
		Sleep(100); 
		if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pMerchantWnd))
		{
			return true; // Return true will result in leaving the SellItem function
		}
		if (!CheckIfItemIsInSlot(InvSlot, BagSlot)) // There is no item in that slot
		{
			return true;
		}
	}
	return false; // we didn't sell the item, going to keep in the SellItem function till it sells, or 30 seconds runs out
}

void SellItem(PCONTENTS pItem)
{
	CHAR szCount[MAX_STRING] = { 0 };
	short InvSlot = pItem->GlobalIndex.Index.Slot1;
	short BagSlot = pItem->GlobalIndex.Index.Slot2;
	if (PITEMINFO theitem = GetItemFromContents(pItem))
	{
		if ((theitem->Type != ITEMTYPE_NORMAL) || (((EQ_Item*)pItem)->IsStackable() != 1))
		{
			sprintf_s(szCount, "1");
		}
		else
		{
			sprintf_s(szCount, "%d", pItem->StackCount);
		}
		if (PickupItem(eItemContainerPossessions, pItem))
		{
			pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::seconds(30);
			while (pluginclock::now() < WhileTimer) // Will wait up to 30 seconds or until I get the merchant buys the item
			{
				if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pMerchantWnd))
				{
					return;
				}
				if (WaitForItemToBeSelected(pItem, InvSlot, BagSlot))
				{
					if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pMerchantWnd))
					{
						return;
					}
					if (!CheckIfItemIsInSlot(InvSlot, BagSlot))
					{
						return;
					}
					else
					{
						if (PCHARINFO pChar = GetCharInfo())
						{
							if (pMerchantWnd->SellButton->dShow)
							{
								if (pMerchantWnd->SellButton->Enabled)
								{
									SellItem(pChar->pSpawn, szCount); // This is the command from MQ2Commands.cpp
									if (WaitForItemToBeSold(InvSlot, BagSlot))
									{
										return;
									}
								}
							}
						}
					}
				}
				else
				{
					return;
				}
			}
		}
	}
}

bool FitInPersonalBank(PITEMINFO pItem)
{
	bool FitInStack = false;
	LONG nPack = 0;
	LONG Count = 0;
	PCHARINFO pChar = GetCharInfo();
	CHAR INISection[MAX_STRING] = { 0 };
	CHAR Value[MAX_STRING] = { 0 };
	sprintf_s(INISection, "%c", pItem->Name[0]);
	if (GetPrivateProfileString(INISection, pItem->Name, 0, Value, MAX_STRING, szLootINI) != 0)
	{
		CHAR *pParsedToken = NULL;
		CHAR *pParsedValue = strtok_s(Value, "|", &pParsedToken);
		if (_stricmp(pParsedValue, "Keep"))
		{
			return false; // The item isn't set to Keep
		}
	}
	else
	{
		return false;  // The item isn't in your loot.ini file
	}
	for (nPack = 0; nPack < NUM_BANK_SLOTS; nPack++)
	{
		if (pChar->pBankArray)
		{
			if (!pChar->pBankArray->Bank[nPack])
			{
				return true;
			}
		}
		else
		{
			return true;
		}
	}
	//checking inside bank bags
	for (nPack = 0; nPack < NUM_BANK_SLOTS; nPack++) //checking bank slots
	{
		if (pChar->pBankArray)
		{
			if (PCONTENTS pPack = pChar->pBankArray->Bank[nPack])
			{
				if (PITEMINFO pItemPack = GetItemFromContents(pPack))
				{
					if (pItemPack->Type == ITEMTYPE_PACK && (_stricmp(pItemPack->Name, szExcludedBag1) || _stricmp(pItemPack->Name, szExcludedBag2)))
					{
						if (pPack->Contents.ContainedItems.pItems)
						{
							for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
							{
								if (!pPack->Contents.ContainedItems.pItems->Item[nItem])
								{
									if (pItemPack->SizeCapacity >= pItem->Size)
									{
										return true; // This bag slot is empty and out item fits
									}
								}
							}
						}
						else
						{
							if (pItemPack->SizeCapacity >= pItem->Size)
							{
								return true; // The bag array is empty, this means it was never initialized and thus should be empty
							}
						}
					}
				}
			}
		}
	}
	return false;
}

void PutInPersonalBank(PITEMINFO pItem)
{
	if (!InGameOK() || bEndThreads)
	{
		bEndThreads = true;
		return;
	}
	if (pItem->Name)
	{
		sprintf_s(szCommand, "/nomodkey /itemnotify \"%s\" leftmouseup", pItem->Name);
		HideDoCommand(GetCharInfo()->pSpawn, szCommand,true);
		pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until I have an item in my cursor
		while (pluginclock::now() < WhileTimer) // While loop to wait for something to pop onto my cursor
		{
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
			if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				bEndThreads = true;
				return;
			}
			if (PCHARINFO2 pChar2 = GetCharInfo2())
			{
				if (pChar2->pInventoryArray && pChar2->pInventoryArray->Inventory.Cursor)
				{
					break;
				}
			}
		}
		if (PCHARINFO2 pChar2 = GetCharInfo2())
		{
			if (!pChar2->pInventoryArray || !pChar2->pInventoryArray->Inventory.Cursor)
			{
				bEndThreads = true;
			}
			if (!InGameOK() || bEndThreads)
			{
				bEndThreads = true;
				return;
			}
			if (pChar2->pInventoryArray && pChar2->pInventoryArray->Inventory.Cursor)
			{
				if (PCONTENTS pItem = pChar2->pInventoryArray->Inventory.Cursor)
				{
					if (WinState((CXWnd*)pBankWnd))
					{
						if (CXWnd *pWndButton = pBankWnd->GetChildItem("BIGB_AutoButton"))
						{
							if (FitInPersonalBank(GetItemFromContents(pItem)))
							{
								if (iSpamLootInfo)
								{
									WriteChatfSafe("%s:: Putting \ag%s\ax into my personal bank", PLUGIN_MSG, pItem->Item2->Name);
								}
								SendWndClick2(pWndButton, "leftmouseup");
								Sleep(1000);
								return;
							}
						}
					}
				}
			}
		}
		else
		{

			bEndThreads = true;
			return;
		}
	}
}

bool CheckGuildBank(PITEMINFO pItem)
{
	if (!InGameOK() || bEndThreads)
	{
		bEndThreads = true;
		return false;
	}
	CHAR INISection[MAX_STRING] = { 0 };
	CHAR Value[MAX_STRING] = { 0 };
	sprintf_s(INISection, "%c", pItem->Name[0]);
	if (GetPrivateProfileString(INISection, pItem->Name, 0, Value, MAX_STRING, szLootINI) != 0)
	{
		CHAR *pParsedToken = NULL;
		CHAR *pParsedValue = strtok_s(Value, "|", &pParsedToken);
		if (!_stricmp(pParsedValue, "Deposit"))
		{
			if (WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))
			{
				if (CXWnd *pWnd = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositCountLabel"))
				{
					CHAR szDepositCountText[MAX_STRING] = { 0 };
					GetCXStr(pWnd->WindowText, szDepositCountText, MAX_STRING);
					CHAR *pParsedToken = NULL;
					CHAR *pParsedValue = strtok_s(szDepositCountText, ":", &pParsedToken);
					pParsedValue = strtok_s(NULL, ":", &pParsedToken);
					DWORD SlotsLeft = atoi(pParsedValue);
					bool bLoreItem = false;
					if (pItem->Lore != 0)
					{
						CXStr	cxstrItemName;
						CHAR	szItemName[MAX_STRING] = { 0 };
						if (CListWnd *cLWnd = (CListWnd *)FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositList"))
						{
							if (cLWnd->ItemsArray.GetLength() > 0)
							{
								for (int nDepositList = 0; nDepositList < cLWnd->ItemsArray.GetLength(); nDepositList++)
								{
									if (cLWnd->GetItemText(&cxstrItemName, nDepositList, 1))
									{
										GetCXStr(cxstrItemName.Ptr, szItemName, MAX_STRING);
										if (!_stricmp(szItemName, pItem->Name))
										{
											bLoreItem = true;  // A item with the same name is already deposited
										}
									}
								}
							}
						}
						if (CListWnd *cLWnd = (CListWnd *)FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_ItemList"))
						{
							if (cLWnd->ItemsArray.GetLength() > 0)
							{
								for (int nItemList = 0; nItemList < cLWnd->ItemsArray.GetLength(); nItemList++)
								{
									if (cLWnd->GetItemText(&cxstrItemName, nItemList, 1))
									{
										GetCXStr(cxstrItemName.Ptr, szItemName, MAX_STRING);
										if (!_stricmp(szItemName, pItem->Name))
										{
											bLoreItem = true;  // A item with the same name is already deposited
										}
									}
								}
							}
						}
					}
					if (!bLoreItem)
					{
						if (SlotsLeft > 0)
						{
							return true;
						}
						else
						{
							WriteChatf("%s:: Your guild bank ran out of space, closing guild bank window", PLUGIN_MSG);
							bEndThreads = true;
							return false;
						}
					}
					else
					{
						WriteChatf("%s:: %s is lore and you have one already in your guild bank, skipping depositing.", PLUGIN_MSG, pItem->Name);
						HideDoCommand(GetCharInfo()->pSpawn, "/autoinventory", true);
						Sleep(1000); // TODO figure out what to wait for so I don't have to hardcode this long of a delay
						return false;
					}
				}
				else // The guild bank window doesn't have a deposit button
				{
					WriteChatf("%s:: I can't find your deposit button on your guild bank window", PLUGIN_MSG);
					bEndThreads = true;
					return false;
				}
			}
			else // Guild Bank Window isn't open, bugging out of this thread
			{
				WriteChatf("%s:: Your guild bank window is closed for some reason", PLUGIN_MSG);
				bEndThreads = true;
				return false;
			}
		}
		else // The item isn't set to be deposited, moving onto the next item
		{
			return false;
		}
	}
	else // The item isn't in loot.ini file, moving onto the next item
	{
		return false;
	}
}

bool DepositItems(PITEMINFO pItem)  //This should only be run from inside threads
{
	if (!InGameOK() || bEndThreads)  // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
	{
		bEndThreads = true;
		return false;
	}
	sprintf_s(szCommand, "/nomodkey /itemnotify \"%s\" leftmouseup", pItem->Name);  // Picking up the item
	DoCommand(GetCharInfo()->pSpawn, szCommand);
	pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until I have an item in my cursor
	while (pluginclock::now() < WhileTimer) // While loop to wait for something to pop onto my cursor
	{
		Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
		if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
		{
			bEndThreads = true;
			return false;
		}
		if (WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))
		{
			if (CXWnd *pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositButton"))
			{
				if (pWndButton->Enabled)
				{
					break;
				}
			}
		}
	}
	if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
	{
		bEndThreads = true;
		return false;
	}
	if (CXWnd *pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositButton"))
	{
		if (!pWndButton->Enabled)
		{
			WriteChatf("%s:: Whoa the deposit button isn't ready, most likely you don't have the correct rights", PLUGIN_MSG);
			DoCommand(GetCharInfo()->pSpawn, "/autoinventory");
			bEndThreads = true;
			return false;
		}
		else
		{
			if (PCHARINFO2 pChar2 = GetCharInfo2())
			{
				if (pChar2->pInventoryArray)
				{
					if (PCONTENTS pItem = pChar2->pInventoryArray->Inventory.Cursor)
					{
						if (iSpamLootInfo)
						{
							WriteChatf("%s:: Putting \ag%s\ax into my guild bank", PLUGIN_MSG, pItem->Item2->Name);
						}
						SendWndClick2(pWndButton, "leftmouseup");
						Sleep(2000); // TODO figure out what to wait for so I don't have to hardcode this long of a delay
						return true;
					}
				}
			}
		}
	}
	WriteChatf("%s:: The function DepositItems failed, stopping depositing items", PLUGIN_MSG);
	bEndThreads = true;
	return false;  // Shit for some reason I got to this point, going to bail on depositing items
}

bool PutInGuildBank(PITEMINFO pItem)
{
	CXStr	cxstrItemName;
	CHAR	szItemName[MAX_STRING] = { 0 };
	if (WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))
	{
		if (CListWnd *cLWnd = (CListWnd *)FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositList"))
		{
			if (cLWnd->ItemsArray.GetLength() > 0)
			{
				for (int nDepositList = 0; nDepositList < cLWnd->ItemsArray.GetLength(); nDepositList++)
				{
					if (cLWnd->GetItemText(&cxstrItemName, nDepositList, 1))
					{
						GetCXStr(cxstrItemName.Ptr, szItemName, MAX_STRING);
						if (!_stricmp(szItemName, pItem->Name))
						{
							if (cLWnd->GetCurSel() != nDepositList)
							{
								SendListSelect("GuildBankWnd", "GBANK_DepositList", nDepositList);
							}
							pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until you select the right item
							while (pluginclock::now() < WhileTimer) // While loop to wait till you select the right item
							{
								if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
								{
									bEndThreads = true;
									return false;
								}
								if (cLWnd->GetCurSel() == nDepositList)
								{
									break;
								}
								Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
							}
							if (cLWnd->GetCurSel() != nDepositList)
							{
								bEndThreads = true;  // Lets end this thread we weren't able to select the right item
							}
							WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until your promote button is enabled
							while (pluginclock::now() < WhileTimer) // Will wait up to 10 seconds or until your promote button is enabled
							{
								if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
								{
									bEndThreads = true;
									return false;
								}
								if (WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))
								{
									if (CXWnd *pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PromoteButton"))
									{
										if (pWndButton->Enabled)
										{
											break;
										}
									}
								}
								Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
							}
							if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
							{
								bEndThreads = true;
								return false;
							}
							if (CXWnd *pWnd = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_BankCountLabel"))
							{
								CHAR szBankCountText[MAX_STRING] = { 0 };
								GetCXStr(pWnd->WindowText, szBankCountText, MAX_STRING);
								CHAR *pParsedToken = NULL;
								CHAR *pParsedValue = strtok_s(szBankCountText, ":", &pParsedToken);
								pParsedValue = strtok_s(NULL, ":", &pParsedToken);
								DWORD SlotsLeft = atoi(pParsedValue);
								if (SlotsLeft > 0)
								{
									if (CXWnd *pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PromoteButton"))
									{
										if (pWndButton->Enabled)
										{
											SendWndClick2(pWndButton, "leftmouseup");
											Sleep(2000); // TODO figure out what to wait for so I don't have to hardcode this long of a delay
											return true;
										}
										else
										{
											WriteChatf("%s:: Whoa the promote button isn't ready, most likely you don't have the correct rights", PLUGIN_MSG);
											bEndThreads = true;
											return false;
										}
									}
									else
									{
										WriteChatf("%s:: Whoa I can't find the promote button", PLUGIN_MSG);
										bEndThreads = true;
										return false;
									}
								}
								else // You ran out of space to promote items into your guild bank
								{
									return false;
								}

							}
						}
					}
				}
			}
		}
		else
		{
			WriteChatf("%s:: Your guild bank doesn't have a deposit list", PLUGIN_MSG);
			bEndThreads = true;
			return false;
		}
	}
	else
	{
		WriteChatf("%s:: You don't have your guild bank window open anymore", PLUGIN_MSG);
		bEndThreads = true;
		return false;
	}
	return false;
}

void SetItemPermissions(PITEMINFO pItem)
{
	CXStr	cxstrItemName;
	CHAR	szItemName[MAX_STRING] = { 0 };
	CXStr	cxstrItemPermission;
	CHAR	szItemPermission[MAX_STRING] = { 0 };
	if (WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))
	{
		if (CListWnd *cLWnd = (CListWnd *)FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_ItemList"))
		{
			if (cLWnd->ItemsArray.GetLength() > 0)
			{
				for (int nItemList = 0; nItemList < cLWnd->ItemsArray.GetLength(); nItemList++)
				{
					if (cLWnd->GetItemText(&cxstrItemName, nItemList, 1))
					{
						GetCXStr(cxstrItemName.Ptr, szItemName, MAX_STRING);
						if (!_stricmp(szItemName, pItem->Name))
						{
							if (cLWnd->GetItemText(&cxstrItemPermission, nItemList, 3))
							{
								GetCXStr(cxstrItemPermission.Ptr, szItemPermission, MAX_STRING);
								if (_stricmp(szItemPermission, szGuildItemPermission)) // If permissions don't match lets fix that
								{
									if (cLWnd->GetCurSel() != nItemList)
									{
										SendListSelect("GuildBankWnd", "GBANK_DepositList", nItemList);
									}
									pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until you select the right item
									while (pluginclock::now() < WhileTimer) // While loop to wait till you select the right item
									{
										if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
										{
											bEndThreads = true;
											return;
										}
										if (cLWnd->GetCurSel() == nItemList)
										{
											break;
										}
										Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
									}
									if (cLWnd->GetCurSel() != nItemList)
									{
										bEndThreads = true;  // Lets end this thread we weren't able to select the right item
									}
									WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until your permission button is enabled
									while (pluginclock::now() < WhileTimer) // Will wait up to 10 seconds or until your permission button is enabled
									{
										if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
										{
											bEndThreads = true;
											return;
										}
										if (WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))
										{
											if (CXWnd *pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PermissionCombo"))
											{
												if (pWndButton->Enabled)
												{
													break; // TODO see if this works
												}
											}
										}
										Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
									}
									if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
									{
										bEndThreads = true;
										return;
									}
									if (CXWnd *pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PermissionCombo"))
									{
										if (pWndButton->Enabled)
										{
											if (!_stricmp(szGuildItemPermission, "View Only"))
											{
												SendComboSelect("GuildBankWnd", "GBANK_PermissionCombo", 0);
											} 
											else if (!_stricmp(szGuildItemPermission, "Public If Usable"))
											{
												SendComboSelect("GuildBankWnd", "GBANK_PermissionCombo", 2);
											}
											else if (!_stricmp(szGuildItemPermission, "Public"))
											{
												SendComboSelect("GuildBankWnd", "GBANK_PermissionCombo", 3);
											}
											Sleep(100); // TODO figure out a more elegant way of doing this
											SendWndClick2(pWndButton, "leftmouseup");
											Sleep(2000); // TODO figure out a more elegant way of doing this
											return;
										}
										else
										{
											WriteChatf("%s:: Whoa the permission button isn't ready, most likely you don't have the correct rights", PLUGIN_MSG);
											bEndThreads = true;
											return;
										}
									}
									else
									{
										WriteChatf("%s:: Whoa I can't find the permission button", PLUGIN_MSG);
										bEndThreads = true;
										return;
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
			WriteChatf("%s:: Your guild bank doesn't have a deposit list", PLUGIN_MSG);
			bEndThreads = true;
			return;
		}
	}
	else
	{
		WriteChatf("%s:: You don't have your guild bank window open anymore", PLUGIN_MSG);
		bEndThreads = true;
		return;
	}
	return;
}

void BarterSearch(int nBarterItems, CHAR* pszItemName, DWORD MyBarterMinimum, CListWnd *cListInvWnd)
{
	if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
	{
		return;
	}
	pluginclock::time_point	WhileTimer = pluginclock::now();
	if (WinState((CXWnd*)FindMQ2Window("BarterSearchWnd")))  // Barter window is open
	{
		WriteChatfSafe("%s:: For entry %i, the item's name is %s and I will sell for: %d platinum", PLUGIN_MSG, nBarterItems + 1, pszItemName, MyBarterMinimum);
		sprintf_s(szCommand, "/nomodkey /notify BarterSearchWnd BTRSRCH_InventoryList listselect %i", nBarterItems + 1);
		HideDoCommand(GetCharInfo()->pSpawn, szCommand,true);
		WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds till we've targeted the correct item
		while (pluginclock::now() < WhileTimer) // While loop to wait till we've targeted the correct item
		{
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
			if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				return;
			}
			if (cListInvWnd)
			{
				if (cListInvWnd->GetCurSel() == nBarterItems) // not sure what this means, but it seems to be -1 when it has something selected
				{
					if (CXWnd *pWndButton = FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_SearchButton"))
					{
						if (pWndButton->Enabled)
						{
							SendWndClick2(pWndButton, "leftmouseup");
							WhileTimer = pluginclock::now();
						}
					}
				}
			}
		}
		WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds for barter search to finish
		while (pluginclock::now() < WhileTimer) // While loop to wait till barter search is done
		{
			if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				return;
			}
			CXStr	cxstrItemName;
			CHAR	szItemName[MAX_STRING] = { 0 };
			if (CListWnd *cBuyLineListWnd = (CListWnd *)FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_BuyLineList"))
			{
				if (cBuyLineListWnd->ItemsArray.GetLength() > 0)
				{
					if (cBuyLineListWnd->GetItemText(&cxstrItemName, 0, 1))
					{
						GetCXStr(cxstrItemName.Ptr, szItemName, MAX_STRING);
						if (!_stricmp(szItemName, pszItemName))
						{
							WhileTimer = pluginclock::now();
						}
					}
				}
			}
			if (CXWnd *pWndButton = FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_SearchButton"))
			{
				if (pWndButton->Enabled)
				{
					WhileTimer = pluginclock::now();
				}
			}
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
		}
	}
}

int FindBarterIndex(DWORD MyBarterMinimum, CListWnd *cBuyLineListWnd)
{
	CXStr	cxstrItemPrice;
	CHAR	szItemPrice[MAX_STRING] = { 0 };
	DWORD	BarterPrice = 0;
	DWORD	BarterMaximumPrice = 0;
	int		BarterMaximumIndex = 0;
	for (int nBarterItems = 0; nBarterItems < cBuyLineListWnd->ItemsArray.GetLength(); nBarterItems++)
	{
		if (cBuyLineListWnd->GetItemText(&cxstrItemPrice, nBarterItems, 3))
		{
			GetCXStr(cxstrItemPrice.Ptr, szItemPrice, MAX_STRING);
			char* pch;
			if (pch = strstr(szItemPrice, "p"))
			{
				strcpy_s(pch, 1, "\0");
				puts(szItemPrice);
				if (IsNumber(szItemPrice))
				{
					BarterPrice = atoi(szItemPrice);
					if (BarterPrice > BarterMaximumPrice)
					{
						BarterMaximumPrice = BarterPrice;
						BarterMaximumIndex = nBarterItems;
					}
				}
			}
		}
	}
	if (BarterMaximumPrice < MyBarterMinimum || BarterMaximumPrice < (DWORD)iBarMinSellPrice)
	{
		return -1; // the maximum price is less then my minimum price i am moving to the next item
	}
	return BarterMaximumIndex;
}

bool SelectBarterSell(int BarterMaximumIndex, CListWnd *cBuyLineListWnd)
{
	if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
	{
		return false;
	}
	pluginclock::time_point	WhileTimer = pluginclock::now();
	if (cBuyLineListWnd->GetCurSel() != BarterMaximumIndex)
	{
		cBuyLineListWnd->SetCurSel(BarterMaximumIndex);
		WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds till we've targeted the correct item
		while (pluginclock::now() < WhileTimer) // While loop to wait till we've targeted the correct item
		{
			if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				return false;
			}
			if (cBuyLineListWnd->GetCurSel() == BarterMaximumIndex) // not sure what this means, but it seems to be -1 when it has something selected
			{
				WhileTimer = pluginclock::now();
			}
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
		}
	}
	if (cBuyLineListWnd->GetCurSel() == BarterMaximumIndex)
	{
		sprintf_s(szCommand, "/nomodkey /notify BarterSearchWnd SellButton leftmouseup");
		HideDoCommand(GetCharInfo()->pSpawn, szCommand, true);
		WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds till we've targeted the correct item
		while (pluginclock::now() < WhileTimer) // While loop to wait till we've targeted the correct item
		{
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
			if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				return false;
			}
			if (WinState((CXWnd*)pQuantityWnd)) // Lets wait till the Quantity Window pops up
			{
				Sleep(100); 
				return true;
			}
		}
	}
	return false;
}

void SelectBarterQuantity(int BarterMaximumIndex, CHAR* pszItemName, CListWnd *cBuyLineListWnd)
{
	if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot barter we want to bug out
	{
		return;
	}
	CXStr	cxstrItemCount;
	CHAR	szItemCount[MAX_STRING] = { 0 };
	DWORD	BarterCount;
	DWORD	MyCount;
	DWORD	SellCount;
	pluginclock::time_point	WhileTimer = pluginclock::now();
	if (WinState((CXWnd*)pQuantityWnd))
	{
		if (cBuyLineListWnd->GetItemText(&cxstrItemCount, BarterMaximumIndex, 2))
		{
			GetCXStr(cxstrItemCount.Ptr, szItemCount, MAX_STRING);
			BarterCount = atoi(szItemCount);
		}

		MyCount = FindItemCountByName(pszItemName, true);
		if (MyCount == 0)
		{
			return;
		}
		if (MyCount >= 20 && BarterCount >= 20)
		{
			SellCount = 20;
		}
		else if (MyCount >= BarterCount)
		{
			SellCount = BarterCount;
		}
		else
		{
			SellCount = MyCount;
		}
		sprintf_s(szCommand, "/nomodkey /notify QuantityWnd QTYW_slider newvalue %i", SellCount);
		HideDoCommand(GetCharInfo()->pSpawn, szCommand, true);
		WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds till we've targeted the correct item
		while (pluginclock::now() < WhileTimer) // While loop to wait till we've targeted the correct item
		{
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
			if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				return;
			}
			if (WinState((CXWnd*)pQuantityWnd))
			{
				if (CXWnd *pWndSliderInput = (CXWnd *)pQuantityWnd->GetChildItem("QTYW_SliderInput"))
				{
					CXStr	cxstrInputCount;
					CHAR	szInputCount[MAX_STRING] = { 0 };
					DWORD	InputCount;
					GetCXStr(pWndSliderInput->GetWindowTextA().Ptr, szInputCount, MAX_STRING);
					InputCount = atoi(szInputCount);
					if (InputCount == SellCount)
					{
						WhileTimer = pluginclock::now();
					}
				}
			}
		}
		if (WinState((CXWnd*)pQuantityWnd))
		{
			if (CXWnd *pWndButton = pQuantityWnd->GetChildItem("QTYW_Accept_Button"))
			{
				bBarterItemSold = false;
				SendWndClick2(pWndButton, "leftmouseup");
				WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds till we've targeted the correct item
				while (pluginclock::now() < WhileTimer) // While loop to wait till we've targeted the correct item
				{
					if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
					{
						return;
					}
					if (bBarterItemSold)
					{
						return;
					}
					Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
				}
				return;
			}
		}
	}
}

DWORD __stdcall SellItems(PVOID pData)
{
	if (!InGameOK()) 
	{ 
		return 0; 
	}
	bSellActive = true;
	bEndThreads = false;
	CHAR INISection[MAX_STRING] = { 0 };
	CHAR Value[MAX_STRING] = { 0 };
	HideDoCommand(GetCharInfo()->pSpawn, "/keypress OPEN_INV_BAGS", true);
	Sleep(500); // TODO make a smarter check of when your bags are all open
	if (!WinState((CXWnd*)pMerchantWnd))  // I don't have the merchant window open
	{
		if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
		{
			if (psTarget->mActorClient.Class != MERCHANT_CLASS)
			{
				WriteChatfSafe("%s:: Please target a merchant!", PLUGIN_MSG);
				ResetItemActions();
				return 0;
			}
			if (MoveToNPC(psTarget))
			{
				if (OpenWindow(psTarget))
				{
					LootThreadTimer = pluginclock::now() + std::chrono::seconds(30); // Will wait up to 30 seconds or until I get the merchant is populated message
					while (pluginclock::now() < LootThreadTimer) // While loop to wait for something to pop onto my cursor
					{
						if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
						{
							WriteChatfSafe("%s:: You aren't in the proper game state or you sent another /autoloot [buy|sell|deposit|barter] command", PLUGIN_MSG);
							ResetItemActions();
							return 0;
						}
						Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
					}
				}
				else
				{
					WriteChatfSafe("%s:: You failed to open the merchant window!!", PLUGIN_MSG);
					ResetItemActions();
					return 0;
				}
			}
			else
			{
				WriteChatfSafe("%s:: You failed to get within range of the merchant!!", PLUGIN_MSG); // This shouldn't fail since it was checked before you entered this thread
				ResetItemActions();
				return 0;
			}
		}
		else
		{
			WriteChatfSafe("%s:: Please target a merchant!", PLUGIN_MSG); // This shouldn't fail since it was checked before you entered this thread
			ResetItemActions();
			return 0;
		}
	}
	if (WinState((CXWnd*)pMerchantWnd))  // merchant window is open
	{
		for (unsigned long nSlot = BAG_SLOT_START; nSlot < NUM_INV_SLOTS; nSlot++) 	// Looping through the items in my inventory and seening if I want to sell them
		{
			if (PCHARINFO2 pChar2 = GetCharInfo2())
			{
				if (pChar2->pInventoryArray && pChar2->pInventoryArray->InventoryArray) //check my inventory slots
				{
					if (PCONTENTS pItem = pChar2->pInventoryArray->InventoryArray[nSlot])
					{
						if (PITEMINFO theitem = GetItemFromContents(pItem))
						{
							if (theitem->Type != ITEMTYPE_PACK)  //It isn't a pack! we should sell it
							{
								sprintf_s(INISection, "%c", theitem->Name[0]);
								if (GetPrivateProfileString(INISection, theitem->Name, 0, Value, MAX_STRING, szLootINI) != 0)
								{
									CHAR *pParsedToken = NULL;
									CHAR *pParsedValue = strtok_s(Value, "|", &pParsedToken);
									if (!_stricmp(pParsedValue, "Sell"))
									{
										if (theitem->NoDrop &&  theitem->Cost > 0)
										{
											WriteChatfSafe("%s:: Attempting to sell \ag%s\ax.", PLUGIN_MSG, theitem->Name);
											SellItem(pItem);
										}
										else if (!theitem->NoDrop)
										{
											WriteChatfSafe("%s:: You attempted to sell \ag%s\ax, but it is no drop.  Setting to Keep in your loot ini file.", PLUGIN_MSG, theitem->Name);
											WritePrivateProfileString(INISection, theitem->Name, "Keep", szLootINI);
										}
										else if (theitem->Cost == 0)
										{
											WriteChatfSafe("%s:: You attempted to sell \ag%s\ax, but it is worth nothing.  Setting to Keep in your loot ini file.", PLUGIN_MSG, theitem->Name);
											WritePrivateProfileString(INISection, theitem->Name, "Keep", szLootINI);
										}
									}
									if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pMerchantWnd))
									{
										ResetItemActions();
										return 0;
									}
								}
							}
						}
					}
				}
			}
		}
		for (unsigned long nPack = 0; nPack < 10; nPack++)
		{
			if (PCHARINFO2 pChar2 = GetCharInfo2())
			{
				if (pChar2->pInventoryArray) //Checking my bags
				{
					if (PCONTENTS pPack = pChar2->pInventoryArray->Inventory.Pack[nPack])
					{
						if (PITEMINFO pItemPack = GetItemFromContents(pPack))
						{
							if (pItemPack->Type == ITEMTYPE_PACK && pPack->Contents.ContainedItems.pItems)
							{
								for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
								{
									if (pPack && pPack->Contents.ContainedItems.pItems)
									{
										if (PCONTENTS pItem = pPack->Contents.ContainedItems.pItems->Item[nItem])
										{
											if (PITEMINFO theitem = GetItemFromContents(pItem))
											{
												sprintf_s(INISection, "%c", theitem->Name[0]);
												if (GetPrivateProfileString(INISection, theitem->Name, 0, Value, MAX_STRING, szLootINI) != 0)
												{
													CHAR *pParsedToken = NULL;
													CHAR *pParsedValue = strtok_s(Value, "|", &pParsedToken);
													if (!_stricmp(pParsedValue, "Sell"))
													{
														if (theitem->NoDrop &&  theitem->Cost > 0)
														{
															WriteChatfSafe("%s:: Attempting to sell \ag%s\ax.", PLUGIN_MSG, theitem->Name);
															SellItem(pItem);
														}
														else if (!theitem->NoDrop)
														{
															WriteChatfSafe("%s:: Attempted to sell \ag%s\ax, but it is no drop.  Setting to Keep in your loot ini file.", PLUGIN_MSG, theitem->Name);
															WritePrivateProfileString(INISection, theitem->Name, "Keep", szLootINI);
														}
														else if (theitem->Cost == 0)
														{
															WriteChatfSafe("%s:: Attempted to sell \ag%s\ax, but it is worth nothing.  Setting to Keep in your loot ini file.", PLUGIN_MSG, theitem->Name);
															WritePrivateProfileString(INISection, theitem->Name, "Keep", szLootINI);
														}
													}
													if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pMerchantWnd))
													{
														ResetItemActions();
														return 0;
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
	ResetItemActions();
	return 0;
}

DWORD __stdcall BuyItem(PVOID pData)
{
	if (!InGameOK()) return 0;
	bBuyActive = true;
	bEndThreads = false;
	int  iItemCount;
	CHAR szCount[MAX_STRING];
	int  iMaxItemCount;
	pluginclock::time_point	WhileTimer;
	CHAR szTemp1[MAX_STRING];
	CHAR szItemToBuy[MAX_STRING];
	CXStr	cxstrItemName;
	CHAR	szItemName[MAX_STRING] = { 0 };
	CXStr	cxstrItemCount;
	CHAR	szItemCount[MAX_STRING] = { 0 };
	CHAR	szBuyItemCount[MAX_STRING] = { 0 };
	sprintf_s(szTemp1, "%s", (PCHAR)pData);
	CHAR *pParsedToken = NULL;
	CHAR *pParsedValue = strtok_s(szTemp1, "|", &pParsedToken);
	sprintf_s(szItemToBuy, "%s", pParsedValue);
	pParsedValue = strtok_s(NULL, "|", &pParsedToken);
	if (!WinState((CXWnd*)pMerchantWnd))  // I don't have the guild bank window open
	{
		if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
		{
			if (psTarget->mActorClient.Class != MERCHANT_CLASS)
			{
				WriteChatfSafe("%s:: Please target a merchant!", PLUGIN_MSG);
				ResetItemActions();
				return 0;
			}
			if (MoveToNPC(psTarget))
			{
				if (OpenWindow(psTarget))
				{
					LootThreadTimer = pluginclock::now() + std::chrono::seconds(30); // Will wait up to 30 seconds or until I get the merchant is populated message
					while (pluginclock::now() < LootThreadTimer) // While loop to wait for something to pop onto my cursor
					{
						if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
						{
							WriteChatfSafe("%s:: You aren't in the proper game state or you sent another /autoloot [buy|sell|deposit|barter] command", PLUGIN_MSG);
							ResetItemActions();
							return 0;
						}
						Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
					}
				}
				else
				{
					WriteChatfSafe("%s:: You failed to open the merchant window!!", PLUGIN_MSG);
					ResetItemActions();
					return 0;
				}
			}
			else
			{
				WriteChatfSafe("%s:: You failed to get within range of the merchant!!", PLUGIN_MSG); // This shouldn't fail since it was checked before you entered this thread
				ResetItemActions();
				return 0;
			}
		}
		else
		{
			WriteChatfSafe("%s:: Please target a merchant!", PLUGIN_MSG); // This shouldn't fail since it was checked before you entered this thread
			ResetItemActions();
			return 0;
		}
	}
	if (WinState((CXWnd*)pMerchantWnd))  // merchant window are open
	{
		if (IsNumber(pParsedValue))
		{
			iItemCount = atoi(pParsedValue);
			if (CListWnd *cLWnd = (CListWnd *)FindMQ2Window("MerchantWnd")->GetChildItem("MW_ItemList"))
			{
				if (cLWnd->ItemsArray.GetLength() > 0)
				{
					for (int nMerchantItems = 0; nMerchantItems < cLWnd->ItemsArray.GetLength(); nMerchantItems++)
					{
						if (cLWnd->GetItemText(&cxstrItemName, nMerchantItems, 1))
						{
							GetCXStr(cxstrItemName.Ptr, szItemName, MAX_STRING);
							if (!_stricmp(szItemToBuy, szItemName)) // Ok we found the right item
							{
								if (cLWnd->GetItemText(&cxstrItemCount, nMerchantItems, 2)) // Ok we need to figure out how many are available to buy
								{
									GetCXStr(cxstrItemCount.Ptr, szItemCount, MAX_STRING);
									if (_stricmp(szItemCount, "--")) // If this is true there is only a set ammount to buy
									{
										iMaxItemCount = atoi(szItemCount); // Get the maximum ammount available
										if (iItemCount > iMaxItemCount)
										{
											iItemCount = iMaxItemCount;
										}
									}
									while (iItemCount > 0) // Ok so lets loop through till we get the ammount of items we wanted to buy
									{
										if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pMerchantWnd))
										{
											ResetItemActions();
											return 0;
										}
										if (cLWnd->GetCurSel() != nMerchantItems)
										{
											SendListSelect("MerchantWnd", "MW_ItemList", nMerchantItems); // If we haven't selected the right item, send the cursor there
										}
										WhileTimer = pluginclock::now() + std::chrono::seconds(30);
										while (pluginclock::now() < WhileTimer)  // Will wait up to 30 seconds for the barter window
										{
											if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pMerchantWnd))
											{
												ResetItemActions();
												return 0;
											}
											if (cLWnd->GetCurSel() == nMerchantItems)
											{
												WhileTimer = pluginclock::now();
											}
											Sleep(100);
										}
										if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pMerchantWnd) || cLWnd->GetCurSel() != nMerchantItems)
										{
											ResetItemActions();
											return 0;
										}
										DWORD iStartCount = FindItemCount(szItemToBuy);
										if (PCHARINFO pChar = GetCharInfo())
										{
											sprintf_s(szCount, "%d", iItemCount);
											BuyItem(pChar->pSpawn, szCount); // This is the command from MQ2Commands.cpp
											WhileTimer = pluginclock::now() + std::chrono::seconds(30);
											while (pluginclock::now() < WhileTimer)  // Will wait up to 30 seconds for the number of items to increase in your inventory
											{
												if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pMerchantWnd))
												{
													ResetItemActions();
													return 0;
												}
												if (FindItemCount(szItemToBuy) > iStartCount)
												{
													WhileTimer = pluginclock::now();
													iItemCount = iItemCount - FindItemCount(szItemToBuy) + iStartCount;
													WriteChatfSafe("%s:: \ag%d\ax left to buy of \ag%s\ax!", PLUGIN_MSG, iItemCount, szItemToBuy);
												}
												Sleep(100);
											}
											Sleep(1000);
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
	else
	{
		WriteChatfSafe("%s:: Please open a merchant window first before attempting to buy: \ag%s\ax", PLUGIN_MSG, szItemToBuy);
	}
	ResetItemActions();
	return 0;
}

DWORD __stdcall DepositPersonalBanker(PVOID pData)
{
	if (!InGameOK()) return 0;
	bEndThreads = false;
	bDepositActive = true;
	PCHARINFO pChar = GetCharInfo();
	PCHARINFO2 pChar2 = GetCharInfo2();
	CHAR INISection[MAX_STRING] = { 0 };
	CHAR Value[MAX_STRING] = { 0 };
	if (!WinState((CXWnd*)pBankWnd))  // I don't have the guild bank window open
	{
		if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
		{
			if (psTarget->mActorClient.Class != PERSONALBANKER_CLASS)
			{
				WriteChatfSafe("%s:: Please target a personal banker!", PLUGIN_MSG);
				ResetItemActions();
				return 0;
			}
			if (MoveToNPC(psTarget))
			{
				LootThreadTimer = pluginclock::now() + std::chrono::seconds(30);
				if (OpenWindow(psTarget))
				{
					while (pluginclock::now() < LootThreadTimer) // Will wait up to 30 seconds or until I get the banker is open for business
					{
						if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
						{
							WriteChatfSafe("%s:: You aren't in the proper game state or you sent another /autoloot [buy|sell|deposit|barter] command", PLUGIN_MSG);
							ResetItemActions();
							return 0;
						}
						Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
					}
				}
				else
				{
					WriteChatfSafe("%s:: You failed to open your personal bank window!!", PLUGIN_MSG);
					ResetItemActions();
					return 0;
				}
			}
			else
			{
				WriteChatfSafe("%s:: You failed to get within range of your personal banker!!", PLUGIN_MSG);
				ResetItemActions();
				return 0;
			}
		}
		else
		{
			WriteChatfSafe("%s:: Please target a personal banker!", PLUGIN_MSG); // This shouldn't fail since it was checked before you entered this thread
			ResetItemActions();
			return 0;
		}
	}
	if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pBankWnd))  // If we aren't in a proper game state, or if they sent /autoloot deposit again we want to bug out
	{
		ResetItemActions();
		return 0;
	}
	HideDoCommand(GetCharInfo()->pSpawn, "/keypress OPEN_INV_BAGS",true); // TODO check if this is necessary
	Sleep(500); // TODO lets make a smart function that checks whether your bags are open
	if (pChar2 && pChar2->pInventoryArray && pChar2->pInventoryArray->InventoryArray) //check my inventory slots
	{
		for (unsigned long nSlot = BAG_SLOT_START; nSlot < NUM_INV_SLOTS; nSlot++) // loop through my inventory
		{
			if (PCONTENTS pItem = pChar2->pInventoryArray->InventoryArray[nSlot])
			{
				if (PITEMINFO theitem = GetItemFromContents(pItem))
				{
					if (FitInPersonalBank(theitem))
					{
						PutInPersonalBank(theitem);
					}
					if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pBankWnd))
					{
						ResetItemActions();
						return 0;
					}
				}
			}
		}
	}
	if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pBankWnd))  // This shouldn't be necessary
	{
		ResetItemActions();
		return 0;
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
									if (FitInPersonalBank(theitem))
									{
										PutInPersonalBank(theitem);
									}
									if (!InGameOK() || bEndThreads || !WinState((CXWnd*)pBankWnd))
									{
										ResetItemActions();
										return 0;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	ResetItemActions();
	return 0;
}

DWORD __stdcall DepositGuildBanker(PVOID pData)
{
	if (!InGameOK()) return 0;
	bEndThreads = false;
	bDepositActive = true;
	PCHARINFO pChar = GetCharInfo();
	PCHARINFO2 pChar2 = GetCharInfo2();
	CHAR INISection[MAX_STRING] = { 0 };
	CHAR Value[MAX_STRING] = { 0 };
	if (!WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))  // I don't have the guild bank window open
	{
		if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
		{
			if (psTarget->mActorClient.Class != GUILDBANKER_CLASS)
			{
				WriteChatfSafe("%s:: Please target a guild banker!", PLUGIN_MSG);
				ResetItemActions();
				return 0;
			}
			if (MoveToNPC(psTarget))
			{
				if (OpenWindow(psTarget))
				{
					LootThreadTimer = pluginclock::now() + std::chrono::seconds(30);
					while (pluginclock::now() < LootThreadTimer) // Will wait up to 30 seconds or until I get the banker is open for business
					{
						if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
						{
							WriteChatfSafe("%s:: You aren't in the proper game state or you sent another /autoloot [buy|sell|deposit|barter] command");
							ResetItemActions();
							return 0;
						}
						Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
					}
				}
				else
				{
					WriteChatfSafe("%s:: You failed to open your guild bank window!!", PLUGIN_MSG); 
					ResetItemActions();
					return 0;
				}
			}
			else
			{
				WriteChatfSafe("%s:: You failed to get within range of your guild banker!!", PLUGIN_MSG);
				ResetItemActions();
				return 0;
			}
		}
		else
		{
			WriteChatfSafe("%s:: Please target a guild banker!", PLUGIN_MSG); // This shouldn't fail since it was checked before you entered this thread
			ResetItemActions();
			return 0;
		}
	}
	if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))  // If we aren't in a proper game state, or if they sent /autoloot deposit again we want to bug out
	{
		ResetItemActions();
		return 0;
	}
	HideDoCommand(GetCharInfo()->pSpawn, "/keypress OPEN_INV_BAGS",true); // TODO check if this is necessary
	Sleep(500);
	if (pChar2 && pChar2->pInventoryArray && pChar2->pInventoryArray->InventoryArray) //check my inventory slots
	{
		for (unsigned long nSlot = BAG_SLOT_START; nSlot < NUM_INV_SLOTS; nSlot++) // loop through my inventory
		{
			if (PCONTENTS pItem = pChar2->pInventoryArray->InventoryArray[nSlot])
			{
				if (PITEMINFO theitem = GetItemFromContents(pItem))
				{
					if (CheckGuildBank(theitem))
					{
						if (DepositItems(theitem))
						{
							if (PutInGuildBank(theitem))
							{
								SetItemPermissions(theitem);
							}
						}
					}
					if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))
					{
						ResetItemActions();
						return 0;
					}
				}
			}
		}
	}
	if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))  // This shouldn't be necessary
	{
		ResetItemActions();
		return 0;
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
									if (CheckGuildBank(theitem))
									{
										if (DepositItems(theitem))
										{
											if (PutInGuildBank(theitem))
											{
												SetItemPermissions(theitem);
											}
										}
									}
									if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("GuildBankWnd")))
									{
										ResetItemActions();
										return 0;
									}
								}
							}
						}
					}
				}
			}
		}
	}	
	ResetItemActions();
	return 0;
}

DWORD __stdcall BarterItems(PVOID pData)
{
	if (!InGameOK()) 
	{ 
		return 0; 
	}
	if (!HasExpansion(EXPANSION_RoF))
	{
		WriteChatfSafe("%s:: You need to have Rain of Fear expansion to use the barter functionality.", PLUGIN_MSG);
		return 0;
	}
	bBarterActive = true;
	bEndThreads = false;
	PCHARINFO pChar = GetCharInfo();
	PCHARINFO2 pChar2 = GetCharInfo2();
	CHAR INISection[MAX_STRING] = { 0 };
	CHAR Value[MAX_STRING] = { 0 };
	pluginclock::time_point	WhileTimer = pluginclock::now() + std::chrono::seconds(10);
	if (!WinState((CXWnd*)FindMQ2Window("BarterSearchWnd")))  // Barter window isn't open
	{
		WriteChatfSafe("%s:: Opening a barter window!", PLUGIN_MSG);
		HideDoCommand(GetCharInfo()->pSpawn, "/barter",true);
		WhileTimer = pluginclock::now() + std::chrono::seconds(30);
		while (pluginclock::now() < WhileTimer)  // Will wait up to 30 seconds for the barter window
		{
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
			if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
			{
				ResetItemActions();
				return 0;
			}
			if (WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) 
			{ 
				WhileTimer = pluginclock::now(); 
			}
		}
	}
	if (WinState((CXWnd*)FindMQ2Window("BarterSearchWnd")))  // Barter window is open
	{
		if (CListWnd *cListInvWnd = (CListWnd *)FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_InventoryList"))
		{
			for (int nBarterItems = 0; nBarterItems < cListInvWnd->ItemsArray.GetLength(); nBarterItems++)
			{
				if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot barter we want to bug out
				{
					ResetItemActions();
					return 0;
				}
				CXStr	cxstrItemName;
				CHAR	szItemName[MAX_STRING] = { 0 };
				CHAR	INISection[MAX_STRING] = { 0 };
				CHAR	Value[MAX_STRING] = { 0 };
				DWORD	MyBarterMinimum;
				DWORD	BarterMaximumPrice = 0;
				int		BarterMaximumIndex = 0;
				if (cListInvWnd->GetItemText(&cxstrItemName, nBarterItems, 1))
				{
					GetCXStr(cxstrItemName.Ptr, szItemName, MAX_STRING);
					sprintf_s(INISection, "%c", szItemName[0]);
					bool IWant = false;  // Will be set true if you want and can accept the item
					bool IDoNotWant = false;  // Will be set true if you don't want or can't accept
					bool CheckIfOthersWant = false;  // Will be set true if I am ML and I can't accept or don't need
					if (GetPrivateProfileString(INISection, szItemName, 0, Value, MAX_STRING, szLootINI) != 0)
					{
						CHAR *pParsedToken = NULL;
						CHAR *pParsedValue = strtok_s(Value, "|", &pParsedToken);
						if (!_stricmp(pParsedValue, "Barter"))
						{
							pParsedValue = strtok_s(NULL, "|", &pParsedToken);
							if (pParsedValue)
							{
								if (IsNumber(pParsedValue))
								{
									MyBarterMinimum = atoi(pParsedValue);
								}
								BarterSearch(nBarterItems, szItemName, MyBarterMinimum, cListInvWnd);
								bool bBarterLoop = true;
								while (bBarterLoop)
								{
									if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot barter we want to bug out
									{
										ResetItemActions();
										return 0;
									}
									if (bBarterReset)
									{
										BarterSearch(nBarterItems, szItemName, MyBarterMinimum, cListInvWnd);
										if (!InGameOK() || bEndThreads || !WinState((CXWnd*)FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot barter we want to bug out
										{
											ResetItemActions();
											return 0;
										}
									}
									if (FindItemCountByName(szItemName, true) > 0)
									{
										if (CListWnd *cBuyLineListWnd = (CListWnd *)FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_BuyLineList"))
										{
											if (cBuyLineListWnd->ItemsArray.GetLength() > 0)
											{
												BarterMaximumIndex = FindBarterIndex(MyBarterMinimum, cBuyLineListWnd); // Will return -1 if no appropriate sellers are found
												if (BarterMaximumIndex > -1)
												{
													if (SelectBarterSell(BarterMaximumIndex, cBuyLineListWnd))
													{
														SelectBarterQuantity(BarterMaximumIndex, szItemName, cBuyLineListWnd);
													}
												}
												else
												{
													bBarterLoop = false;
												}
											}
											else
											{
												bBarterLoop = false;
											}
										}
										else
										{
											bBarterLoop = false;
										}
									}
									else
									{
										bBarterLoop = false;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	ResetItemActions();
	return 0;
}
#endif