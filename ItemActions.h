#define PERSONALBANKER_CLASS		40
#define MERCHANT_CLASS				41
#define GUILDBANKER_CLASS			66

#include <chrono>

using namespace std;
typedef std::chrono::high_resolution_clock pluginclock;

// Variables necessary to deposit/sell/barter/etc items
extern bool							bBarterActive;
extern bool							bDepositActive;
extern bool							bSellActive;
extern bool							bBuyActive;
extern bool							bBarterReset; // Set to true when you need to refresh your barter search
extern bool							bBarterItemSold; // Set to true when you sell an item
extern bool							bEndThreads; // Set to true when you want to end any threads out there, also it is used to enforce that at most a single thread is active at one time
extern PCONTENTS					pItemToPickUp;
extern CXWnd*						pWndLeftMouseUp;
extern pluginclock::time_point		LootThreadTimer;

// Functions necessary to deposit/sell/barter/etc items
void ResetItemActions(void);
bool MoveToNPC(PSPAWNINFO pSpawn);
bool HandleMoveUtils(void);  // Used to connect to MQ2MoveUtils
bool OpenWindow(PSPAWNINFO pSpawn);
bool CheckIfItemIsInSlot(short InvSlot, short BagSlot);
bool WaitForItemToBeSelected(PCONTENTS pItem, short InvSlot, short BagSlot);
bool WaitForItemToBeSold(short InvSlot, short BagSlot);
void SellItem(PCONTENTS pItem);
bool FitInPersonalBank(PITEMINFO pItem);
void PutInPersonalBank(PITEMINFO pItem);
bool CheckGuildBank(PITEMINFO pItem);
bool PutInGuildBank(PITEMINFO pItem);
bool DepositItems(PITEMINFO pItem);
void SetItemPermissions(PITEMINFO pItem);
void BarterSearch(int nBarterItems, CHAR* pzItemName, DWORD	MyBarterMinimum, CListWnd *cListInvWnd);
int FindBarterIndex(CHAR* pszItemName, DWORD MyBarterMinimum, CListWnd *cBuyLineListWnd);
bool SelectBarterSell (int BarterMaximumIndex, CListWnd *cBuyLineListWnd);
void SelectBarterQuantity(int BarterMaximumIndex, CHAR* pszItemName, CListWnd *cBuyLineListWnd);
DWORD __stdcall SellItems(PVOID pData);
DWORD __stdcall BuyItem(PVOID pData);
DWORD __stdcall DepositPersonalBanker(PVOID pData);
DWORD __stdcall DepositGuildBanker(PVOID pData);
DWORD __stdcall BarterItems(PVOID pData);