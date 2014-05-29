local ffi = require "ffi"
local format = string.format

local gl = require "gl"
local sketch = gl.sketch

ffi.cdef [[

static const int JIT_MATRIX_MAX_DIMCOUNT = 32;                                          
static const int JIT_MATRIX_MAX_PLANECOUNT = 32;

typedef enum {
	A_NOTHING = 0,	///< no type, thus no atom
	A_LONG,			///< long integer
	A_FLOAT,		///< 32-bit float
	A_SYM,			///< t_symbol pointer
	A_OBJ,			///< t_object pointer (for argtype lists; passes the value of sym)	
	A_DEFLONG,		///< long but defaults to zero
	A_DEFFLOAT,		///< float, but defaults to zero
	A_DEFSYM,		///< symbol, defaults to ""
	A_GIMME,		///< request that args be passed as an array, the routine will check the types itself.
	A_CANT,			///< cannot typecheck args
	A_SEMI,			///< semicolon
	A_COMMA,		///< comma
	A_DOLLAR,		///< dollar
	A_DOLLSYM,		///< dollar
	A_GIMMEBACK
} e_max_atomtypes;

// opaque struct for the host object:
typedef struct t_object t_object;

// currently assumes 32-bit max:
typedef long t_ptr_int;
typedef t_ptr_int t_atom_long;
typedef t_atom_long t_max_err;

typedef float t_atom_float;

typedef struct {
	char *s_name;
	t_object * s_thing;	
} t_symbol;

union word {
	t_atom_long w_long;	
	t_atom_float w_float;	
	t_symbol * w_sym;
	t_object * w_obj;
};
typedef struct {
	short			a_type;	
	union word		a_w;
} t_atom;

typedef t_atom_long t_jit_err;
typedef struct t_jit_matrix t_jit_matrix;

typedef struct {
	long			size;			///< in bytes (0xFFFFFFFF=UNKNOWN)
	t_symbol		*type;			///< primitifve type (char, long, float32, or float64)
	long			flags;			///< flags to specify data reference, handle, or tightly packed
	long			dimcount;		///< number of dimensions
	long			dim[JIT_MATRIX_MAX_DIMCOUNT];		///< dimension sizes
	long			dimstride[JIT_MATRIX_MAX_DIMCOUNT]; ///< stride across dimensions in bytes
	long			planecount;		///< number of planes
} t_jit_matrix_info;

typedef struct {
	long 	flags;									///< flags for whether or not to use interpolation, or source/destination dimensions
	long	planemap[JIT_MATRIX_MAX_PLANECOUNT];	///< plane mapping
	long	srcdimstart[JIT_MATRIX_MAX_DIMCOUNT];	///< source dimension start	
	long	srcdimend[JIT_MATRIX_MAX_DIMCOUNT];		///< source dimension end
	long	dstdimstart[JIT_MATRIX_MAX_DIMCOUNT];	///< destination dimension start
	long	dstdimend[JIT_MATRIX_MAX_DIMCOUNT];		///< destination dimension end
} t_matrix_conv_info;

t_max_err atom_setlong(t_atom *a, t_atom_long b);	
t_max_err atom_setfloat(t_atom *a, double b);
t_max_err atom_setsym(t_atom *a, t_symbol *b);	
t_max_err atom_setobj(t_atom *a, void *b);

t_atom_long atom_getlong(const t_atom *a);
t_atom_float atom_getfloat(const t_atom *a);
t_symbol *atom_getsym(const t_atom *a);
void *atom_getobj(const t_atom *a);

t_symbol * gensym(const char * s);
void post(const char *fmt, ...);	

void *outlet_anything(void *o, t_symbol *s, short ac, t_atom *av);

void object_post(t_object *x, const char *s, ...);
void object_warn(t_object *x, const char *s, ...);
void object_error(t_object *x, const char *s, ...);

t_atom_long object_attr_getlong(void *x, t_symbol *s);
t_symbol * object_attr_getsym(void *x, t_symbol *s);
t_object * object_attr_getobj(void *x, t_symbol *s);

t_max_err object_notify(void *x, t_symbol *s, void *data);


// Jitter object model
void * jit_object_new(t_symbol *classname, ...);	
t_jit_err jit_object_free(void *x);
void *jit_object_method(void *x, t_symbol *s, ...);
void *jit_object_method_typed(void *x, t_symbol *s, long ac, t_atom *av, t_atom *rv);
t_symbol * jit_object_classname(void * x);
long jit_object_classname_compare(void * x, t_symbol *s);
void * jit_object_register(void *x, t_symbol *s);
void * jit_object_findregistered(t_symbol *s);
t_symbol * jit_object_findregisteredbyptr(void * x);
t_jit_err jit_object_unregister(void * 	x);
t_jit_err jit_object_notify(void * x, t_symbol * s, void * data);	
void * jit_object_attr_get (void *x, t_symbol *attrname);

t_symbol * jit_symbol_unique();

// matrix routines... not exported...
//t_jit_matrix * jit_matrix_new (t_jit_matrix_info *info);
//t_jit_matrix * 	jit_matrix_newcopy (t_jit_matrix *copyme);
//t_jit_err 	jit_matrix_free (t_jit_matrix *x);
//t_jit_err 	jit_matrix_setinfo(t_jit_matrix *x, t_jit_matrix_info *info); 
//t_jit_err 	jit_matrix_setinfo_ex (t_jit_matrix *x, t_jit_matrix_info *info);
//t_jit_err 	jit_matrix_getinfo(t_jit_matrix *x, t_jit_matrix_info *info);
//t_jit_err 	jit_matrix_getdata (t_jit_matrix *x, void **data);
//t_jit_err 	jit_matrix_data (t_jit_matrix *x, void *data);
//t_jit_err 	jit_matrix_freedata (t_jit_matrix *x); 
//t_jit_err 	jit_matrix_info_default (t_jit_matrix_info *info);
//t_jit_err 	jit_matrix_clear (t_jit_matrix *x);
//t_jit_err 	jit_matrix_setcell1d (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv); 
//t_jit_err 	jit_matrix_setcell2d (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);
//t_jit_err 	jit_matrix_setcell3d (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);
//t_jit_err 	jit_matrix_setplane1d (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);
//t_jit_err 	jit_matrix_setplane2d (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);
//t_jit_err 	jit_matrix_setplane3d (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);
//t_jit_err 	jit_matrix_setcell (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);
//t_jit_err 	jit_matrix_getcell (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv, long *rac, t_atom **rav);
//t_jit_err 	jit_matrix_setall (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);
//t_jit_err 	jit_matrix_fillplane (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);
//t_jit_err 	jit_matrix_frommatrix (t_jit_matrix *dst_matrix, t_jit_matrix *src_matrix, t_matrix_conv_info *mcinfo);
//t_jit_err 	jit_matrix_op (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);
//t_jit_err 	jit_matrix_exprfill (t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);
//t_jit_err 	jit_matrix_jit_gl_texture(t_jit_matrix *x, t_symbol *s, long argc, t_atom *argv);

/// LUAJIT HOST BINDING

typedef struct {
	
	int (*draw)();
	void (*anything)(t_symbol *s, long argc, t_atom *argv);
	void (*bang)();
} t_luajit_handlers;

]]

