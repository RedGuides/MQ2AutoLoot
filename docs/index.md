---
tags:
  - plugin
resource_link: "https://www.redguides.com/community/resources/mq2autoloot.87/"
support_link: "https://www.redguides.com/community/threads/mq2autoloot.51451/"
repository: "https://github.com/RedGuides/MQ2AutoLoot"
config: "MQ2AutoLoot.ini, loot.ini"
authors: "plure, knightly, alynel"
tagline: "Control looting automatically"
---

# MQ2AutoLoot
<!--desc-start-->
This plugin is designed to handle loot from within the advanced looting window. It uses a loot.ini file located in your macro folder to define what your characters should do with each item. It can be setup to handle looting if your soloing, in a group, or a raid. If you use the advanced loot list for an item, that action is takes precedence over of this plugin. IE if you have set an item to Always Need/Always Greed/Never/Autoroll that action will be done as opposed to what you have set in your loot.ini. This means that this plugin will only work on items you have not previously specified an action for on the advanced loot list.
<!--desc-end-->

The master looter will keep looting items until their number of free bag slots reaches SaveBagSlots. You can set up to 2 different bag to not count when checking for free bag space with ExcludeBag1/ExcludeBag2. If the master looter can't loot or doesn't want (ie items set to quest/gear) the item he will try to pass it out to others in the group. The rest of the group will either set the item to need or no.

When passing out loot the ML waits DistributeLootDelay (default is 5 seconds) for group members to set need/no before passing out items. If two items comes up it waits the value set by DistributeLootDelay for the first item, then passes it out. After that they go to the next item to be passed out and again wait the value set by DistributeLootDelay before passing out that item. If you have any problems with this taking too long you can shorten DistributeLootDelay.


## Commands

<a href="cmd-autoloot/">
{% 
  include-markdown "projects/mq2autoloot/cmd-autoloot.md" 
  start="<!--cmd-syntax-start-->" 
  end="<!--cmd-syntax-end-->" 
%}
</a>
:    {% include-markdown "projects/mq2autoloot/cmd-autoloot.md" 
        start="<!--cmd-desc-start-->" 
        end="<!--cmd-desc-end-->" 
        trailing-newlines=false 
     %} {{ readMore('projects/mq2autoloot/cmd-autoloot.md') }}

<a href="cmd-setitem/">
{% 
  include-markdown "projects/mq2autoloot/cmd-setitem.md" 
  start="<!--cmd-syntax-start-->" 
  end="<!--cmd-syntax-end-->" 
%}
</a>
:    {% include-markdown "projects/mq2autoloot/cmd-setitem.md" 
        start="<!--cmd-desc-start-->" 
        end="<!--cmd-desc-end-->" 
        trailing-newlines=false 
     %} {{ readMore('projects/mq2autoloot/cmd-setitem.md') }}

### Settings for MQ2AutoLoot.ini

| Setting | Option | Description |
| --- | --- | --- |
| lootini | `<c:\path\Loot.ini>` | The absolute path to your Loot.ini file |
| UseAutoLoot | `[0|1]` | Turns off/on MQ2AutoLoot. Default is on. |

### General Settings for Loot.ini

| Setting | Option | Description |
| --- | --- | --- |
| Raidloot | `[0|1]` | Turns MQ2AutoLoot off/on when you're in a raid. Default is off. |
| SpamLootInfo | `[0|1]` | Turns off/on loot action messages to the MQ chat window. Default is on. |
| LogLoot | `[0|1]` | Turns off/on logging loot action messages for the master looter. Default is off. |
| CursorDelay | `#n` | Time delay in seconds before MQ2AutoLoot auto-inventories items on your cursor. Default is 10 seconds. |
| NewItemDelay | `#n` | Time delay in seconds before the master looter will attempt to loot/distribute items not found in your loot.ini. Default is 0 seconds. |
| DistributeLootDelay | `#n` | Time delay in seconds before the master looter will attempt to distribute items to others in the group/raid. Default is 5 seconds. |
| NoDropDefault | `[quest|keep|ignore]` | Default loot action for new no-drop items. Default is quest. |
| QuestKeep | `#n` | If NoDropDefault is set to quest the quantity to keep will be #n. Default is 10. |
| SaveBagSlots | `#n` | Will stop looting when #n is the number of open bag slots, not including ExcludeBag1/ExcludeBag2. Default is 0. |
| ExcludeBag1 | `<bagname>` | Will exclude bags named XXX when counting open bag slots. Default is Extraplanar Trade Satchel. |
| ExcludeBag2 | `<bagname>` | Will exclude bags named XXX when counting open bag slots. Default is Extraplanar Trade Satchel. |
| BarMinSellPrice | `#n` | The minimum price in plat you will barter all particular items; note you can set the individual item price minimum higher. Default is 1 plat. |
| GuildItemPermission | `[view only|public if usable|public]` | Default permission for items placed in your guild bank. Default is "View Only". |

### Item Settings for Loot.ini

| Setting | Option | Description |
| --- | --- | --- |
| =Keep |  | Everyone will try to loot |
| =Sell |  | Will sell at merchants with */autoloot sell*. During looting, this is treated the same as setting the item to "Keep". |
| =Deposit |  | Will deposit at guild banks with */autoloot deposit*. During looting, this is treated the same as setting the item to "Keep". |
| =Barter | `|#n` | Will barter at the minimum price of #n. e.g. *Batwing Crunchies=Barter\|3* will attempt to barter for a minimum of 3 plat. |
| =Quest | `|#n` | Everyone will try to loot up to #n of that item. |
| =Gear | `|Classes|XXX|YYY|NumberToLoot|#n|` | Only classes listed will attempt to loot till they have #n. Accepts 3-letter class names. Example: *`Batwing Crunchies=Gear|Classes|WIZ|ENC|NEC|MAG|NumberToLoot|1|`* |
| =Ignore |  | Everyone will ignore these items. |
| =Destroy |  | The master looter will try and loot these items; once looted they will pick them out of your inventory and destroy them. |

## Examples

```ini
Black Flame Charcoal=Ignore
Sarnak Earring of Station=Gear|Classes|WAR|ROG|NumberToLoot|2|
Zazuzh's Key=Quest|1
Sapphire Necklace=Sell
```

## Top-Level Objects

## [AutoLoot](tlo-autoloot.md)
{% include-markdown "projects/mq2autoloot/tlo-autoloot.md" start="<!--tlo-desc-start-->" end="<!--tlo-desc-end-->" trailing-newlines=false %} {{ readMore('projects/mq2autoloot/tlo-autoloot.md') }}

## DataTypes

## [AutoLoot](datatype-autoloot.md)
{% include-markdown "projects/mq2autoloot/datatype-autoloot.md" start="<!--dt-desc-start-->" end="<!--dt-desc-end-->" trailing-newlines=false %} {{ readMore('projects/mq2autoloot/datatype-autoloot.md') }}

<h2>Members</h2>
{% include-markdown "projects/mq2autoloot/datatype-autoloot.md" start="<!--dt-members-start-->" end="<!--dt-members-end-->" %}
{% include-markdown "projects/mq2autoloot/datatype-autoloot.md" start="<!--dt-linkrefs-start-->" end="<!--dt-linkrefs-end-->" %}
