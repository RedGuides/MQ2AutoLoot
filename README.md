# MQ2AutoLoot

## Purpose

This plugin is designed to handle loot from within the advanced looting window. It uses a loot.ini file located in your macro folder to define what your characters should do with each item. It can be setup to handle looting if your soloing, in a group, or a raid. If you use the advanced loot list for an item, that action is takes precedence over of this plugin. IE if you have set an item to Always Need/Always Greed/Never/Autoroll that action will be done as opposed to what you have set in your loot.ini. This means that this plugin will only work on items you have not previously specified an action for on the advanced loot list.

The master looter will keep looting items until their number of free bag slots reaches SaveBagSlots. You can set up to 2 different bag to not count when checking for free bag space with ExcludeBag1/ExcludeBag2. If the master looter can't loot or doesn't want (ie items set to quest/gear) the item he will try to pass it out to others in the group. The rest of the group will either set the item to need or no.

When passing out loot the ML waits DistributeLootDelay (default is 5 seconds) for group members to set need/no before passing out items. If two items comes up it waits the value set by DistributeLootDelay for the first item, then passes it out. After that they go to the next item to be passed out and again wait the value set by DistributeLootDelay before passing out that item. If you have any problems with this taking too long you can shorten DistributeLootDelay.

## Usage

1. Turn on advanced looting.  
  Type: /advloot  
  Click the Loot Settings Button on the Advanced Loot Window  
  Select Use Advanced Looting
2. Load the plugin.  
  Type: /plugin MQ2AutoLoot  
3. Set someone to be the master looter.  
  If you don't and you're in a group, the leader of the group will assign himself to be the master looter.  
  If you don't and you're in a raid, the leader of the raid will act as the master looter.  

## Settings

### MQ2AutoLoot.ini

1. lootini= XXX absolute path to your Loot.ini file  
  /AutoLoot lootini FILENAME -> Will set your loot ini as FILENAME.ini in your macro folder  
2. UseAutoLoot=[0/1] turns [off/on] MQ2AutoLoot. Default is on.  
  /AutoLoot turn [on|off] -> Toggle autoloot  

### Loot.ini

#### General Settings

1. RaidLoot= [0/1] turns MQ2AutoLoot [off/on] when your in a raid. Default is off.  
  /AutoLoot raidloot [on|off] -> Toggle raid looting on and off​  
2. SpamLootInfo= [0/1] turns [off/on] loot action messages to the MQ chat window. Default is on.  
  /AutoLoot spamloot [on|off] -> Toggle loot action messages​  
3. LogLoot=[0/1] turns [off/on] logging loot action messages for the master looter. Default is off.  
  /AutoLoot logloot [on|off] -> Toggle logging of loot actions for the master looter​  
4. CursorDelay=#n time delay in seconds before MQ2AutoLoot auto inventory items on your cursor. Default is 10 seconds.  
  /AutoLoot cursordelay #n -> You will wait #n seconds before trying to auto inventory items on your cursor​  
5. NewItemDelay= #n time delay in seconds before the master looter will attempt to loot/distribute items not found in your loot.ini. Default is 0 seconds.  
  /AutoLoot newitemdelay#n -> Master looter waits #n seconds when a new item drops before looting that item​  
6. DistributeLootDelay= #n time delay in seconds before the master looter will attempt to distribute items to others in the group/raid. Default is 5 seconds.  
  /AutoLoot distributedelay #n -> Master looter waits #n seconds to try and distribute the loot​  
7. NoDropDefault= XXX default loot action for new no drop items. Default is quest.  
  /AutoLoot nodropdefault [quest|keep|ignore] -> Will set new no drop items to this value​  
8. QuestKeep= #n if nodropdefault is set to quest the quantity to keep will be #n. Default is 10.  
  /AutoLoot questkeep #n -> If nodropdefault is set to quest your new no drop items will be set to Quest|#n​  
9. SaveBagSlots= #n will stop looting when #n is the number of open bag slots, not including excludedbag1/excludedbag2. Default is 0.  
  /AutoLoot saveslots #n -> Stops looting when #n slots are left​  