local lib = ffi.C



-- automatic caching in gensym:
local symcache = {}
local function cachesym(s)
	local r = lib.gensym(s)
	symcache[s] = r
	return r
end
local function gensym(s)
	assert(s and type(s) == "string", "gensym requires a string argument")
	return symcache[s] or cachesym(s)
end

-- convert a lua type to a t_atom type:
local function atom_set(atom, v)
	local tv = type(v)
	if tv == "number" then
		lib.atom_setfloat(atom, v)
	elseif tv == "boolean" then
		lib.atom_setlong(atom, v and 1 or 0)
	elseif tv == "string" then
		lib.atom_setsym(atom, gensym(v))
	elseif tv == "cdata" or tv == "userdata" then
		lib.atom_setobj(atom, v)
	else
		-- what is the right fallback here?
		lib.atom_setsym(atom, gensym(tostring(v)))
	end
	return self
end
local function atom_get(atom)
	local ta = atom.a_type
	if ta == lib.A_NOTHING then
		return nil
	elseif ta == lib.A_LONG then
		return lib.atom_getlong(atom)
	elseif ta == lib.A_FLOAT then
		return lib.atom_getfloat(atom)
	elseif ta == lib.A_SYM then
		return ffi.string(lib.atom_getsym(atom).s_name)
	elseif ta == lib.A_OBJ then
		-- f' knows what kind of data it is... 
		return lib.atom_getobj(atom)
	else
		error("unhandled atom type " .. ta)
		return ta
	end
