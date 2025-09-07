---
tags:
  - command
---

# /autoloot

## Syntax

<!--cmd-syntax-start-->
```eqcommand
/autoloot [option] [#|setting]
```
<!--cmd-syntax-end-->

## Description

<!--cmd-desc-start-->
Interact with merchants, bankers, or barter, as well as modify settings
<!--cmd-desc-end-->

## Options

| Option | Description |
|--------|-------------|
| `turn [on|off]` | Toggle autoloot on or off |
| `help` | displays in-game command help |
| `sell` | If you have targeted a merchant, it will sell all the items marked Sell in your inventory |
| `buy <item name> #n` | Will buy #n of "item name" from the merchant |
| `deposit` | If you have your personal banker targeted it will put all items marked Keep into your bank. If you have your guild banker targeted it will put all items marked Deposit into your guild bank |
| `barter` | It will attempt to barter everything marked "Barter&#124;#n" in your Loot.ini as long as they meet your minimum price of #n plat. |
| `sort` | Alphabetically sort the Loot.ini file |
| `status` | Shows current settings for the plugin |
| `reload` | AutoLoot will reload variables from MQ2AutoLoot.ini |
| `lootini <filename>` | Will set your loot ini as FILENAME.ini in your macro folder |
| `spamloot [on|off]` | toggle loot action messages |
| `logloot [on|off]` | Toggle logging of loot actions for the master looter |
| `raidloot [on|off]` | Toggle raid looting on and off |
| `distributedelay #n` | Master looter waits #n seconds to try and distribute the loot |
| `cursordelay #n` | You will wait #n seconds before trying to auto inventory items on your cursor​ |
| `newitemdelay #n` | Master looter waits #n seconds when a new item drops before looting that item​ |
| `barterminimum #n` | The minimum price for all items to be bartered is #n |
| `nodropdefault [quest|keep|ignore]` | default loot action for new no drop items. Default is quest. |
| `excludebag1 XXX` | Will exclude bags named XXX when checking for how many slots you are free |
| `excludebag2 XXX` | Will exclude bags named XXX when checking for how many slots you are free |
| `questkeep #n` | If nodropdefault is set to quest your new no drop items will be set to Quest&#124;#n​ |
| `saveslots #n` | Stops looting when #n slots are left​ |
| `guilditempermission [view only|public if usable|public]` | Change your default permission for items put into your guild bank​ |