10. ExcludeBag1= XXX will exclude bags named XXX when counting open bag slots. Default is Extraplanar Trade Satchel.  
  /AutoLoot excludebag1 XXX -> Will exclude bags named XXX when checking for how many slots you are free​  
11. ExcludeBag2= XXX will exclude bags named XXX when counting open bag slots. Default is Extraplanar Trade Satchel.  
  /AutoLoot excludebag2 XXX -> Will exclude bags named XXX when checking for how many slots you are free​  
12. BarMinSellPrice= #n the minimum price in plat you will barter all particular items, note you can set the individual item price minimum higher. Default is 1 plat.  
  /AutoLoot barterminimum #n -> The minimum price for all items to be bartered is #n​  
13. GuildItemPermission=[view only|public if usable|public]default permission for items placed in your guild bank. Default is "View Only".  
  /AutoLoot guilditempermission "[view only|public if usable|public]" -> Change your default permission for items put into your guild bank​  

#### Item Settings

1. Keep  
  Everyone will try to loot.  
  To set an item to keep. Put it on cursor and type : /SetItem Keep  
2. Sell  
  Everyone will try to loot. During looting, this is treated the same as setting the item to "Keep".  
  To set an item to sell. Put it on cursor and type : /SetItem Sell  
3. Deposit  
  Everyone will try to loot. During looting, this is treated the same as setting the item to "Keep".  
  To set an item to deposit. Put it on cursor and type : /SetItem Deposit  
4. Barter  
  Example entry: Barter|#n  
  Everyone will try to loot. During looting, this is treated the same as setting the item to "Keep".  
  To set an item to barter. Put it on cursor and type : /SetItem Barter #n -> #n is the minimum price to sell that item when you use the barter functionality.  
5. Quest  
  An example entry: Quest|#n  
  Everyone will try to loot up to #n of that item.  
  To set an item to quest. Put it on cursor and type : /SetItem Quest #n -> when someone gets #n of that item they will stop trying to loot them.  
6. Gear  
  An example entry: Gear|Classes|WAR|PAL|SHD|BRD|NumberToLoot|#n|  
  Only classes listed will attempt to loot till they have #n.  
  To set an item to gear. Put it on cursor and type : /SetItem Gear, it will auto populate the entry and number to loot.  
7. Ignore  
  Everyone will ignore these items.  
  To set an item to ignore. Put it on cursor and type : /SetItem Ignore  
8. Destroy  
  The master looter will try and loot these items, once looted they will pick them out of your inventory and destroy them.  
  To set an item to destroy. Put it on cursor and type : /SetItem Destroy  
9. To check what an item is set to in your Loot.ini  
  Put it on cursor and type : /SetItem Status  

### Item Actions

1. Sell  
  Target a Merchant.  
  Type: /AutoLoot Sell  
  The plugin will then sell every item marked "Sell" to that merchant.  
2. Buy  
  Target a Merchant.  
  Type: /AutoLoot Buy "Item Name" #n  
  The plugin will then buy #n of that item from the merchant.  
3. Deposit - Personal Banker  
  Target a personal Banker.  
  Type: /AutoLoot Deposit  
  The plugin will then deposit every item marked "Keep" into your personal banker.  
4. Deposit - Guild Banker  
  Target a Guild Banker.  
  Type: /AutoLoot Deposit  
  The plugin will then deposit every item marked "Deposit " into your guild banker.  
  It will set the permission to either "view only|public if usable|public" depending on what you set "GuildItemPermission" in your Loot.ini.  
5. Barter  
  Type: /AutoLoot Barter  
  It will attempt to barter everything marked "Barter|#n" in your Loot.ini as long as they meet your minimum price of #n plat.  

## TLO

`AutoLoot.Active` -> Will return true when you are using MQ2AutoLoot to handle your advanced looting.  
`AutoLoot.SellActive` -> Will return true when your selling your items to a merchant.  
`AutoLoot.BuyActive` -> Will return true when your buying itemat a merchant.  
`AutoLoot.DepositActive` -> Will return true when your depositing your items to a personal/guild banker.  
`AutoLoot.BarterActive` -> Will return true when your bartering your items.  
`AutoLoot.FreeInventory` -> Will return the number of empty slots not in excludebag1 or excludebag2  