end

-- cast 'this' userdata to typed cdata pointer:
assert(this, "global 'this' missing")
this = ffi.typeof("struct t_object *")(this)

-- METATYPES --

local t_object = {
	__tostring = function(self)
		return format("t_object(%p)", self)
	end,
	
	__index = {
		post = lib.object_post,
		warn = lib.object_warn,
		error = lib.object_error,
		
		attr_getobj = function(self, name)
			return lib.object_attr_getobj(self, gensym(name))
		end,
		
		notify = function(self, name, dataptr)
			return lib.object_notify(self, gensym(name), dataptr)
		end,
	}
}
ffi.metatype("t_object", t_object)

-- actually we probably want to create a wrapper...
local t_jit_matrix = {
	__tostring = function(self)
		return format("jit_matrix(%p)", self)
	end,
}
t_jit_matrix.__index = t_jit_matrix

function t_jit_matrix:lock()
	return lib.jit_object_method(self, gensym("sym_lock"), 1)
end

function t_jit_matrix:unlock(savelock)
	assert(savelock, "unlock() requires an argument (the value previously returned by lock())")
	lib.jit_object_method(self, gensym("sym_lock"), savelock)
	return self
end

function t_jit_matrix:data() 
	local dataptr = ffi.new("char *[1]")
	lib.jit_object_method(self, gensym("getdata"), dataptr)
	return dataptr[0]
end

function t_jit_matrix:clear() 
	lib.jit_object_method(self, gensym("clear"))
	return self
end

function t_jit_matrix:info()
	local info = ffi.new("t_jit_matrix_info[1]")
	lib.jit_object_method(self, gensym("getinfo"), info)
	
	info = {
		-- the meta-properties in a more Lua-friendly form:
		size = info[0].size,
		type = tostring(info[0].type),
		flags = info[0].flags,
		dimcount = info[0].dimcount,
		dim = info[0].dim,
		dimstride = info[0].dimstride,
		planecount = info[0].planecount,
		
		-- the raw byte pointer:
		--bp = ffi.new("char *", dataptr[0]),
			
		-- cached to prevent gc:
		_raw = info,	
	}
	
	return info
end

ffi.metatype("t_jit_matrix", t_jit_matrix)

local jit_matrix = {}
jit_matrix.__index = jit_matrix

local
function jit_matrix_new(name, planecount, typename, ...)
	-- name is optional:
	if type(name) ~= "string" then
		-- generate a new unique name:
		return jit_matrix_new(
			tostring(lib.jit_symbol_unique()),
			name, planecount, typename, ...
		)
	end
	-- does this matrix already exist?
	local m = ffi.cast("t_jit_matrix *", max.jit_object_findregistered(gensym(name)))
	if m == nil then
		-- allocate
	elseif planecount then
		-- reconfigure
	end
	
end

setmetatable(jit_matrix, {
	__call = function(mt, name, planecount, typename, ...)
		local dims = {...}
		
		
		
	end,
})

local t_atom = ffi.metatype("t_atom", {
	__tostring = function(self)
		local a_type = self.a_type
		local value = "nil"
		if a_type == lib.A_LONG then
			value = tostring(self.a_w.w_long)	
		elseif a_type == lib.A_FLOAT then
			value = tostring(self.a_w.w_float)	
		elseif a_type == lib.A_SYM then
			value = ffi.string(self.a_w.w_sym.s_name)
		elseif a_type == lib.A_OBJ then
			value = string.format("%p", self.a_w.w_obj)
		end
		return string.format("t_atom(%p,%s)", self, value)
	end,
	__index = {
		-- atom:set(<lua value>)
		set = atom_set,
		setlong = lib.atom_setlong,
		setfloat = lib.atom_setfloat,
		setsym = function(self, v) lib.atom_setsym(self, gensym(v)) end,
		setobj = lib.atom_setobj,
		-- <lua value> = atom:get()
		get = atom_get,
	},
}) 

