#include <mq/Plugin.h>
#if !defined(ROF2EMU) && !defined(UFEMU)
#include "MQ2AutoLoot.h"
#include "ItemActions.h"
#include <chrono>

typedef std::chrono::high_resolution_clock pluginclock;

bool* pbStickOn; //MoveUtils 11.x
fEQCommand fStickCommand; //MoveUtils 11.x

bool                        bBarterActive = false;
bool                        bDepositActive = false;
bool                        bBuyActive = false;
bool                        bSellActive = false;
bool                        bBarterSearchDone = false; // Set to true when your search has finished
bool                        bBarterReset = false; // Set to true when you need to refresh your barter search
bool                        bBarterItemSold; // Set to true when you sell an item
bool                        bEndThreads = true;  // Set to true when you want to end any threads out here, also it is used to enforce that at most a single thread is active at one time
CONTENTS*                   pItemToPickUp = nullptr;
CXWnd*                      pWndLeftMouseUp = nullptr;
pluginclock::time_point     LootThreadTimer = pluginclock::now();

void ResetItemActions()
{
	if (WinState(pMerchantWnd))
	{
		HideDoCommand(GetCharInfo()->pSpawn, "/squelch /windowstate MerchantWnd close", true);
	}
	if (WinState(pBankWnd))
	{
		HideDoCommand(GetCharInfo()->pSpawn, "/squelch /windowstate BigBankWnd close", true);
	}
	if (WinState(FindMQ2Window("GuildBankWnd")))
	{
		HideDoCommand(GetCharInfo()->pSpawn, "/squelch /windowstate GuildBankWnd close", true);
	}
	if (WinState(FindMQ2Window("BarterSearchWnd")))
	{
		HideDoCommand(GetCharInfo()->pSpawn, "/squelch /windowstate BarterSearchWnd close", true);
	}
	if (bBarterActive)
	{
		WriteChatf("%s:: Finished bartering!!", PLUGIN_CHAT_MSG);
		bBarterActive = false;
	}
	if (bBuyActive)
	{
		WriteChatf("%s:: Finished buying!!", PLUGIN_CHAT_MSG);
		bBuyActive = false;
	}
	if (bDepositActive)
	{
		WriteChatf("%s:: Finished depositing stuff!!", PLUGIN_CHAT_MSG);
		bDepositActive = false;
	}
	if (bSellActive)
	{
		WriteChatf("%s:: Finished selling your stuff!!", PLUGIN_CHAT_MSG);
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
					WriteChatf("%s:: Moving towards: \ag%s\ax", PLUGIN_CHAT_MSG, pSpawn->DisplayedName);
				}
				else
				{
					WriteChatf("%s:: Hey friend, you don't have MQ2MoveUtils loaded.  Move to within 20 of the target before trying to sell or deposit", PLUGIN_CHAT_MSG);
					return false;
				}
				pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(30); // Will wait up to 30 seconds or until you are within 20 of the guild banker
				while (pluginclock::now() < WhileTimer) // While loop to wait for npc to aggro me while i'm casting
				{
					if (!InGameOK() || bEndThreads)
					{
						return false; // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
					}
					if (pSpawn->X && pSpawn->Y && pSpawn->Z)
					{
						Distance = Get3DDistance(pChar->pSpawn->X, pChar->pSpawn->Y, pChar->pSpawn->Z, pSpawn->X, pSpawn->Y, pSpawn->Z);
						if (Distance < 20)
						{
							break; // I am close enough to the banker I can stop moving towards them
						}
					}
					else
					{
						WriteChatf("%s:: For some reason I can't find the distance to my banker/merchant!", PLUGIN_CHAT_MSG);
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
					Distance = Get3DDistance(pChar->pSpawn->X, pChar->pSpawn->Y, pChar->pSpawn->Z, pSpawn->X, pSpawn->Y, pSpawn->Z);
					if (Distance < 20)
					{
						return true; // I am close enough to the banker/merchant to open their window
					}
					else
					{
						WriteChatf("%s:: I wasn't able to get to within 20 of my banker/merchant!", PLUGIN_CHAT_MSG);
						return false;
					}
				}
				else
				{
					WriteChatf("%s:: For some reason I can't find the distance to !", PLUGIN_CHAT_MSG);
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
			WriteChatf("%s:: For some reason I can't find the distance to your npc!", PLUGIN_CHAT_MSG);
			return false; // This shouldn't fail, but for some reason I wasn't able to get the location of the spawn I want to navigate to
		}
	}
	else
	{
		WriteChatf("%s:: For some reason GetCharInfo() failed!", PLUGIN_CHAT_MSG);
		return false; // This shouldn't fail, but for some reason I wasn't able to get the character's GetCharInfo()
	}
}

bool HandleMoveUtils()
{
	fStickCommand = (fEQCommand)GetPluginProc("mq2moveutils", "StickCommand");
	pbStickOn = (bool*)GetPluginProc("mq2moveutils", "bStickOn");

	return fStickCommand && pbStickOn;
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
				WriteChatf("%s:: Opening merchant window and waiting 30 seconds for the merchant window to populate", PLUGIN_CHAT_MSG);
			}
			if (pSpawn->mActorClient.Class == PERSONALBANKER_CLASS)
			{
				WriteChatf("%s:: Opening personal banking window and waiting 30 seconds for the personal banking window to populate", PLUGIN_CHAT_MSG);
			}
			if (pSpawn->mActorClient.Class == GUILDBANKER_CLASS)
			{
				WriteChatf("%s:: Opening guild banking window and waiting 30 seconds for the guild banking window to populate", PLUGIN_CHAT_MSG);
			}
			HideDoCommand(GetCharInfo()->pSpawn, "/face nolook",true);
			HideDoCommand(GetCharInfo()->pSpawn, "/nomodkey /click right target",true);
			pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(30); // Will wait up to 30 seconds or until the window is open
			while (pluginclock::now() < WhileTimer) // While loop to wait for something to pop onto my cursor
			{
				Sleep(100);  // Sleep for 100 ms and lets check if the window is open
				if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
				{
					bEndThreads = true;
					return false;
				}
				if (WinState(FindMQ2Window("GuildBankWnd")) || WinState(pBankWnd) || WinState(pMerchantWnd))
				{
					return true;
				}
			}
			return false;
		}
		else
		{
			WriteChatf("%s:: Your target has changed!", PLUGIN_CHAT_MSG);
			bEndThreads = true;
			return false;
		}
	}
	else
	{
		WriteChatf("%s:: You don't have a target!", PLUGIN_CHAT_MSG);
		bEndThreads = true;
		return false;
	}
}

