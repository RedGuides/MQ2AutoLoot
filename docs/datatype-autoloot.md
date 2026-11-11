---
tags:
  - datatype
---
# `AutoLoot`

<!--dt-desc-start-->
Contains members that return the current status of the autoloot plugin, as well as inventory information.
<!--dt-desc-end-->

## Members
<!--dt-members-start-->
/// member | bool Active
Will return true when you are using MQ2AutoLoot to handle your advanced looting .
///

/// member | bool SellActive
Will return true when your selling your items to a merchant.
///

/// member | bool BuyActive
Will return true when your buying itemat a merchant.
///

/// member | bool DepositActive
Will return true when your depositing your items to a personal/guild banker.
///

/// member | bool BarterActive
Will return true when your bartering your items.
///

/// member | int FreeInventory
Will return the number of empty slots not in excludebag1 or excludebag2
///

<!--dt-members-end-->

<!--dt-linkrefs-start-->
[bool]: ../macroquest/reference/data-types/datatype-bool.md
[int]: ../macroquest/reference/data-types/datatype-int.md
<!--dt-linkrefs-end-->
