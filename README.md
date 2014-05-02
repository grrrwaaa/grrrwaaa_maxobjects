maxobjects
==========

Objects (externals) for Max/MSP/Jitter

git clone this into the MaxSDK examples folder.
it can be built from there using the build.rb script, or by opening the object files

## luajit

A very generic binding of LuaJIT within a Max object. Supports messages, Jitter matrices, Jitter OpenGL (and raw OpenGL), and offers an FFI interface to the Max API, which is low-level and thus powerful/dangerous. 
Some kind of MSP support is also planned.

NOTE: On OSX it will only work if Max is launched in 32-bit mode.