bool CheckIfItemIsInSlot(short InvSlot, short BagSlot)
{
	if (CONTENTS* pTempItem = FindItemBySlot(InvSlot, BagSlot, eItemContainerPossessions))
	{
		return true; // shit still there
	}
	else
	{
		return false; // the item is gone
	}
}

bool WaitForItemToBeSelected(CONTENTS* pItem, short InvSlot, short BagSlot)
{
	Sleep(750);  // Lets see if 750 ms is sufficient of a delay, TODO figure out a smarter way to determine when a merchant is ready to buy an item
	pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(30);
	while (pluginclock::now() < WhileTimer) // Will wait up to 30 seconds or until I get the merchant buys the item
	{
		if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
		{
			return false;
		}

		if (!CheckIfItemIsInSlot(InvSlot, BagSlot)) // There is no item in
		{
			return false;
		}
		if (pMerchantWnd->IsVisible())
		{
			if (pMerchantWnd->pSelectedItem && pMerchantWnd->pSelectedItem->GlobalIndex.Index == pItem->GlobalIndex.Index)
				return true;
		}
		Sleep(100);
	}
	return false;
}

bool WaitForItemToBeSold(short InvSlot, short BagSlot)
{
	pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::milliseconds(5000);
	while (pluginclock::now() < WhileTimer) // Will wait up to 5 seconds or until the item is removed from that slot
	{
		Sleep(100);
		if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
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

void SellItem(CONTENTS* pItem)
{
	CHAR szCount[MAX_STRING] = { 0 };
	short InvSlot = pItem->GlobalIndex.Index.GetSlot(0);
	short BagSlot = pItem->GlobalIndex.Index.GetSlot(1);
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
		// We need to call PickupItem(eItemContainerPossessions, pItemToPickUp) within the pulse due to eq not being thread safe
		pItemToPickUp = pItem; // This is our backhanded way of doing it to avoid crashing some people
		pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(30);
		while (pluginclock::now() < WhileTimer) // Will wait up to 30 seconds or until I get the merchant buys the item
		{
			if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
			{
				return;
			}
			if (WaitForItemToBeSelected(pItem, InvSlot, BagSlot))
			{
				if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
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
						if (pMerchantWnd->SellButton->IsVisible())
						{
							if (pMerchantWnd->SellButton->IsEnabled())
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

bool FitInPersonalBank(PITEMINFO pItem)
{
	LONG nPack = 0;
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
		if (pChar->BankItems.Items.Size > (unsigned int)nPack)
		{
			if (!pChar->BankItems.Items[nPack].pObject)
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
		if (pChar->BankItems.Items.Size > (unsigned int)nPack)
		{
			if (PCONTENTS pPack = pChar->BankItems.Items[nPack].pObject)
			{
				if (PITEMINFO pItemPack = GetItemFromContents(pPack))
				{
					if (pItemPack->Type == ITEMTYPE_PACK && _stricmp(pItemPack->Name, szExcludeBag1) && _stricmp(pItemPack->Name, szExcludeBag2))
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
	if (pItem)
	{
		sprintf_s(szCommand, "/nomodkey /itemnotify \"%s\" leftmouseup", pItem->Name);
		HideDoCommand(GetCharInfo()->pSpawn, szCommand,true);
		pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until I have an item in my cursor
		while (pluginclock::now() < WhileTimer) // While loop to wait for something to pop onto my cursor
		{
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
			if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				bEndThreads = true;
				return;
			}
			if (auto pChar2 = GetPcProfile())
			{
				if (pChar2->pInventoryArray && pChar2->pInventoryArray->Inventory.Cursor)
				{
					break;
				}
			}
		}
		if (auto pChar2 = GetPcProfile())
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
				if (CONTENTS* pItem2 = pChar2->pInventoryArray->Inventory.Cursor)
				{
					if (WinState(pBankWnd))
					{
						if (CXWnd *pWndButton = pBankWnd->GetChildItem("BIGB_AutoButton"))
						{
							if (FitInPersonalBank(GetItemFromContents(pItem2)))
							{
								if (iSpamLootInfo)
								{
									WriteChatf("%s:: Putting \ag%s\ax into my personal bank", PLUGIN_CHAT_MSG, pItem2->Item2->Name);
								}
								pWndLeftMouseUp = pWndButton;
								//SendWndClick2(pWndButton, "leftmouseup");
								Sleep(1000);
							}
						}
					}
				}
			}
		}
		else
		{

			bEndThreads = true;
		}
	}
}

uint32_t GetGuildBankFreeSlots()
{
	auto pWnd = FindMQ2Window("GuildBankWnd");
	if (!pWnd || !pWnd->IsVisible())
		throw std::exception("Your guild bank window is closed for some reason");

	auto pLabel = pWnd->GetChildItem("GBANK_BankCountLabel");
	if (!pLabel)
		throw std::exception("I can't find your free slot count label on your guild bank window");

	// Label is "Free Slots: 123"
	return GetIntFromString(pLabel->GetWindowText().substr(lengthof("Free Slots: ") - 1), 0);
}

uint32_t GetGuildBankDepositFreeSlots()
{
	auto pWnd = FindMQ2Window("GuildBankWnd");
	if (!pWnd || !pWnd->IsVisible())
		throw std::exception("Your guild bank window is closed for some reason");

	auto pLabel = pWnd->GetChildItem("GBANK_DepositCountLabel");
	if (!pLabel)
		throw std::exception("I can't find your deposit free slot count label on your guild bank window");

	// Label is "Free Slots: 123"
	return GetIntFromString(pLabel->GetWindowText().substr(lengthof("Free Slots: ") - 1), 0);
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
			try
			{
				auto SlotsLeft = GetGuildBankDepositFreeSlots();
				bool bLoreItem = false;
				if (pItem->Lore != 0)
				{
					if (CListWnd * cLWnd = (CListWnd*)FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositList"))
						if (cLWnd->Contains(1, [&](auto text) { return ci_equals(text, pItem->Name); }))
							bLoreItem = true;  // A item with the same name is already deposited
					if (CListWnd * cLWnd = (CListWnd*)FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_ItemList"))
						if (cLWnd->Contains(1, [&](auto text) { return ci_equals(text, pItem->Name); }))
							bLoreItem = true;  // A item with the same name is already deposited
				}
				if (!bLoreItem)
				{
					if (SlotsLeft > 0)
					{
						return true;
					}
					else
					{
						WriteChatf("%s:: Your guild bank ran out of space, closing guild bank window", PLUGIN_CHAT_MSG);
						bEndThreads = true;
						return false;
					}
				}
				else
				{
					WriteChatf("%s:: %s is lore and you have one already in your guild bank, skipping depositing.", PLUGIN_CHAT_MSG, pItem->Name);
					HideDoCommand(GetCharInfo()->pSpawn, "/autoinventory", true);
					Sleep(1000); // TODO figure out what to wait for so I don't have to hardcode this long of a delay
					return false;
				}
			}
			catch (std::exception& e)
			{
				WriteChatf("%s:: %s", PLUGIN_CHAT_MSG, e.what());
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
	pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until I have an item in my cursor
	while (pluginclock::now() < WhileTimer) // While loop to wait for something to pop onto my cursor
	{
		Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
		if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
		{
			bEndThreads = true;
			return false;
		}
		if (WinState(FindMQ2Window("GuildBankWnd")))
		{
			if (CXWnd *pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositButton"))
			{
				if (pWndButton->IsEnabled())
				{
					break;
				}
			}
		}
	}
	if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
	{
		bEndThreads = true;
		return false;
	}
	if (CXWnd *pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositButton"))
	{
		if (!pWndButton->IsEnabled())
		{
			WriteChatf("%s:: Whoa the deposit button isn't ready, most likely you don't have the correct rights", PLUGIN_CHAT_MSG);
			DoCommand(GetCharInfo()->pSpawn, "/autoinventory");
			bEndThreads = true;
			return false;
		}
		else
		{
			if (auto pChar2 = GetPcProfile())
			{
				if (pChar2->pInventoryArray)
				{
					if (CONTENTS* pItem2 = pChar2->pInventoryArray->Inventory.Cursor)
					{
						if (iSpamLootInfo)
						{
							WriteChatf("%s:: Putting \ag%s\ax into my guild bank", PLUGIN_CHAT_MSG, pItem2->Item2->Name);
						}
						pWndLeftMouseUp = pWndButton;
						//SendWndClick2(pWndButton, "leftmouseup");
						Sleep(2000); // TODO figure out what to wait for so I don't have to hardcode this long of a delay
						return true;
					}
				}
			}
		}
	}
	WriteChatf("%s:: The function DepositItems failed, stopping depositing items", PLUGIN_CHAT_MSG);
	bEndThreads = true;
	return false;  // Shit for some reason I got to this point, going to bail on depositing items
}

bool PutInGuildBank(PITEMINFO pItem)
{
	if (WinState(FindMQ2Window("GuildBankWnd")))
	{
		if (CListWnd *cLWnd = (CListWnd *)FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositList"))
		{
			for (int nDepositList = 0; nDepositList < cLWnd->ItemsArray.GetLength(); nDepositList++)
			{
				if (ci_equals(cLWnd->GetItemText(nDepositList, 1), pItem->Name))
				{
					if (cLWnd->GetCurSel() != nDepositList)
					{
						SendListSelect("GuildBankWnd", "GBANK_DepositList", nDepositList);
					}
					pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until you select the right item
					while (pluginclock::now() < WhileTimer) // While loop to wait till you select the right item
					{
						if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
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
						if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
						{
							bEndThreads = true;
							return false;
						}
						if (WinState(FindMQ2Window("GuildBankWnd")))
						{
							if (CXWnd * pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PromoteButton"))
							{
								if (pWndButton->IsEnabled())
								{
									break;
								}
							}
						}
						Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
					}
					if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
					{
						bEndThreads = true;
						return false;
					}
					try
					{
						auto SlotsLeft = GetGuildBankFreeSlots();
						if (SlotsLeft > 0)
						{
							if (CXWnd * pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PromoteButton"))
							{
								if (pWndButton->IsEnabled())
								{
									pWndLeftMouseUp = pWndButton;
									//SendWndClick2(pWndButton, "leftmouseup");
									Sleep(2000); // TODO figure out what to wait for so I don't have to hardcode this long of a delay
									return true;
								}
								else
								{
									WriteChatf("%s:: Whoa the promote button isn't ready, most likely you don't have the correct rights", PLUGIN_CHAT_MSG);
									bEndThreads = true;
									return false;
								}
							}
							else
							{
								WriteChatf("%s:: Whoa I can't find the promote button", PLUGIN_CHAT_MSG);
								bEndThreads = true;
								return false;
							}
						}
						else // You ran out of space to promote items into your guild bank
						{
							return false;
						}

					}
					catch (std::exception& e)
					{
						WriteChatf("%s:: %s", PLUGIN_CHAT_MSG, e.what());
						bEndThreads = true;
						return false;
					}
				}
			}
		}
		else
		{
			WriteChatf("%s:: Your guild bank doesn't have a deposit list", PLUGIN_CHAT_MSG);
			bEndThreads = true;
			return false;
		}
	}
	else
	{
		WriteChatf("%s:: You don't have your guild bank window open anymore", PLUGIN_CHAT_MSG);
		bEndThreads = true;
		return false;
	}
	return false;
}

void SetItemPermissions(PITEMINFO pItem)
{
	auto pWnd = FindMQ2Window("GuildBankWnd");
	if (!WinState(pWnd))
	{
		WriteChatf("%s:: You don't have your guild bank window open anymore", PLUGIN_CHAT_MSG);
		bEndThreads = true;
		return;
	}

	auto cLWnd = (CListWnd*)pWnd->GetChildItem("GBANK_ItemList");
	if (!cLWnd)
	{
		WriteChatf("%s:: Your guild bank doesn't have a deposit list", PLUGIN_CHAT_MSG);
		bEndThreads = true;
		return;
	}

	auto nItemList = cLWnd->IndexOf(1, [&](auto text) { return ci_equals(text, pItem->Name); });
	if (nItemList == -1)
		return;

	// If permissions don't match lets fix that
	if (cLWnd->GetItemText(nItemList, 3) == szGuildItemPermission)
		return;

	if (cLWnd->GetCurSel() != nItemList)
	{
		SendListSelect("GuildBankWnd", "GBANK_DepositList", nItemList);
	}
	pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until you select the right item
	while (pluginclock::now() < WhileTimer) // While loop to wait till you select the right item
	{
		if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
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
		if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
		{
			bEndThreads = true;
			return;
		}
		if (WinState(FindMQ2Window("GuildBankWnd")))
		{
			if (CXWnd *pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PermissionCombo"))
			{
				if (pWndButton->IsEnabled())
				{
					break; // TODO see if this works
				}
			}
		}
		Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
	}
	if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
	{
		bEndThreads = true;
		return;
	}
	if (CXWnd *pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PermissionCombo"))
	{
		if (pWndButton->IsEnabled())
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
			pWndLeftMouseUp = pWndButton;
			//SendWndClick2(pWndButton, "leftmouseup");
			Sleep(2000); // TODO figure out a more elegant way of doing this
			return;
		}

		WriteChatf("%s:: Whoa the permission button isn't ready, most likely you don't have the correct rights", PLUGIN_CHAT_MSG);
		bEndThreads = true;
		return;
	}

	WriteChatf("%s:: Whoa I can't find the permission button", PLUGIN_CHAT_MSG);
	bEndThreads = true;
}

void BarterSearch(int nBarterItems, const char* pszItemName, DWORD MyBarterMinimum, CListWnd *cListInvWnd)
{
	if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
	{
		return;
	}
	pluginclock::time_point WhileTimer = pluginclock::now();
	if (WinState(FindMQ2Window("BarterSearchWnd")))  // Barter window is open
	{
		WriteChatf("%s:: For entry %i, the item's name is %s and I will sell for: %d platinum", PLUGIN_CHAT_MSG, nBarterItems + 1, pszItemName, MyBarterMinimum);
		sprintf_s(szCommand, "/nomodkey /notify BarterSearchWnd BTRSRCH_InventoryList listselect %i", nBarterItems + 1);
		HideDoCommand(GetCharInfo()->pSpawn, szCommand,true);
		WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds till we've targeted the correct item
		while (pluginclock::now() < WhileTimer) // While loop to wait till we've targeted the correct item
		{
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
			if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				return;
			}
			if (cListInvWnd)
			{
				if (cListInvWnd->GetCurSel() == nBarterItems) // not sure what this means, but it seems to be -1 when it has something selected
				{
					if (CXWnd *pWndButton = FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_SearchButton"))
					{
						if (pWndButton->IsEnabled())
						{
							pWndLeftMouseUp = pWndButton;
							//SendWndClick2(pWndButton, "leftmouseup");
							Sleep(500); // Not sure if this is necessary, but lets leave it in here till we figure out if it is required or not
							WhileTimer = pluginclock::now();
						}
					}
				}
			}
		}
		WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds for barter search to finish
		while (pluginclock::now() < WhileTimer) // While loop to wait till barter search is done
		{
			if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				return;
			}
			if (CListWnd *cBuyLineListWnd = (CListWnd *)FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_BuyLineList"))
			{
				if (cBuyLineListWnd->ItemsArray.GetLength() > 0 && ci_equals(cBuyLineListWnd->GetItemText(0, 1), pszItemName))
					WhileTimer = pluginclock::now();
			}
			if (CXWnd *pWndButton = FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_SearchButton"))
			{
				if (pWndButton->IsEnabled())
				{
					WhileTimer = pluginclock::now();
				}
			}
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
		}
	}
}

int FindBarterIndex(const char* pszItemName, DWORD MyBarterMinimum, CListWnd *cBuyLineListWnd)
{
	DWORD BarterMaximumPrice = 0;
	int BarterMaximumIndex = 0;
	for (int nBarterItems = 0; nBarterItems < cBuyLineListWnd->ItemsArray.GetLength(); nBarterItems++)
	{
		if (ci_equals(cBuyLineListWnd->GetItemText(nBarterItems, 1), pszItemName)) // Lets get the name of the item
		{
			std::string price{ cBuyLineListWnd->GetItemText(nBarterItems, 3) }; // Lets get the price of the item
			auto index = price.find("p");
			if (index != std::string_view::npos)
			{
				DWORD BarterPrice = GetIntFromString(price.substr(0, index), 0);
				if (BarterPrice > BarterMaximumPrice)
				{
					BarterMaximumPrice = BarterPrice;
					BarterMaximumIndex = nBarterItems;
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
	if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
	{
		return false;
	}
	pluginclock::time_point WhileTimer = pluginclock::now();
	if (cBuyLineListWnd->GetCurSel() != BarterMaximumIndex)
	{
		cBuyLineListWnd->SetCurSel(BarterMaximumIndex);
		WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds till we've targeted the correct item
		while (pluginclock::now() < WhileTimer) // While loop to wait till we've targeted the correct item
		{
			if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
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
			if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				return false;
			}
			if (WinState(pQuantityWnd)) // Lets wait till the Quantity Window pops up
			{
				Sleep(100);
				return true;
			}
		}
	}
	return false;
}

void SelectBarterQuantity(int BarterMaximumIndex, const char* pszItemName, CListWnd *cBuyLineListWnd)
{
	if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot barter we want to bug out
	{
		return;
	}
	pluginclock::time_point WhileTimer = pluginclock::now();
	if (WinState(pQuantityWnd))
	{
		int BarterCount = GetIntFromString(cBuyLineListWnd->GetItemText(BarterMaximumIndex, 2), 0);
		int MyCount = FindItemCountByName(pszItemName, true);
		if (MyCount == 0)
		{
			return;
		}
		int SellCount = 0;
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
			if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				return;
			}
			if (WinState(pQuantityWnd))
			{
				if (CXWnd *pWndSliderInput = (CXWnd *)pQuantityWnd->GetChildItem("QTYW_SliderInput"))
				{
					if (SellCount == GetIntFromString(pWndSliderInput->GetWindowText(), 0))
					{
						WhileTimer = pluginclock::now();
					}
				}
			}
		}
		if (WinState(pQuantityWnd))
		{
			if (CXWnd *pWndButton = pQuantityWnd->GetChildItem("QTYW_Accept_Button"))
			{
				bBarterItemSold = false;
				pWndLeftMouseUp = pWndButton;
				//SendWndClick2(pWndButton, "leftmouseup");
				WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds till we've targeted the correct item
				while (pluginclock::now() < WhileTimer) // While loop to wait till we've targeted the correct item
				{
					if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
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
		hSellItemsThread = 0;
		return 0;
	}
	bSellActive = true;
	bEndThreads = false;
	CHAR INISection[MAX_STRING] = { 0 };
	CHAR Value[MAX_STRING] = { 0 };
	HideDoCommand(GetCharInfo()->pSpawn, "/keypress OPEN_INV_BAGS", true);
	Sleep(500); // TODO make a smarter check of when your bags are all open
	if (!WinState(pMerchantWnd))  // I don't have the merchant window open
	{
		if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
		{
			if (psTarget->mActorClient.Class != MERCHANT_CLASS)
			{
				WriteChatf("%s:: Please target a merchant!", PLUGIN_CHAT_MSG);
				ResetItemActions();
				hSellItemsThread = 0;
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
							WriteChatf("%s:: You aren't in the proper game state or you sent another /autoloot [buy|sell|deposit|barter] command", PLUGIN_CHAT_MSG);
							ResetItemActions();
							hSellItemsThread = 0;
							return 0;
						}
						Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
					}
				}
				else
				{
					WriteChatf("%s:: You failed to open the merchant window!!", PLUGIN_CHAT_MSG);
					ResetItemActions();
					hSellItemsThread = 0;
					return 0;
				}
			}
			else
			{
				WriteChatf("%s:: You failed to get within range of the merchant!!", PLUGIN_CHAT_MSG); // This shouldn't fail since it was checked before you entered this thread
				ResetItemActions();
				hSellItemsThread = 0;
				return 0;
			}
		}
		else
		{
			WriteChatf("%s:: Please target a merchant!", PLUGIN_CHAT_MSG); // This shouldn't fail since it was checked before you entered this thread
			ResetItemActions();
			hSellItemsThread = 0;
			return 0;
		}
	}
	if (WinState(pMerchantWnd))  // merchant window is open
	{
		for (unsigned long nSlot = BAG_SLOT_START; nSlot < NUM_INV_SLOTS; nSlot++) // Looping through the items in my inventory and seening if I want to sell them
		{
			if (auto pChar2 = GetPcProfile())
			{
				if (pChar2->pInventoryArray) //check my inventory slots
				{
					if (CONTENTS* pItem = pChar2->pInventoryArray->InventoryArray[nSlot])
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
											WriteChatf("%s:: Attempting to sell \ag%s\ax.", PLUGIN_CHAT_MSG, theitem->Name);
											SellItem(pItem);
										}
										else if (!theitem->NoDrop)
										{
											WriteChatf("%s:: You attempted to sell \ag%s\ax, but it is no drop.  Setting to Keep in your loot ini file.", PLUGIN_CHAT_MSG, theitem->Name);
											WritePrivateProfileString(INISection, theitem->Name, "Keep", szLootINI);
										}
										else if (theitem->Cost == 0)
										{
											WriteChatf("%s:: You attempted to sell \ag%s\ax, but it is worth nothing.  Setting to Keep in your loot ini file.", PLUGIN_CHAT_MSG, theitem->Name);
											WritePrivateProfileString(INISection, theitem->Name, "Keep", szLootINI);
										}
									}
									if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
									{
										ResetItemActions();
										hSellItemsThread = 0;
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
			if (auto pChar2 = GetPcProfile())
			{
				if (pChar2->pInventoryArray) //Checking my bags
				{
					if (CONTENTS* pPack = pChar2->pInventoryArray->Inventory.Pack[nPack])
					{
						if (PITEMINFO pItemPack = GetItemFromContents(pPack))
						{
							if (pItemPack->Type == ITEMTYPE_PACK && pPack->Contents.ContainedItems.pItems)
							{
								for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
								{
									if (pPack && pPack->Contents.ContainedItems.pItems)
									{
										if (CONTENTS* pItem = pPack->Contents.ContainedItems.pItems->Item[nItem])
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
															WriteChatf("%s:: Attempting to sell \ag%s\ax.", PLUGIN_CHAT_MSG, theitem->Name);
															SellItem(pItem);
														}
														else if (!theitem->NoDrop)
														{
															WriteChatf("%s:: Attempted to sell \ag%s\ax, but it is no drop.  Setting to Keep in your loot ini file.", PLUGIN_CHAT_MSG, theitem->Name);
															WritePrivateProfileString(INISection, theitem->Name, "Keep", szLootINI);
														}
														else if (theitem->Cost == 0)
														{
															WriteChatf("%s:: Attempted to sell \ag%s\ax, but it is worth nothing.  Setting to Keep in your loot ini file.", PLUGIN_CHAT_MSG, theitem->Name);
															WritePrivateProfileString(INISection, theitem->Name, "Keep", szLootINI);
														}
													}
													if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
													{
														ResetItemActions();
														hSellItemsThread = 0;
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
	hSellItemsThread = 0;
	return 0;
}

DWORD __stdcall BuyItem(PVOID pData)
{
	if (!InGameOK())
	{
		hBuyItemThread = 0;
		return 0;
	}
	bBuyActive = true;
	bEndThreads = false;
	CHAR szTemp1[MAX_STRING];
	CHAR szItemToBuy[MAX_STRING];
	sprintf_s(szTemp1, "%s", (PCHAR)pData);
	CHAR *pParsedToken = NULL;
	CHAR *pParsedValue = strtok_s(szTemp1, "|", &pParsedToken);
	sprintf_s(szItemToBuy, "%s", pParsedValue);
	pParsedValue = strtok_s(NULL, "|", &pParsedToken);
	if (!WinState(pMerchantWnd))  // I don't have the guild bank window open
	{
		if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
		{
			if (psTarget->mActorClient.Class != MERCHANT_CLASS)
			{
				WriteChatf("%s:: Please target a merchant!", PLUGIN_CHAT_MSG);
				ResetItemActions();
				hBuyItemThread = 0;
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
							WriteChatf("%s:: You aren't in the proper game state or you sent another /autoloot [buy|sell|deposit|barter] command", PLUGIN_CHAT_MSG);
							ResetItemActions();
							hBuyItemThread = 0;
							return 0;
						}
						Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
					}
				}
				else
				{
					WriteChatf("%s:: You failed to open the merchant window!!", PLUGIN_CHAT_MSG);
					ResetItemActions();
					hBuyItemThread = 0;
					return 0;
				}
			}
			else
			{
				WriteChatf("%s:: You failed to get within range of the merchant!!", PLUGIN_CHAT_MSG); // This shouldn't fail since it was checked before you entered this thread
				ResetItemActions();
				hBuyItemThread = 0;
				return 0;
			}
		}
		else
		{
			WriteChatf("%s:: Please target a merchant!", PLUGIN_CHAT_MSG); // This shouldn't fail since it was checked before you entered this thread
			ResetItemActions();
			hBuyItemThread = 0;
			return 0;
		}
	}
	if (WinState(pMerchantWnd))  // merchant window are open
	{
		if (IsNumber(pParsedValue))
		{
			int iItemCount = GetIntFromString(pParsedValue, 0);
			if (CListWnd *cLWnd = (CListWnd *)FindMQ2Window("MerchantWnd")->GetChildItem("MW_ItemList"))
			{
				for (int nMerchantItems = 0; nMerchantItems < cLWnd->ItemsArray.GetLength(); nMerchantItems++)
				{
					if (!ci_equals(cLWnd->GetItemText(nMerchantItems, 1), szItemToBuy))
						continue;

					// Ok we found the right item
					auto availableCount = cLWnd->GetItemText(nMerchantItems, 2);
					if (availableCount == "--") // If this is true there is only a set amount to buy
					{
						int iMaxItemCount = GetIntFromString(availableCount, 0); // Get the maximum amount available
						if (iItemCount > iMaxItemCount)
						{
							iItemCount = iMaxItemCount;
						}
					}

					while (iItemCount > 0) // Ok so lets loop through till we get the amount of items we wanted to buy
					{
						if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
						{
							ResetItemActions();
							hBuyItemThread = 0;
							return 0;
						}
						if (cLWnd->GetCurSel() != nMerchantItems)
						{
							SendListSelect("MerchantWnd", "MW_ItemList", nMerchantItems); // If we haven't selected the right item, send the cursor there
						}
						pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(30);
						while (pluginclock::now() < WhileTimer)  // Will wait up to 30 seconds for the barter window
						{
							if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
							{
								ResetItemActions();
								hBuyItemThread = 0;
								return 0;
							}
							if (cLWnd->GetCurSel() == nMerchantItems)
							{
								WhileTimer = pluginclock::now();
							}
							Sleep(100);
						}
						if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd) || cLWnd->GetCurSel() != nMerchantItems)
						{
							ResetItemActions();
							hBuyItemThread = 0;
							return 0;
						}
						int iStartCount = FindItemCount(szItemToBuy);
						if (PCHARINFO pChar = GetCharInfo())
						{
							char szCount[MAX_STRING] = { 0 };
							sprintf_s(szCount, "%d", iItemCount);
							BuyItem(pChar->pSpawn, szCount); // This is the command from MQ2Commands.cpp
							WhileTimer = pluginclock::now() + std::chrono::seconds(30);
							while (pluginclock::now() < WhileTimer)  // Will wait up to 30 seconds for the number of items to increase in your inventory
							{
								if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
								{
									ResetItemActions();
									hBuyItemThread = 0;
									return 0;
								}
								if (FindItemCount(szItemToBuy) > iStartCount)
								{
									WhileTimer = pluginclock::now();
									iItemCount = iItemCount - FindItemCount(szItemToBuy) + iStartCount;
									WriteChatf("%s:: \ag%d\ax left to buy of \ag%s\ax!", PLUGIN_CHAT_MSG, iItemCount, szItemToBuy);
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
	else
	{
		WriteChatf("%s:: Please open a merchant window first before attempting to buy: \ag%s\ax", PLUGIN_CHAT_MSG, szItemToBuy);
	}
	ResetItemActions();
	hBuyItemThread = 0;
	return 0;
}

DWORD __stdcall DepositPersonalBanker(PVOID pData)
{
	if (!InGameOK())
	{
		hDepositPersonalBankerThread = 0;
		return 0;
	}
	bEndThreads = false;
	bDepositActive = true;
	auto pChar2 = GetPcProfile();
	if (!WinState(pBankWnd))  // I don't have the guild bank window open
	{
		if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
		{
			if (psTarget->mActorClient.Class != PERSONALBANKER_CLASS)
			{
				WriteChatf("%s:: Please target a personal banker!", PLUGIN_CHAT_MSG);
				ResetItemActions();
				hDepositPersonalBankerThread = 0;
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
							WriteChatf("%s:: You aren't in the proper game state or you sent another /autoloot [buy|sell|deposit|barter] command", PLUGIN_CHAT_MSG);
							ResetItemActions();
							hDepositPersonalBankerThread = 0;
							return 0;
						}
						Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
					}
				}
				else
				{
					WriteChatf("%s:: You failed to open your personal bank window!!", PLUGIN_CHAT_MSG);
					ResetItemActions();
					hDepositPersonalBankerThread = 0;
					return 0;
				}
			}
			else
			{
				WriteChatf("%s:: You failed to get within range of your personal banker!!", PLUGIN_CHAT_MSG);
				ResetItemActions();
				hDepositPersonalBankerThread = 0;
				return 0;
			}
		}
		else
		{
			WriteChatf("%s:: Please target a personal banker!", PLUGIN_CHAT_MSG); // This shouldn't fail since it was checked before you entered this thread
			ResetItemActions();
			hDepositPersonalBankerThread = 0;
			return 0;
		}
	}
	if (!InGameOK() || bEndThreads || !WinState(pBankWnd))  // If we aren't in a proper game state, or if they sent /autoloot deposit again we want to bug out
	{
		ResetItemActions();
		hDepositPersonalBankerThread = 0;
		return 0;
	}
	HideDoCommand(GetCharInfo()->pSpawn, "/keypress OPEN_INV_BAGS",true); // TODO check if this is necessary
	Sleep(500); // TODO lets make a smart function that checks whether your bags are open
	if (pChar2 && pChar2->pInventoryArray) //check my inventory slots
	{
		for (unsigned long nSlot = BAG_SLOT_START; nSlot < NUM_INV_SLOTS; nSlot++) // loop through my inventory
		{
			if (CONTENTS* pItem = pChar2->pInventoryArray->InventoryArray[nSlot])
			{
				if (PITEMINFO theitem = GetItemFromContents(pItem))
				{
					if (FitInPersonalBank(theitem))
					{
						PutInPersonalBank(theitem);
					}
					if (!InGameOK() || bEndThreads || !WinState(pBankWnd))
					{
						ResetItemActions();
						hDepositPersonalBankerThread = 0;
						return 0;
					}
				}
			}
		}
	}
	if (!InGameOK() || bEndThreads || !WinState(pBankWnd))  // This shouldn't be necessary
	{
		ResetItemActions();
		hDepositPersonalBankerThread = 0;
		return 0;
	}
	if (pChar2 && pChar2->pInventoryArray) //Checking my bags
	{
		for (unsigned long nPack = 0; nPack < 10; nPack++)
		{
			if (CONTENTS* pPack = pChar2->pInventoryArray->Inventory.Pack[nPack])
			{
				if (PITEMINFO pItemPack = GetItemFromContents(pPack))
				{
					if (pItemPack->Type == ITEMTYPE_PACK && pPack->Contents.ContainedItems.pItems)
					{
						for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
						{
							if (CONTENTS* pItem = pPack->Contents.ContainedItems.pItems->Item[nItem])
							{
								if (PITEMINFO theitem = GetItemFromContents(pItem))
								{
									if (FitInPersonalBank(theitem))
									{
										PutInPersonalBank(theitem);
									}
									if (!InGameOK() || bEndThreads || !WinState(pBankWnd))
									{
										ResetItemActions();
										hDepositPersonalBankerThread = 0;
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
	hDepositPersonalBankerThread = 0;
	return 0;
}

DWORD __stdcall DepositGuildBanker(PVOID pData)
{
	if (!InGameOK())
	{
		hDepositGuildBankerThread = 0;
		return 0;
	}
	bEndThreads = false;
	bDepositActive = true;
	auto pChar2 = GetPcProfile();
	if (!WinState(FindMQ2Window("GuildBankWnd")))  // I don't have the guild bank window open
	{
		if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
		{
			if (psTarget->mActorClient.Class != GUILDBANKER_CLASS)
			{
				WriteChatf("%s:: Please target a guild banker!", PLUGIN_CHAT_MSG);
				ResetItemActions();
				hDepositGuildBankerThread = 0;
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
							WriteChatf("%s:: You aren't in the proper game state or you sent another /autoloot [buy|sell|deposit|barter] command");
							ResetItemActions();
							hDepositGuildBankerThread = 0;
							return 0;
						}
						Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
					}
				}
				else
				{
					WriteChatf("%s:: You failed to open your guild bank window!!", PLUGIN_CHAT_MSG);
					ResetItemActions();
					hDepositGuildBankerThread = 0;
					return 0;
				}
			}
			else
			{
				WriteChatf("%s:: You failed to get within range of your guild banker!!", PLUGIN_CHAT_MSG);
				ResetItemActions();
				hDepositGuildBankerThread = 0;
				return 0;
			}
		}
		else
		{
			WriteChatf("%s:: Please target a guild banker!", PLUGIN_CHAT_MSG); // This shouldn't fail since it was checked before you entered this thread
			ResetItemActions();
			hDepositGuildBankerThread = 0;
			return 0;
		}
	}
	if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd")))  // If we aren't in a proper game state, or if they sent /autoloot deposit again we want to bug out
	{
		ResetItemActions();
		hDepositGuildBankerThread = 0;
		return 0;
	}
	HideDoCommand(GetCharInfo()->pSpawn, "/keypress OPEN_INV_BAGS",true); // TODO check if this is necessary
	Sleep(500);
	if (pChar2 && pChar2->pInventoryArray) //check my inventory slots
	{
		for (unsigned long nSlot = BAG_SLOT_START; nSlot < NUM_INV_SLOTS; nSlot++) // loop through my inventory
		{
			if (CONTENTS* pItem = pChar2->pInventoryArray->InventoryArray[nSlot])
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
					if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd")))
					{
						ResetItemActions();
						hDepositGuildBankerThread = 0;
						return 0;
					}
				}
			}
		}
	}
	if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd")))  // This shouldn't be necessary
	{
		ResetItemActions();
		hDepositGuildBankerThread = 0;
		return 0;
	}
	if (pChar2 && pChar2->pInventoryArray) //Checking my bags
	{
		for (unsigned long nPack = 0; nPack < 10; nPack++)
		{
			if (CONTENTS* pPack = pChar2->pInventoryArray->Inventory.Pack[nPack])
			{
				if (PITEMINFO pItemPack = GetItemFromContents(pPack))
				{
					if (pItemPack->Type == ITEMTYPE_PACK && pPack->Contents.ContainedItems.pItems)
					{
						for (unsigned long nItem = 0; nItem < pItemPack->Slots; nItem++)
						{
							if (CONTENTS* pItem = pPack->Contents.ContainedItems.pItems->Item[nItem])
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
									if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd")))
									{
										ResetItemActions();
										hDepositGuildBankerThread = 0;
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
	hDepositGuildBankerThread = 0;
	return 0;
}

DWORD __stdcall BarterItems(PVOID pData)
{
	if (!InGameOK())
	{
		hBarterItemsThread = 0;
		return 0;
	}
	if (!HasExpansion(EXPANSION_RoF))
	{
		WriteChatf("%s:: You need to have Rain of Fear expansion to use the barter functionality.", PLUGIN_CHAT_MSG);
		hBarterItemsThread = 0;
		return 0;
	}
	bBarterActive = true;
	bEndThreads = false;
	if (!WinState(FindMQ2Window("BarterSearchWnd")))  // Barter window isn't open
	{
		WriteChatf("%s:: Opening a barter window!", PLUGIN_CHAT_MSG);
		HideDoCommand(GetCharInfo()->pSpawn, "/barter",true);
		pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(30);
		while (pluginclock::now() < WhileTimer)  // Will wait up to 30 seconds for the barter window
		{
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
			if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
			{
				ResetItemActions();
				hBarterItemsThread = 0;
				return 0;
			}
			if (WinState(FindMQ2Window("BarterSearchWnd")))
			{
				WhileTimer = pluginclock::now();
			}
		}
	}
	if (WinState(FindMQ2Window("BarterSearchWnd")))  // Barter window is open
	{
		if (CListWnd *cListInvWnd = (CListWnd *)FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_InventoryList"))
		{
			for (int nBarterItems = 0; nBarterItems < cListInvWnd->ItemsArray.GetLength(); nBarterItems++)
			{
				if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot barter we want to bug out
				{
					ResetItemActions();
					hBarterItemsThread = 0;
					return 0;
				}
				auto itemName = cListInvWnd->GetItemText(nBarterItems, 1);
				char INISection[MAX_STRING] = { 0 };
				sprintf_s(INISection, "%c", itemName[0]);
				char Value[MAX_STRING] = { 0 };
				if (GetPrivateProfileString(INISection, itemName.c_str(), 0, Value, MAX_STRING, szLootINI) != 0)
				{
					CHAR *pParsedToken = NULL;
					CHAR *pParsedValue = strtok_s(Value, "|", &pParsedToken);
					if (!_stricmp(pParsedValue, "Barter"))
					{
						pParsedValue = strtok_s(NULL, "|", &pParsedToken);
						if (pParsedValue)
						{
							int MyBarterMinimum = GetIntFromString(pParsedValue, 0);
							BarterSearch(nBarterItems, itemName.c_str(), MyBarterMinimum, cListInvWnd);
							bool bBarterLoop = true;
							while (bBarterLoop)
							{
								if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot barter we want to bug out
								{
									ResetItemActions();
									hBarterItemsThread = 0;
									return 0;
								}
								if (bBarterReset)
								{
									BarterSearch(nBarterItems, itemName.c_str(), MyBarterMinimum, cListInvWnd);
									if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot barter we want to bug out
									{
										ResetItemActions();
										hBarterItemsThread = 0;
										return 0;
									}
								}
								if (FindItemCountByName(itemName.c_str(), true) > 0)
								{
									if (CListWnd *cBuyLineListWnd = (CListWnd *)FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_BuyLineList"))
									{
										if (cBuyLineListWnd->ItemsArray.GetLength() > 0)
										{
											int BarterMaximumIndex = FindBarterIndex(itemName.c_str(), MyBarterMinimum, cBuyLineListWnd); // Will return -1 if no appropriate sellers are found
											if (BarterMaximumIndex > -1)
											{
												if (SelectBarterSell(BarterMaximumIndex, cBuyLineListWnd))
												{
													SelectBarterQuantity(BarterMaximumIndex, itemName.c_str(), cBuyLineListWnd);
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
	ResetItemActions();
	hBarterItemsThread = 0;
	return 0;
}
#endif
