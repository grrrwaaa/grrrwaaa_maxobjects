local ffi = require "ffi"
local max = require "max"

math.randomseed(os.time())

print(string.rep("_", 50))

local gl = require "gl"
local GL = gl

outlet("hellO")

local canvas = require "canvas"

--[[
local info = ffi.new("t_jit_matrix_info[1]")
max.jit_matrix_info_default(info)
print(info[0].dimcount, info[0].type)
local m1 = max.jit_object_new(max.gensym("jit_matrix"), info)
print(m1)
m1 = max.jit_object_register(m1, max.gensym("m1"))
print(m1)
--]]

local angle = 0
local radius = 5

function draw()
	canvas.draw(function()
	
		-- this is still pretty gross...
		-- another option is to do all data processing in Lua FFI arrays, 
		-- and just use copy methods to/from jitter matrices.
		
		-- or provide a luajit wrapper for matrices that can optionally be a facade for jit.matrix?
		
		-- maybe also assert() if the matrix is not tightly packed?
		
		-- grab the matrix:
		local m1 = max:getmatrix("m1")
		-- lock it:
		local savelock = m1:lock()
		
		-- get matrix data:
		local info = ffi.new("t_jit_matrix_info[1]")
		local dataptr = ffi.new("char *[1]")
		max.jit_object_method(m1, max.gensym("getinfo"), info)
		max.jit_object_method(m1, max.gensym("getdata"), dataptr)
		local m = {
			-- the meta-properties in a more Lua-friendly form:
			type = tostring(info[0].type),
			planecount = info[0].planecount,
			dimcount = info[0].dimcount,
			dim = info[0].dim,
			stride = info[0].dimstride,
			
			-- the raw byte pointer:
			bp = ffi.new("char *", dataptr[0]),
			
			-- cached to prevent gc:
			_raw = info,	
		}
			
		-- for now only process 2D float matrices
		if m.bp ~= nil and m.type == "float32" and m.dimcount == 2 then
			local w, stridex = m.dim[0], m.stride[0]
			local h, stridey = m.dim[1], m.stride[1]
			
			-- cloud effect:
			local wscale = math.pi * 0.5/w
			local hscale = math.pi * 0.5/h
			
			---[[
			for i = 1, w*h*0.25 do
			
				-- read cell:
				local x = math.random(w)-1
				local y = math.random(h)-1
				local cell = ffi.cast("float *", m.bp + y*stridey + x*stridex)
				local v = cell[0] 
				
				-- change wind direction:
				angle = angle + 0.003*(math.random()*2-1)
				local rx = radius * math.cos(angle+i)
				local ry = radius * math.sin(angle+i)
				--print(rx, ry)
				
				-- write cell:
				local x = (x + math.random(rx)) % w
				local y = (y + math.random(ry)) % h
				local cell = ffi.cast("float *", m.bp + y*stridey + x*stridex)
				cell[0] = v
				
			end
			--]]
			
			---[[
			-- iterate all cells:
			-- per row y:
			for y = 0, h-1 do
				local bpy = m.bp + y*stridey
				-- per column x:
				for x = 0, w-1 do
					local bpx = bpy + x*stridex
					-- per plane p...
					local p = 0
					local cell = ffi.cast("float *", bpx)
					-- operate!
					cell[p] = math.max(0, math.min(1, cell[p] + 0.02*(math.random()*2-1)))
				end
			end
			--]]
		end
		
		-- unlock it:
		m1:unlock(savelock)
			
		--[[
		for i = 1, 100 do
			canvas.line(math.random(100), math.random(100), math.random(100), math.random(100))
		end
		--]]
		
		-- this is how to output a matrix:
		--outlet("matrix", "jit_matrix", "m1")
	end)
end
