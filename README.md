stated
======

`stated` is a simple daemon designed to augment existing settings and power
management daemons (e.g. `gnome-settings-daemon`) in order to support
the wakelock-centered, opportunistic sleep as found in Android devices.

`stated` is **NOT** meant to replace DE provided daemons, and tries not
to be in their way. If you require a more complete experience, consider
using [mce](https://git.sailfishos.org/mer-core/mce/) or
[repowerd](https://github.com/ubports/repowerd).

Important note
--------------

`stated` is pre-alpha software, much of the code will probably be reworked
in some way or another. Do not try to use it unless you know what you're
doing.

Features
--------

`stated`'s feature set is currently small, and focuses in getting the device
in or out of sleep:

* Enabling opportunistic sleep if supported
* Acquiring or releasing wakelocks depending on display state
* Reacting to the device's powerkey button events

Known issues
------------

* Code can definitely be improved
* Display state detection should be more generic