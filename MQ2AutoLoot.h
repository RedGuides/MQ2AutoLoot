#define	PLUGIN_CHAT_MSG		"\ag[MQ2AutoLoot]\ax "  
#define	PLUGIN_DEBUG_MSG	"[MQ2AutoLoot] "  

using namespace std;
#include <map>
#include <chrono>
#include <string>

typedef std::chrono::high_resolution_clock pluginclock;

// Variables that are setable through the /AutoLoot command
extern int				iUseAutoLoot;
extern int				iSpamLootInfo;
extern int				iCursorDelay;
extern int				iNewItemDelay;
extern int				iDistributeLootDelay;
extern int				iSaveBagSlots;
extern int				iQuestKeep;
extern int				iBarMinSellPrice;
extern int				iLogLoot;
extern char				szNoDropDefault[MAX_STRING];
extern char				szLootINI[MAX_STRING];
extern char				szExcludedBag1[MAX_STRING];
extern char				szExcludedBag2[MAX_STRING];
extern char				szGuildItemPermission[MAX_STRING];
extern char				szTemp[MAX_STRING];
extern char				szCommand[MAX_STRING];
extern char				szLootStuffAction[MAX_STRING];
extern char				szLogPath[MAX_STRING];
extern char				szLogFileName[MAX_STRING];

// Functions necessary for MQ2AutoLoot
bool InGameOK(void);
void SetAutoLootVariables(void);
void CreateLootINI(void);
bool DoThreadAction(void); // Do actions from threads that need to be in the pulse to stop crashing to desktop
void ShowInfo(void); // Outputs the variable settings into your MQ2 Window
bool CheckCursor(void);  // Returns true if an item is on your cursor
bool DestroyStuff(void); // Will find items you loot marked destroy and delete them
bool CheckWindows(bool ItemOnCursor);  // Returns true if your attempting to accept trade requests or click the confirmation box for no drop items
void ClearLootEntries(void); // Clears out the LootEntries from the non-ML toons
bool SetLootSettings(void); // Turn off Auto Loot All
bool HandlePersonalLoot(bool ItemOnCursor, PCHARINFO pChar, PCHARINFO2 pChar2, PEQADVLOOTWND pAdvLoot, CListWnd *pPersonalList, CListWnd *pSharedList); // Handle items in your personal loot window
bool HandleSharedLoot(bool ItemOnCursor, PCHARINFO pChar, PCHARINFO2 pChar2, PEQADVLOOTWND pAdvLoot, CListWnd *pPersonalList, CListWnd *pSharedList); // Handle items in your shared loot window
bool WinState(CXWnd *Wnd);
PMQPLUGIN Plugin(char* PluginName);
bool HandleEQBC(void);  // Used to get EQBC Names
bool AmITheMasterLooter(PCHARINFO pChar, bool* pbMasterLooter); // Determines if the character is the master looter, if it has to set a master looter it instructs the plugin to pause looting
bool ParseLootEntry(bool ItemOnCursor, PCHARINFO pChar, PCHARINFO2 pChar2, PLOOTITEM pLootItem, bool bMasterLooter, char* pszItemAction, bool* pbIWant, bool* pbCheckIfOthersWant); // Parse loot entry for an item and determine an appropriate action
void InitialLootEntry(PLOOTITEM pLootItem, bool bCreateImmediately); // Create the initial loot entry for a new item or one where we don't recognize the loot action as valid
bool DoIHaveSpace(CHAR* pszItemName, DWORD plMaxStackSize, DWORD pdStackSize, bool bSaveBagSlots);
bool FitInInventory(DWORD pdItemSize);
int CheckIfItemIsLoreByID(int ItemID);
DWORD FindItemCount(CHAR* pszItemName);
DWORD __stdcall PassOutLoot(PVOID pData); // Pass out items to the group or the raid
bool DistributeLoot(CHAR* szName, PLOOTITEM pShareItem, LONG ItemIndex);
bool ContinueCheckingSharedLoot(void); // Will stop looping through the shared loot list when the number of items clicked need/greed is equal or exceeds the number of free inventory slots
int AutoLootFreeInventory(void); // used to calculate free inventory
bool DirectoryExists(LPCTSTR lpszPath);
void CreateLogEntry(PCHAR szLogEntry);
void AutoLootCommand(PSPAWNINFO pCHAR, PCHAR szLine);
LONG SetBOOL(long Cur, PCHAR Val, PCHAR Sec = "", PCHAR Key = "", PCHAR INI = "");
void SetItemCommand(PSPAWNINFO pCHAR, PCHAR szLine);
void CreateLootEntry(CHAR* szAction, CHAR* szEntry, PITEMINFO pItem);
int dataAutoLoot(char* szName, MQ2TYPEVAR &Ret);

