It's a little coroutine library!

Will be closely following the ideas set by the Lua API, but targeting C, with
the idea being that this library can be easily ported to MCUs (Though this
**will** require a stack manipulation porting layer). The main focus is on
usabiliy through minimalism, so don't expect anything more than resume + yield.

Lua coroutines: https://www.lua.org/pil/9.1.html

Intended to work well with its sister event queue library:
https://github.com/geky/equeue

