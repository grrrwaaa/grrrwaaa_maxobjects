local gl = require "gl"
local GL = gl
local sketch = gl.sketch

local context = {}
context.__index = context

local state = {}
state.__index = state

local path = {}
path.__index = path

local pathInterface = {}

function pathInterface:moveTo(x, y)
	
end

function pathInterface:lineTo(x, y)
	
end

-- ends last path (
function pathInterface:closePath()
	
end

local function make_state()
	return setmetatable({
		-- current drawing state:
		-- transform matrix
		-- clipping region
		-- stroke & fill styles, global alpha, line styles, etc.
		lineWidth = 1,
	}, state)
end

local function make_context()
	return setmetatable({
		-- drawing state stack:
		state_stack = {},
		state = make_state(),
	}, context)
end

-- push drawing state:
function context:save()
	table.insert(self.state_stack, self.state)
	self.state = make_state()
	return self
end

-- pop drawing state:
function context:restore()
	self.state = table.remove(self.state_stack)
	return self
end

function context.line(x1, y1, x2, y2)
	gl.Begin(gl.LINES)
		gl.Vertex(x1, y1, 0)
		gl.Vertex(x2, y2, 0)
	gl.End()
end

function context.draw(f)
	-- go 2D:
	sketch.enter_ortho(0, 0, 100, 100)
	
	local ok, err = pcall(f)
	
	sketch.leave_ortho()
	
	if not ok then error(err) end
end

local canvas = make_context()

return canvas