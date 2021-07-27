
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
ItemPtr                     pItemToPickUp = nullptr;
CXWnd*                      pWndLeftMouseUp = nullptr;
pluginclock::time_point     LootThreadTimer = pluginclock::now();

void ResetItemActions()
{
	if (WinState(pMerchantWnd))
	{
		EzCommand("/squelch /windowstate MerchantWnd close");
	}
	if (WinState(pBankWnd))
	{
		EzCommand("/squelch /windowstate BigBankWnd close");
	}
	if (WinState(FindMQ2Window("GuildBankWnd")))
	{
		EzCommand("/squelch /windowstate GuildBankWnd close");
	}
	if (WinState(FindMQ2Window("BarterSearchWnd")))
	{
		EzCommand("/squelch /windowstate BarterSearchWnd close");
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

	if (pSpawn->X && pSpawn->Y && pSpawn->Z)
	{
		FLOAT Distance = Get3DDistance(pLocalPlayer->X, pLocalPlayer->Y, pLocalPlayer->Z, pSpawn->X, pSpawn->Y, pSpawn->Z);
		if (Distance >= 20)  // I am too far away from the merchant/banker and need to move closer
		{
			if (HandleMoveUtils())
			{
				sprintf_s(szCommand, "id %d", pSpawn->SpawnID);
				fStickCommand(pLocalPlayer, szCommand);
				WriteChatf("%s:: Moving towards: \ag%s\ax", PLUGIN_CHAT_MSG, pSpawn->DisplayedName);
			}
			else
			{
				WriteChatf("%s:: MQ2MoveUtils not loaded.  Move to within 20 of the target before trying to sell or deposit", PLUGIN_CHAT_MSG);
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
					Distance = Get3DDistance(pLocalPlayer->X, pLocalPlayer->Y, pLocalPlayer->Z, pSpawn->X, pSpawn->Y, pSpawn->Z);
					if (Distance < 20)
					{
						break; // I am close enough to the banker I can stop moving towards them
					}
				}
				else
				{
					WriteChatf("%s:: Can't find the distance to banker/merchant!", PLUGIN_CHAT_MSG);
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
				fStickCommand(pLocalPlayer, "off"); // We either got close enough or timed out, lets turn stick off
			}

			if (pSpawn->X && pSpawn->Y && pSpawn->Z)
			{
				Distance = Get3DDistance(pLocalPlayer->X, pLocalPlayer->Y, pLocalPlayer->Z, pSpawn->X, pSpawn->Y, pSpawn->Z);
				if (Distance < 20)
				{
					return true; // I am close enough to the banker/merchant to open their window
				}
				else
				{
					WriteChatf("%s:: Failed to get to within 20 of banker/merchant!", PLUGIN_CHAT_MSG);
				}
			}
			else
			{
				WriteChatf("%s:: Can't find the distance!", PLUGIN_CHAT_MSG);
			}
		}
		else
		{
			return true; // Hey you already were within 20 of the spawn
		}
	}
	else
	{
		WriteChatf("%s:: Can't find the distance to npc!", PLUGIN_CHAT_MSG);
	}
	return false;
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

	if (pTarget)
	{
		if (pTarget->SpawnID == pSpawn->SpawnID)
		{
			if (pSpawn->GetClass() == Class_Merchant)
			{
				WriteChatf("%s:: Opening merchant window and waiting 30 seconds for the merchant window to populate", PLUGIN_CHAT_MSG);
			}
			if (pSpawn->GetClass() == Class_Banker)
			{
				WriteChatf("%s:: Opening personal banking window and waiting 30 seconds for the personal banking window to populate", PLUGIN_CHAT_MSG);
			}
			if (pSpawn->GetClass() == Class_GuildBanker)
			{
				WriteChatf("%s:: Opening guild banking window and waiting 30 seconds for the guild banking window to populate", PLUGIN_CHAT_MSG);
			}

			EzCommand("/face nolook");
			EzCommand("/nomodkey /click right target");

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

		WriteChatf("%s:: Your target has changed!", PLUGIN_CHAT_MSG);
	}
	else
	{
		WriteChatf("%s:: You don't have a target!", PLUGIN_CHAT_MSG);
	}

	bEndThreads = true;
	return false;
}

bool WaitForItemToBeSelected(const ItemPtr& pItem, short InvSlot, short BagSlot)
{
	Sleep(750);  // Lets see if 750 ms is sufficient of a delay, TODO figure out a smarter way to determine when a merchant is ready to buy an item
	pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(30);

	while (pluginclock::now() < WhileTimer) // Will wait up to 30 seconds or until I get the merchant buys the item
	{
		if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
		{
			return false;
		}

		if (!FindItemBySlot(InvSlot, BagSlot)) // There is no item in
		{
			return false;
		}

		if (pMerchantWnd
			&& pMerchantWnd->pSelectedItem
			&& pMerchantWnd->pSelectedItem->GetItemLocation() == pItem->GetItemLocation())
		{
			return true; //adding this check incase someone clicks on a different item
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

		if (!FindItemBySlot(InvSlot, BagSlot)) // There is no item in that slot
		{
			return true;
		}
	}
	return false; // we didn't sell the item, going to keep in the SellItem function till it sells, or 30 seconds runs out
}

void SellItem(const ItemPtr& pItem)
{
	if (pItem)
	{
		CHAR szCount[MAX_STRING] = { 0 };
		short InvSlot = pItem->GetItemLocation().GetSlot(0);
		short BagSlot = pItem->GetItemLocation().GetSlot(1);

		if (!pItem->IsStackable())
		{
			sprintf_s(szCount, "1");
		}
		else
		{
			sprintf_s(szCount, "%d", pItem->GetItemCount());
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
				// Even though this is checked above, WaitForItemToBeSelected has a sleep in it
				if (!InGameOK() || bEndThreads || !WinState(pMerchantWnd))
				{
					return;
				}

				if (!FindItemBySlot(InvSlot, BagSlot))
				{
					return;
				}
				if (pMerchantWnd->SellButton->IsVisible())
				{
					if (pMerchantWnd->SellButton->IsEnabled())
					{
						SellItem(pLocalPlayer, szCount); // This is the command from MQ2Commands.cpp
						if (WaitForItemToBeSold(InvSlot, BagSlot))
						{
							return;
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

bool FitInPersonalBank(const ItemPtr& pItem)
{
	CHAR INISection[MAX_STRING] = { 0 };
	CHAR Value[MAX_STRING] = { 0 };
	sprintf_s(INISection, "%c", pItem->GetName()[0]);

	if (GetPrivateProfileString(INISection, pItem->GetName(), 0, Value, MAX_STRING, szLootINI) != 0)
	{
		CHAR* pParsedToken = NULL;
		CHAR* pParsedValue = strtok_s(Value, "|", &pParsedToken);
		if (_stricmp(pParsedValue, "Keep"))
		{
			return false; // The item isn't set to Keep
		}
	}
	else
	{
		return false;  // The item isn't in your loot.ini file
	}

	for (int nPack = 0; nPack < GetAvailableBankSlots(); nPack++)
	{
		ItemPtr pBankItem = pLocalPC->BankItems.GetItem(nPack);

		if (!pBankItem)
		{
			return true;
		}

		// checking inside bank bags
		if (pBankItem->IsContainer()
			&& pBankItem->GetItemDefinition()->SizeCapacity >= pItem->GetItemDefinition()->Size
			&& !ci_equals(pBankItem->GetName(), szExcludeBag1)
			&& !ci_equals(pBankItem->GetName(), szExcludeBag2))
		{
			for (const ItemPtr& pBagItem : pBankItem->GetHeldItems())
			{
				if (!pBagItem) return true;
			}
		}
	}

	return false;
}

void PutInPersonalBank(const ItemPtr& pItem)
{
	if (!InGameOK() || bEndThreads)
	{
		bEndThreads = true;
		return;
	}

	if (pItem)
	{
		sprintf_s(szCommand, "/nomodkey /itemnotify \"%s\" leftmouseup", pItem->GetName());
		EzCommand(szCommand);

		pluginclock::time_point WhileTimer = pluginclock::now() + std::chrono::seconds(10); // Will wait up to 10 seconds or until I have an item in my cursor
		while (pluginclock::now() < WhileTimer) // While loop to wait for something to pop onto my cursor
		{
			Sleep(100);  // Sleep for 100 ms and lets check the previous conditions again
			if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot command we want to bug out
			{
				bEndThreads = true;
				return;
			}

			if (GetPcProfile()->GetInventorySlot(InvSlot_Cursor))
			{
				break;
			}
		}

		if (!GetPcProfile()->GetInventorySlot(InvSlot_Cursor))
		{
			bEndThreads = true;
		}

		if (!InGameOK() || bEndThreads)
		{
			bEndThreads = true;
			return;
		}

		if (ItemPtr pItem = GetPcProfile()->GetInventorySlot(InvSlot_Cursor))
		{
			if (WinState(pBankWnd))
			{
				if (CXWnd* pWndButton = pBankWnd->GetChildItem("BIGB_AutoButton"))
				{
					if (FitInPersonalBank(pItem))
					{
						if (iSpamLootInfo)
						{
							WriteChatf("%s:: Putting \ag%s\ax into personal bank", PLUGIN_CHAT_MSG, pItem->GetName());
						}

						pWndLeftMouseUp = pWndButton;
						Sleep(1000);
					}
				}
			}
		}
	}
}

bool CheckGuildBank(const ItemPtr& pItem)
{
	if (!InGameOK() || bEndThreads)
	{
		bEndThreads = true;
		return false;
	}

	CHAR INISection[MAX_STRING] = { 0 };
	CHAR Value[MAX_STRING] = { 0 };
	sprintf_s(INISection, "%c", pItem->GetName()[0]);
	if (GetPrivateProfileString(INISection, pItem->GetName(), 0, Value, MAX_STRING, szLootINI) != 0)
	{
		CHAR* pParsedToken = NULL;
		CHAR* pParsedValue = strtok_s(Value, "|", &pParsedToken);

		if (!_stricmp(pParsedValue, "Deposit"))
		{
			if (WinState(FindMQ2Window("GuildBankWnd")))
			{
				if (CXWnd* pWnd = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositCountLabel"))
				{
					CHAR szDepositCountText[MAX_STRING] = { 0 };
					strcpy_s(szDepositCountText, pWnd->GetWindowText().c_str());

					CHAR* pParsedToken = NULL;
					CHAR* pParsedValue = strtok_s(szDepositCountText, ":", &pParsedToken);
					pParsedValue = strtok_s(NULL, ":", &pParsedToken);
					DWORD SlotsLeft = atoi(pParsedValue);
					bool bLoreItem = false;

					if (pItem->GetItemDefinition()->Lore != 0)
					{
						if (CListWnd* cLWnd = (CListWnd*)FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositList"))
						{
							for (int nDepositList = 0; nDepositList < cLWnd->ItemsArray.GetLength(); nDepositList++)
							{
								CXStr itemName = cLWnd->GetItemText(nDepositList, 1);

								if (ci_equals(itemName, pItem->GetName()))
								{
									bLoreItem = true;  // A item with the same name is already deposited
								}
							}
						}

						if (CListWnd* cLWnd = (CListWnd*)FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_ItemList"))
						{
							for (int nItemList = 0; nItemList < cLWnd->ItemsArray.GetLength(); nItemList++)
							{
								CXStr itemName = cLWnd->GetItemText(nItemList, 1);

								if (ci_equals(itemName, pItem->GetName()))
								{
									bLoreItem = true;  // A item with the same name is already deposited
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
							WriteChatf("%s:: Guild bank ran out of space, closing guild bank window", PLUGIN_CHAT_MSG);
							bEndThreads = true;
							return false;
						}
					}
					else
					{
						WriteChatf("%s:: %s is lore and you have one already in your guild bank, skipping depositing.", PLUGIN_CHAT_MSG, pItem->GetName());
						EzCommand("/autoinventory");
						Sleep(1000); // TODO figure out what to wait for so I don't have to hardcode this long of a delay
						return false;
					}
				}
				else // The guild bank window doesn't have a deposit button
				{
					WriteChatf("%s:: Can't find deposit button on guild bank window", PLUGIN_CHAT_MSG);
					bEndThreads = true;
					return false;
				}
			}
			else // Guild Bank Window isn't open, bugging out of this thread
			{
				WriteChatf("%s:: Guild bank window is closed", PLUGIN_CHAT_MSG);
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

bool DepositItems(const ItemPtr& pItem)  //This should only be run from inside threads
{
	if (!InGameOK() || bEndThreads)  // If we aren't in a proper game state, or if they sent /autoloot deposit we want to bug out
	{
		bEndThreads = true;
		return false;
	}

	sprintf_s(szCommand, "/nomodkey /itemnotify \"%s\" leftmouseup", pItem->GetName());  // Picking up the item
	EzCommand(szCommand);

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
			if (CXWnd* pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositButton"))
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

	if (CXWnd* pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositButton"))
	{
		if (!pWndButton->IsEnabled())
		{
			WriteChatf("%s:: Deposit button isn't ready, most likely you don't have the correct rights", PLUGIN_CHAT_MSG);
			EzCommand("/autoinventory");
			bEndThreads = true;
			return false;
		}
		else
		{
			if (ItemPtr pItem = GetPcProfile()->GetInventorySlot(InvSlot_Cursor))
			{
				if (iSpamLootInfo)
				{
					WriteChatf("%s:: Putting \ag%s\ax into guild bank", PLUGIN_CHAT_MSG, pItem->GetName());
				}

				pWndLeftMouseUp = pWndButton;
				//SendWndClick2(pWndButton, "leftmouseup");
				Sleep(2000); // TODO figure out what to wait for so I don't have to hardcode this long of a delay
				return true;
			}
		}
	}

	WriteChatf("%s:: The function DepositItems failed, stopped depositing items", PLUGIN_CHAT_MSG);
	bEndThreads = true;
	return false;  // Shit for some reason I got to this point, going to bail on depositing items
}

bool PutInGuildBank(const ItemPtr& pItem)
{
	if (WinState(FindMQ2Window("GuildBankWnd")))
	{
		if (CListWnd* cLWnd = (CListWnd*)FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_DepositList"))
		{
			for (int nDepositList = 0; nDepositList < cLWnd->ItemsArray.GetLength(); nDepositList++)
			{
				CXStr itemName = cLWnd->GetItemText(nDepositList, 1);

				if (ci_equals(itemName, pItem->GetName()))
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
							if (CXWnd* pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PromoteButton"))
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

					if (CXWnd* pWnd = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_BankCountLabel"))
					{
						CHAR szBankCountText[32] = { 0 };
						strcpy_s(szBankCountText, pWnd->GetWindowText().c_str());

						CHAR* pParsedToken = NULL;
						CHAR* pParsedValue = strtok_s(szBankCountText, ":", &pParsedToken);
						pParsedValue = strtok_s(NULL, ":", &pParsedToken);
						DWORD SlotsLeft = atoi(pParsedValue);
						if (SlotsLeft > 0)
						{
							if (CXWnd* pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PromoteButton"))
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
									WriteChatf("%s:: The promote button isn't ready, most likely you don't have the correct rights", PLUGIN_CHAT_MSG);
									bEndThreads = true;
									return false;
								}
							}
							else
							{
								WriteChatf("%s:: Can't find the promote button", PLUGIN_CHAT_MSG);
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
		else
		{
			WriteChatf("%s:: Guild bank doesn't have a deposit list", PLUGIN_CHAT_MSG);
			bEndThreads = true;
			return false;
		}
	}
	else
	{
		WriteChatf("%s:: Guild bank window isn't open anymore", PLUGIN_CHAT_MSG);
		bEndThreads = true;
		return false;
	}
	return false;
}

void SetItemPermissions(const ItemPtr& pItem)
{
	auto pWnd = FindMQ2Window("GuildBankWnd");
	if (!WinState(pWnd))
	{
		WriteChatf("%s:: Guild bank window isn't open anymore", PLUGIN_CHAT_MSG);
		bEndThreads = true;
		return;
	}

	auto cLWnd = (CListWnd*)pWnd->GetChildItem("GBANK_ItemList");
	if (!cLWnd)
	{
		WriteChatf("%s:: Guild bank doesn't have a deposit list", PLUGIN_CHAT_MSG);
		bEndThreads = true;
		return;
	}

	for (int nItemList = 0; nItemList < cLWnd->ItemsArray.GetLength(); nItemList++)
	{
		CXStr itemName = cLWnd->GetItemText(nItemList, 1);
		if (ci_equals(itemName, pItem->GetName()))
		{
			CXStr itemPermission = cLWnd->GetItemText(nItemList, 3);
			if (!ci_equals(itemPermission, szGuildItemPermission)) // If permissions don't match lets fix that
			{
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
						if (CXWnd* pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PermissionCombo"))
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

				if (CXWnd* pWndButton = FindMQ2Window("GuildBankWnd")->GetChildItem("GBANK_PermissionCombo"))
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

					WriteChatf("%s:: The permission button isn't ready, most likely you don't have the correct rights", PLUGIN_CHAT_MSG);
					bEndThreads = true;
					return;
				}

				WriteChatf("%s:: Can't find the permission button", PLUGIN_CHAT_MSG);
				bEndThreads = true;
			}
		}
	}
}

void BarterSearch(int nBarterItems, const char* pszItemName, DWORD MyBarterMinimum, CListWnd* cListInvWnd)
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
		EzCommand(szCommand);
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

			if (CListWnd* cBuyLineListWnd = (CListWnd*)FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_BuyLineList"))
			{
				CXStr itemName = cBuyLineListWnd->GetItemText(0, 1);
				if (ci_equals(itemName, pszItemName))
				{
					WhileTimer = pluginclock::now();
				}
			}

			if (CXWnd* pWndButton = FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_SearchButton"))
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

bool SelectBarterSell(int BarterMaximumIndex, CListWnd* cBuyLineListWnd)
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
		EzCommand("/nomodkey /notify BarterSearchWnd SellButton leftmouseup");

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

void SelectBarterQuantity(int BarterMaximumIndex, const char* pszItemName, CListWnd* cBuyLineListWnd)
{
	if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("BarterSearchWnd"))) // If we aren't in a proper game state, or if they sent /autoloot barter we want to bug out
	{
		return;
	}

	pluginclock::time_point WhileTimer = pluginclock::now();
	if (WinState(pQuantityWnd))
	{
		int BarterCount = GetIntFromString(cBuyLineListWnd->GetItemText(BarterMaximumIndex, 2), 0);
		int MyCount = FindItemCountByName(pszItemName);

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
		EzCommand(szCommand);

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
				if (CXWnd* pWndSliderInput = (CXWnd*)pQuantityWnd->GetChildItem("QTYW_SliderInput"))
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
	EzCommand("/keypress OPEN_INV_BAGS");
	Sleep(500); // TODO make a smarter check of when your bags are all open

	if (!WinState(pMerchantWnd))  // I don't have the merchant window open
	{
		if (pTarget)
		{
			if (pTarget->GetClass() != Class_Merchant)
			{
				WriteChatf("%s:: Please target a merchant!", PLUGIN_CHAT_MSG);
				ResetItemActions();
				hSellItemsThread = 0;
				return 0;
			}
			if (MoveToNPC(pTarget))
			{
				if (OpenWindow(pTarget))
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
		for (int nSlot = InvSlot_FirstBagSlot; nSlot <= GetHighestAvailableBagSlot(); nSlot++)  // Looping through the items in my inventory and seening if I want to sell them
		{
			ItemPtr pItem = GetPcProfile()->GetInventorySlot(nSlot);
			if (!pItem || pItem->IsContainer())
				continue;

			// It isn't a pack! we should sell it
			sprintf_s(INISection, "%c", pItem->GetName()[0]);
			if (GetPrivateProfileString(INISection, pItem->GetName(), 0, Value, MAX_STRING, szLootINI) != 0)
			{
				CHAR* pParsedToken = NULL;
				CHAR* pParsedValue = strtok_s(Value, "|", &pParsedToken);
				if (!_stricmp(pParsedValue, "Sell"))
				{
					if (pItem->GetItemDefinition()->IsDroppable && pItem->GetItemDefinition()->Cost > 0)
					{
						WriteChatf("%s:: Attempting to sell \ag%s\ax.", PLUGIN_CHAT_MSG, pItem->GetName());
						SellItem(pItem);
					}
					else if (!pItem->GetItemDefinition()->IsDroppable)
					{
						WriteChatf("%s:: You attempted to sell \ag%s\ax, but it is no drop.  Setting to Keep in your loot ini file.", PLUGIN_CHAT_MSG, pItem->GetName());
						WritePrivateProfileString(INISection, pItem->GetName(), "Keep", szLootINI);
					}
					else if (pItem->GetItemDefinition()->Cost > 0 == 0)
					{
						WriteChatf("%s:: You attempted to sell \ag%s\ax, but it is worth nothing.  Setting to Keep in your loot ini file.", PLUGIN_CHAT_MSG, pItem->GetName());
						WritePrivateProfileString(INISection, pItem->GetName(), "Keep", szLootINI);
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

		for (int nPack = InvSlot_FirstBagSlot; nPack <= GetHighestAvailableBagSlot(); nPack++)
		{
			ItemPtr pPack = GetPcProfile()->GetInventorySlot(nPack);
			if (!pPack || !pPack->IsContainer())
				continue;

			for (const ItemPtr& pItem : pPack->GetHeldItems())
			{
				sprintf_s(INISection, "%c", pItem->GetName()[0]);

				if (GetPrivateProfileString(INISection, pItem->GetName(), 0, Value, MAX_STRING, szLootINI) != 0)
				{
					CHAR* pParsedToken = NULL;
					CHAR* pParsedValue = strtok_s(Value, "|", &pParsedToken);

					if (!_stricmp(pParsedValue, "Sell"))
					{
						// FIXME:  This code is duplicated around line 1164
						if (pItem->GetItemDefinition()->IsDroppable && pItem->GetItemDefinition()->Cost > 0)
						{
							WriteChatf("%s:: Attempting to sell \ag%s\ax.", PLUGIN_CHAT_MSG, pItem->GetName());
							SellItem(pItem.get());
						}
						else if (!pItem->GetItemDefinition()->IsDroppable)
						{
							WriteChatf("%s:: Attempted to sell \ag%s\ax, but it is no drop.  Setting to Keep in your loot ini file.", PLUGIN_CHAT_MSG, pItem->GetName());
							WritePrivateProfileString(INISection, pItem->GetName(), "Keep", szLootINI);
						}
						else if (pItem->GetItemDefinition()->Cost > 0 == 0)
						{
							WriteChatf("%s:: Attempted to sell \ag%s\ax, but it is worth nothing.  Setting to Keep in your loot ini file.", PLUGIN_CHAT_MSG, pItem->GetName());
							WritePrivateProfileString(INISection, pItem->GetName(), "Keep", szLootINI);
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
	CHAR* pParsedToken = NULL;
	CHAR* pParsedValue = strtok_s(szTemp1, "|", &pParsedToken);
	sprintf_s(szItemToBuy, "%s", pParsedValue);
	pParsedValue = strtok_s(NULL, "|", &pParsedToken);

	if (!WinState(pMerchantWnd))  // I don't have the guild bank window open
	{
		if (pTarget)
		{
			if (pTarget->GetClass() != Class_Merchant)
			{
				WriteChatf("%s:: Please target a merchant!", PLUGIN_CHAT_MSG);
				ResetItemActions();
				hBuyItemThread = 0;
				return 0;
			}
			if (MoveToNPC(pTarget))
			{
				if (OpenWindow(pTarget))
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
			if (CListWnd* cLWnd = (CListWnd*)FindMQ2Window("MerchantWnd")->GetChildItem("MW_ItemList"))
			{
				for (int nMerchantItems = 0; nMerchantItems < cLWnd->ItemsArray.GetLength(); nMerchantItems++)
				{
					if (ci_equals(szItemToBuy, cLWnd->GetItemText(nMerchantItems, 1))) // We found the right item
					{
						// We need to figure out how many are available to buy
						CXStr availableCount = cLWnd->GetItemText(nMerchantItems, 2);

						if (availableCount != "--") // There is only a set amount to buy
						{
							int iMaxItemCount = GetIntFromString(availableCount, 0); // Get the maximum amount available
							if (iItemCount > iMaxItemCount)
							{
								iItemCount = iMaxItemCount;
							}
						}

						while (iItemCount > 0) // Ok so lets loop through until we get the amount of items we wanted to buy
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

							char szCount[32] = { 0 };
							sprintf_s(szCount, "%d", iItemCount);
							BuyItem(pLocalPlayer, szCount); // This is the command from MQ2Commands.cpp
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

	if (!WinState(pBankWnd))  // I don't have the guild bank window open
	{
		if (pTarget)
		{
			if (pTarget->GetClass() != Class_Banker)
			{
				WriteChatf("%s:: Please target a personal banker!", PLUGIN_CHAT_MSG);
				ResetItemActions();
				hDepositPersonalBankerThread = 0;
				return 0;
			}

			if (MoveToNPC(pTarget))
			{
				LootThreadTimer = pluginclock::now() + std::chrono::seconds(30);
				if (OpenWindow(pTarget))
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

	EzCommand("/keypress OPEN_INV_BAGS"); // TODO check if this is necessary

	Sleep(500); // TODO lets make a smart function that checks whether your bags are open

	for (int nSlot = InvSlot_FirstBagSlot; nSlot <= GetHighestAvailableBagSlot(); nSlot++) // loop through my inventory
	{
		ItemPtr pItem = GetPcProfile()->GetInventorySlot(nSlot);
		if (!pItem)
			continue;

		if (FitInPersonalBank(pItem))
		{
			PutInPersonalBank(pItem);
		}

		if (!InGameOK() || bEndThreads || !WinState(pBankWnd))
		{
			ResetItemActions();
			hDepositPersonalBankerThread = 0;
			return 0;
		}
	}

	if (!InGameOK() || bEndThreads || !WinState(pBankWnd))  // This shouldn't be necessary
	{
		ResetItemActions();
		hDepositPersonalBankerThread = 0;
		return 0;
	}

	for (int nPack = InvSlot_FirstBagSlot; nPack <= GetHighestAvailableBagSlot(); nPack++) // loop through my bags
	{
		ItemPtr pPack = GetPcProfile()->GetInventorySlot(nPack);
		if (pPack && pPack->IsContainer())
		{
			for (const ItemPtr& pItem : pPack->GetHeldItems())
			{
				if (pItem)
				{
					if (FitInPersonalBank(pItem))
					{
						PutInPersonalBank(pItem);
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

	if (!WinState(FindMQ2Window("GuildBankWnd")))  // I don't have the guild bank window open
	{
		if (pTarget)
		{
			if (pTarget->GetClass() != Class_GuildBanker)
			{
				WriteChatf("%s:: Please target a guild banker!", PLUGIN_CHAT_MSG);
				ResetItemActions();
				hDepositGuildBankerThread = 0;
				return 0;
			}

			if (MoveToNPC(pTarget))
			{
				if (OpenWindow(pTarget))
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

	EzCommand("/keypress OPEN_INV_BAGS"); // TODO check if this is necessary
	Sleep(500);

	for (int nSlot = InvSlot_FirstBagSlot; nSlot <= GetHighestAvailableBagSlot(); nSlot++) // loop through my inventory
	{
		ItemPtr pItem = GetPcProfile()->GetInventorySlot(nSlot);
		if (!pItem)
			continue;

		if (CheckGuildBank(pItem) && DepositItems(pItem) && PutInGuildBank(pItem))
		{
			SetItemPermissions(pItem);
		}

		if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd")))
		{
			ResetItemActions();
			hDepositGuildBankerThread = 0;
			return 0;
		}
	}

	if (!InGameOK() || bEndThreads || !WinState(FindMQ2Window("GuildBankWnd")))  // This shouldn't be necessary
	{
		ResetItemActions();
		hDepositGuildBankerThread = 0;
		return 0;
	}

	for (int nPack = InvSlot_FirstBagSlot; nPack <= GetHighestAvailableBagSlot(); nPack++) // loop through my bags
	{
		ItemPtr pPack = GetPcProfile()->GetInventorySlot(nPack);
		if (pPack && pPack->IsContainer())
		{
			for (const ItemPtr& pItem : pPack->GetHeldItems())
			{
				if (!pItem)
				  continue;

				if (CheckGuildBank(pItem) && DepositItems(pItem) && PutInGuildBank(pItem))
				{
					SetItemPermissions(pItem);
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
		EzCommand("/barter");

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
		if (CListWnd* cListInvWnd = (CListWnd*)FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_InventoryList"))
		{
			for (int nBarterItems = 0; nBarterItems < cListInvWnd->ItemsArray.GetLength(); nBarterItems++)
			{
				if (!InGameOK() || bEndThreads) // If we aren't in a proper game state, or if they sent /autoloot barter we want to bug out
				{
					ResetItemActions();
					hBarterItemsThread = 0;
					return 0;
				}

				CXStr itemName = cListInvWnd->GetItemText(nBarterItems, 1);
				char INISection[MAX_STRING] = { 0 };
				sprintf_s(INISection, "%c", itemName[0]);

				char Value[MAX_STRING] = { 0 };
				if (GetPrivateProfileString(INISection, itemName.c_str(), 0, Value, MAX_STRING, szLootINI) != 0)
				{
					CHAR* pParsedToken = NULL;
					CHAR* pParsedValue = strtok_s(Value, "|", &pParsedToken);
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
								if (FindInventoryItemCountByName(itemName.c_str()) > 0)
								{
									if (CListWnd* cBuyLineListWnd = (CListWnd*)FindMQ2Window("BarterSearchWnd")->GetChildItem("BTRSRCH_BuyLineList"))
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
