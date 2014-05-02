local ffi = require "ffi"
local max = require "max"
local gl = require "gl"
local GL = gl

print(string.rep("_", 50))

local t = 0
local dt = math.pi/40

function draw()
	--print("drawing")
	
	gl.Begin(gl.LINES)
	--gl.Color(1, 1, 1, 1)
	for i = 0, math.pi*2, dt do
		gl.Vertex(math.sin(i+t)*math.sin(t/4), math.cos(i+t), math.cos(t/3))
		gl.Vertex(math.sin(i), math.cos(i), 0)
	end
	gl.End()
end

function foo(...)
	print(...)
end

function bang()
	t = t + dt
end

function anything(...)
	print("anything", ...)
end

print("ok")