local t_symbol = {}
function t_symbol:__tostring()
	return ffi.string(self.s_name)
end
ffi.metatype("t_symbol", t_symbol)

local t_jit_matrix_info = {}
function t_jit_matrix_info:__tostring()
	return format("t_jit_matrix_info(%p)", self)
end
ffi.metatype("t_jit_matrix_info", t_jit_matrix_info)

local max = {
	gensym = gensym,
	
	post = function(fmt, ...) this:post(fmt, ...) end,
	warn = function(fmt, ...) this:warn(fmt, ...) end,
	error = function(fmt, ...) this:error(fmt, ...) end,
	
	jit_matrix = jit_matrix,
}

-- actually probably want to return a wrapper rather than the raw pointer
function max:getmatrix(name)
	local m = ffi.cast("t_jit_matrix *", max.jit_object_findregistered(gensym(name)))
	if m == nil then
		-- default properties:
		local info = ffi.new("t_jit_matrix_info")
		info.planecount = 1
		info.type = gensym("float32")
		info.dimcount = 2
		info.dim[0] = 32
		info.dim[1] = 32
		
		-- create & register it:
		local m = ffi.cast("t_jit_matrix *", lib.jit_object_new(gensym("jit_matrix"), info))
		m = max.jit_object_register(m, gensym(name))
		
		--[[
		local info = ffi.new("t_jit_matrix_info[1]")
		local dataptr = ffi.new("char *[1]")
		max.jit_object_method(m, max.gensym("getinfo"), info)
		max.jit_object_method(m, max.gensym("getdata"), dataptr)
		local meta = {
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
		
		print("matrix", meta.type, meta.planecount, meta.dim[0], meta.dim[1], meta._raw[0].size)
		--]]
		
		-- TODO: this should install a gc handler from Lua to jit_object_free(m)
	end
	return m
end

local lua_outlet = this:attr_getobj("lua_outlet")
assert(lua_outlet ~= nil, "could not acquire lua outlet")

function outlet(name, ...)
	assert(name and type(name) == "string", "outlet argument 1 must be a string")
	local ac = select("#", ...)
	local av = ffi.new("t_atom[?]", ac)
	for i = 1, ac do
		av[i-1]:set(select(i, ...))
	end
	lib.outlet_anything(lua_outlet, gensym(name), ac, av)
end

-- overload some globals:
function print(...)
	local args = {}
	for i = 1, select("#", ...) do 
		-- TODO verify that this will escape printf-style fmt symbols?
		args[i] = tostring((select(i, ...))):gsub("%%", "%%")
	end
	lib.post(table.concat(args, " "))
end

local lua_handlers = ffi.cast("t_luajit_handlers *", this:attr_getobj("lua_handlers"))
assert(lua_handlers ~= nil, "could not acquire lua handlers")

assert(app == nil, "app already defined!")
app = {
	
}

-- override C handlers:
lua_handlers.draw = function()
	if _G.draw and type(_G.draw) == "function" then
		local ok, err = xpcall(_G.draw, debug.traceback)
		if not ok then 
			this:error(err) 
			return 1
		end
	end
	return 0
end

lua_handlers.anything = function(symbol, argc, argv)
	local s = tostring(symbol)
	local f = _G[s]
	if f and type(f) == "function" then
		local ok, err
		if argc > 1 then
			local args = {}
			for i = 1, argc do
				args[i] = atom_get(argv[i-1])
			end
			ok, err = xpcall(function() f(unpack(args)) end, debug.traceback)
		elseif argc == 1 then
			ok, err = xpcall(function() f(atom_get(argv[0])) end, debug.traceback)
		else
			ok, err = xpcall(f, debug.traceback)
		end
		if not ok then 
			this:error(err)
		end
	else
		f = _G.anything
		if f and type(f) == "function" then
			local ok, err = xpcall(function() f(s, argc, argv) end, debug.traceback)
			if not ok then 
				this:error(err)
			end
		end
	end
end

lua_handlers.bang = function()
	local f = _G.bang
	if f and type(f) == "function" then
		local ok, err = xpcall(f, debug.traceback)
		if not ok then 
			this:error(err)
		end
	end
end

-- add lazy loader:
return setmetatable(max, { 
	__index = function(t, k)
		t[k] = lib[k]
		return lib[k]
	end,
})