---
tags:
  - command
---

# /setitem

## Syntax

<!--cmd-syntax-start-->
```eqcommand
/setitem [option] [#]
```
<!--cmd-syntax-end-->

## Description

<!--cmd-desc-start-->
Adds settings for the item on your cursor to Loot.ini, and will react accordingly
<!--cmd-desc-end-->

## Options

| Option | Description |
|--------|-------------|
| `help` | displays help |
| `keep` | sets the item to keep. Will loot. '/AutoLoot deposit' will put these items into your personal banker |
| `deposit` | sets the item to deposit. Will loot. '/AutoLoot deposit' will put these items into your guild banker |
| `sell` | sets the item to sell. Will loot.  '/AutoLoot sell' will sell these items to a merchant. |
| `barter #n` | sets the item to barter. Will loot, and barter if any buyers offer more then #n plat. |
| `quest #n` | Will loot up to #n on each character running this plugin. |
| `gear` | sets the item to "Gear" and auto populates with classes allowed to wear the item along with the number to loot. Will loot if requirements are met. Example entry in the ini: itemname=Gear&#124;Classes&#124;WAR&#124;PAL&#124;SHD&#124;BRD&#124;NumberToLoot&#124;2&#124; |
| `ignore` | Will not loot this item |
| `destroy` | The master looter will attempt to loot and then destroy this item |
| `status` | Will tell you what that item is set to in your loot.ini. |