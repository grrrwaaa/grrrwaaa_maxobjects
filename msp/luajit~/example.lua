local max = require "max"

print(string.rep("_", 50))

local gl = require "gl"
local GL = gl

function draw()
	gl.Begin(GL.QUADS)
	gl.Color(math.random(), math.random(), math.random())
	gl.Vertex(-1,-1,0)
	gl.Vertex(-1,1,0)
	gl.Vertex(1,1,0)
	gl.Vertex(1,-1,0)
	gl.End()
end

