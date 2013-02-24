local ffi = require "ffi"

ffi.cdef [[

typedef enum {
	A_NOTHING = 0,	///< no type, thus no atom
	A_LONG,			///< long integer
	A_FLOAT,		///< 32-bit float
	A_SYM,			///< t_symbol pointer
	A_OBJ,			///< t_object pointer (for argtype lists; passes the value of sym)	
} e_max_atomtypes;

// opaque struct for the host object:
typedef struct t_luajit t_luajit;
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
]]

local lib = ffi.C

assert(this, "global 'this' missing")

-- define a metatype for the host object:
local t_luajit = ffi.metatype("t_luajit", {
	__tostring = function(self)
		return string.format("t_luajit(%p)", self)
	end,
	__index = {
		
	},
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
		
	},
}) 

-- cast 'this' userdata to typed cdata pointer:
this = ffi.typeof("struct t_luajit *")(this)

local max = {}

function max.post(fmt, ...)
	lib.object_post(ffi.cast("t_object *", this), fmt, ...)
end
function max.warn(fmt, ...)
	lib.object_warn(ffi.cast("t_object *", this), fmt, ...)
end
function max.error(fmt, ...)
	lib.object_error(ffi.cast("t_object *", this), fmt, ...)
end

local lua_outlet = lib.object_attr_getobj(this, lib.gensym("lua_outlet"))
assert(lua_outlet, "could not acquire lua outlet")

-- convert a lua type to a t_atom type:
function atom_set(atom, v)
	if type(v) == "number" then
		lib.atom_setfloat(atom, v)
	elseif type(v) == "boolean" then
		lib.atom_setlong(atom, v and 1 or 0)
	elseif type(v) == "string" then
		lib.atom_setsym(atom, lib.gensym(v))
	elseif type(v) == "cdata" or type(v) == "userdata" then
		lib.atom_setobj(atom, v)
	else
		-- what is the right fallback here?
		lib.atom_setsym(atom, lib.gensym(tostring(v)))
	end
end

function outlet(name, ...)
	assert(name and type(name) == "string", "outlet argument 1 must be a string")
	local ac = select("#", ...)
	local av = ffi.new("t_atom[?]", ac)
	for i = 1, ac do
		atom_set(av[i-1], (select(i, ...)))
	end
	lib.outlet_anything(lua_outlet, lib.gensym(name), ac, av)
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

-- add lazy loader:
return setmetatable(max, { 
	__index = function(t, k)
		t[k] = lib[k]
		return lib[k]
	end,
})