grrrwaaa maxobjects
===================

Objects (externals) for Max/MSP/Jitter 6.0

```git clone``` this into the MaxSDK examples folder. Build from there using the ```./build.rb``` script, or by opening the individual project files.

## Oculus

An object to provide access to the Oculus Rift HMD via the LibOVR SDK. The object reports head orientation as well as the HMD calibration parameters, and the maxhelp patch demonstrates rendering pipeline for the appropriate distortion.  

OSX only currently.

## Luajit

A very generic binding of [LuaJIT](http://www.luajit.org) within a Max object. Supports messages, Jitter matrices, Jitter OpenGL (and raw OpenGL), and offers an FFI interface to the Max API, which is low-level and thus powerful/dangerous. 
Some kind of MSP support is also planned.

OSX only currently.
NOTE: On OSX it will only work if Max is launched in 32-bit mode.