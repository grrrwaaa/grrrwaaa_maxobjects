local max = require "max"

print(string.rep("_", 50))

local gl = require "gl"
local GL = gl

local canvas = require "canvas"

function draw()
	canvas.draw(function()
	
		canvas.line(0, 0, 10, 10)
		
	end)
end
