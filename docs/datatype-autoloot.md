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
### {{ renderMember(type='bool', name='Active') }}

:   Will return true when you are using MQ2AutoLoot to handle your advanced looting .

### {{ renderMember(type='bool', name='SellActive') }}

:   Will return true when your selling your items to a merchant.

### {{ renderMember(type='bool', name='BuyActive') }}

:   Will return true when your buying itemat a merchant.

### {{ renderMember(type='bool', name='DepositActive') }}

:   Will return true when your depositing your items to a personal/guild banker.

### {{ renderMember(type='bool', name='BarterActive') }}

:   Will return true when your bartering your items.

### {{ renderMember(type='int', name='FreeInventory') }}

:   Will return the number of empty slots not in excludebag1 or excludebag2

<!--dt-members-end-->

<!--dt-linkrefs-start-->
[bool]: ../macroquest/reference/data-types/datatype-bool.md
[int]: ../macroquest/reference/data-types/datatype-int.md
<!--dt-linkrefs-end-->
