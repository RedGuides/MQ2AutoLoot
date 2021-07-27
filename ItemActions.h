
#pragma once
#include <chrono>

typedef std::chrono::high_resolution_clock pluginclock;

// Variables necessary to deposit/sell/barter/etc items
extern bool                         bBarterActive;
extern bool                         bDepositActive;
extern bool                         bSellActive;
extern bool                         bBuyActive;
extern bool                         bBarterReset; // Set to true when you need to refresh your barter search
extern bool                         bBarterItemSold; // Set to true when you sell an item
extern bool                         bEndThreads; // Set to true when you want to end any threads out there, also it is used to enforce that at most a single thread is active at one time
extern ItemPtr                      pItemToPickUp;
extern CXWnd*                       pWndLeftMouseUp;
extern pluginclock::time_point      LootThreadTimer;

// Functions necessary to deposit/sell/barter/etc items
void ResetItemActions();
bool MoveToNPC(PSPAWNINFO pSpawn);
bool HandleMoveUtils();  // Used to connect to MQ2MoveUtils
bool OpenWindow(PSPAWNINFO pSpawn);
bool WaitForItemToBeSelected(const ItemPtr& pItem, short InvSlot, short BagSlot);
bool WaitForItemToBeSold(short InvSlot, short BagSlot);

void SellItem(const ItemPtr& pItem);
bool FitInPersonalBank(const ItemPtr& pItem);
void PutInPersonalBank(const ItemPtr& pItem);
bool CheckGuildBank(const ItemPtr& pItem);
bool PutInGuildBank(const ItemPtr& pItem);
bool DepositItems(const ItemPtr& pItem);
void SetItemPermissions(const ItemPtr& pItem);
void BarterSearch(int nBarterItems, const char* pszItemName, DWORD MyBarterMinimum, CListWnd* cListInvWnd);
int FindBarterIndex(const char* pszItemName, DWORD MyBarterMinimum, CListWnd* cBuyLineListWnd);
bool SelectBarterSell(int BarterMaximumIndex, CListWnd* cBuyLineListWnd);
void SelectBarterQuantity(int BarterMaximumIndex, const char* pszItemName, CListWnd* cBuyLineListWnd);

DWORD __stdcall SellItems(PVOID pData);
DWORD __stdcall BuyItem(PVOID pData);
DWORD __stdcall DepositPersonalBanker(PVOID pData);
DWORD __stdcall DepositGuildBanker(PVOID pData);
DWORD __stdcall BarterItems(PVOID pData);
