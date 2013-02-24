
Rename to luaav or jit.gl.luaav~ ...?
Or alive or jit.gl.alive~ ...?
Or an or jit.gl.an~ ...?
Or waaa.* version?


Compare some js drawing apis
Paper.js
	descendent of scriptographer
	e.g. project.activeLayer, project.currentStyle, new Path.Rectangle, rect.rotate(), shape = new CompoundPath(rect, circ), path.moveTo, path.lineTo, path.clone, new Group...
Raphael.js
	lightly wraps html5 canvas
	circle(), rect(), ellipse(), path("M x y l x y z"), path.attr({fill: "#9cf"}), shape.animate(attrdict, ms, 'easingstyle'), paper.text(str, x, y), path.toFront(), 
	-> can also animate (tween) path strings!
D3.js
	wraps html5 canvas in the d3 select / apply style
	.append("circle").attr("x", func)
processing.js
	less path-oriented api

Focus on the needs of the class. 
A grid-based rendering system (jit.matrix) for 1D, 2D CA etc.
2D drawing for:
	- various agent systems
	- fractals, turtle, L-system
Go 3D later		
	Displacement maps
	Volumes?
	Shaders...

Text for labeling
Nice colors

Plots for visualizing progress
Plots for graphing dynamical systems

UI controls: keyboard, mouse
Sliders etc?

Space utilities (wrapping, scrolling)
Simple collision for agent systems
Scene graph structures for EvoDevo


Layers:

alive.lua
Provide basic types/interfaces shared by all modules
scheduler

high-level modules
draw.lua (canvas-style wrapper; emulate processing or svg canvas etc.)

low-level modules
gl.lua

max.lua (Host abstraction layer)
Wrapping in alive/lua host methods
Conversions to/from alive/lua and Max types
FFI bindings to Max exported symbols

Audio requires hooking into (possibly also creating) a second thread

