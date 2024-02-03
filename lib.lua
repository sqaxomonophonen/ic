local save = {
	require=require,
}
require = function(x)
	print("TODO install reload watch " .. x .. ".lua")
	return save.require(x)
end

local function MKISVECN(n,v)
	return function(v)
		if type(v) ~= "table" then return false end
		if #v ~= n then return false end
		for i=1,n do if type(v[i]) ~= "number" then return false end end
		return true
	end
end
local isvec2 = MKISVECN(2)
local isvec3 = MKISVECN(3)
local isvec4 = MKISVECN(4)

-- removes leading whitespace from every line
local function untab(source)
	local rempatt <const> = "^"..source:match("^%s*")
	local lines = {}
	for line in source:gmatch("[^\r\n]+") do
		local line, n = line:gsub(rempatt, "")
		assert(n == 1)
		table.insert(lines, line)
	end
	return table.concat(lines, "\n")
end

function contains(xs, x)
	local n <const> = #xs
	for i=1,n do if xs[i]==x then return true end end
	return false
end

local mkgen = (function()
	local MT = {
		__index = {
			var = function(self, prefix)
				assert(type(self) == "table")
				assert(type(prefix) == "string")
				local s = self.variable_serials
				if s[prefix] == nil then
					s[prefix] = 0
				end
				local n = prefix .. s[prefix]
				s[prefix] = s[prefix] + 1
				return n
			end,
			line = function(self, line)
				assert(type(self) == "table")
				assert(type(line) == "string")
				table.insert(self.lines, line)
			end,
			linef = function(self, fmt, ...)
				return self:line(string.format(fmt, ...))
			end,
			src = function(self)
				assert(type(self) == "table")
				return table.concat(self.lines, "\n")
			end,
		},
	}
	return function()
		return setmetatable({
			lines = {},
			variable_serials = {},
		}, MT)
	end
end)()

local function NODECLASS(t)
	return setmetatable(t, {
		__index = {},
	})
end

sdf3d    = NODECLASS{ D=3,       F={"map"}}
sdf2d    = NODECLASS{ D=2,       F={"map"}}
tx3d     = NODECLASS{ D=3,       F={"tx","d11"}}
tx2d     = NODECLASS{ D=2,       F={"tx","d11"}}
volumize = NODECLASS{ D=3, DD=2, F={"tx","d11"}}
d11      = NODECLASS{            F={"d11"}}
d21      = NODECLASS{            F={"d21","d2m21"}}

local ST
local function stackpush(D,pvar,def,glsl_argnames)
	table.insert(ST.stack, {
		D=D,
		pvar=pvar,
		def=def,
		glsl_argnames=glsl_argnames,
		dvars={},
	})
end
local function stacktop()
	return ST.stack[#ST.stack]
end
function RESET(dim)
	ST = {
		defset = {},
		src0 = {},
		stack = {},
		mapgen = mkgen(),
	}

	local g = ST.mapgen
	local p0 = g:var("p")
	g:linef("float map(vec%d %s)", dim, p0)
	g:line("{")
	stackpush(dim, p0)
end

function EMIT()
	assert(#ST.stack == 1, "expected one element (root) on stack, got " .. #ST.stack)
	print(table.concat(ST.src0, "\n"))
	local g = ST.mapgen
	g:linef("\treturn %s;", "TODO") -- TODO
	g:line("}")
	print(g:src())
end

function pop(n)
	if n == nil then n = 1 end
	for i=1,n do
		-- XXX TODO chaining can cause n to be larger?
		assert(#ST.stack >= 2, "stack underflow (cannot pop root element)")
		table.remove(ST.stack)
	end
end
function chain()
	error("TODO") -- TODO
end
function endchain()
	error("TODO") -- TODO
end

function DEF(def)
	local name = def[1]
	local name0, name1 = string.gmatch(name, "(%w+):(%w+)")()
	if name0 == nil then
		error("name '" .. name .. "' not in a:b form")
	end

	local nodeclass = _G[name0]
	if nodeclass == nil then
		error("unknown node class '" .. name0 .. "' in '" .. name .. "'")
	end

	def.glsl_names = {}
	for k in pairs(def.glsl or {}) do
		assert(contains(nodeclass.F, k), "unknown glsl function type " .. k)
		def.glsl_names[k] = name0 .. "_" .. name1 .. "_" .. k
	end

	local argfmt = def.argfmt or ""
	local index = getmetatable(nodeclass).__index
	assert(index[name1] == nil, "redefinition of " .. name)
	index[name1] = function(self, ...)
		local args = {...}
		assert(#args == #argfmt, "expected " .. #argfmt .. " argument(s) for " .. name .. "() but got " .. #args)
		local g = ST.mapgen
		local glsl_argnames = {}

		local glsl_argstr = ""
		for i=1,#argfmt do
			local ch = string.sub(argfmt,i,i)
			local arg = args[i]
			local function expect(p, t)
				assert(p, "expected " .. name .. "() arg #" .. i .. " to be " .. t)
			end
			local a = g:var("a")
			table.insert(glsl_argnames, a)
			glsl_argstr = glsl_argstr .. ", " .. a
			if ch == "1" then
				expect(type(arg) == "number", "number")
				g:linef("\tfloat %s = %f;", a, arg)
			elseif ch == "2" then
				expect(isvec2(arg), "vec2")
				g:linef("\tvec2 %s = vec2(%f,%f);", a, arg[1], arg[2])
			elseif ch == "3" then
				expect(isvec3(arg), "vec3")
				g:linef("\tvec3 %s = vec3(%f,%f,%f);", a, arg[1], arg[2], arg[3])
			elseif ch == "4" then
				expect(isvec4(arg), "vec4")
				g:linef("\tvec4 %s = vec4(%f,%f,%f,%f);", a, arg[1], arg[2], arg[3], arg[4])
			else
				error("unhandled argfmt char: " .. ch)
			end
		end

		if ST.defset[name] == nil then
			ST.defset[name] = true
			for k,src in pairs(def.glsl or {}) do
				local fn = def.glsl_names[k]
				local src, nsub = string.gsub(src, "[$]", fn)
				assert(nsub == 1, "expected one $-substitution for " .. fn .. " source")
				src = untab(src)
				table.insert(ST.src0, src)
			end
		end

		local fn_tx = def.glsl_names.tx
		local fn_map = def.glsl_names.map

		local top = stacktop()

		local DC = nodeclass.D
		local D0 <const> = top.D
		local D1 = D0
		local p = top.pvar

		local is_leaf = false

		if fn_tx then
			assert(DC)
			assert(DC == D0, string.format("%dD tx-node in %dD context", DC, D0))
			local D1 = nodeclass.DD or DC
			assert(D1)
			local pn = g:var("p")
			g:linef("\tvec%d %s = %s(%s%s);", D1, pn, fn_tx, p, glsl_argstr)
			p = pn
		end

		if fn_map then
			assert(DC)
			assert(DC == D0, string.format("%dD map-node in %dD context", DC, D0))
			local dn = g:var("d")
			g:linef("\tfloat %s = %s(%s%s);", dn, fn_map, p, glsl_argstr)
			is_leaf = true
			table.insert(top.dvars, dn)
		end

		if not is_leaf then
			stackpush(D1, p, def, glsl_argnames)
		end
	end
end

DEF{
	"sdf3d:sphere",
	argfmt = "1",
	glsl = { map = [[
	float $(vec3 p, float r)
	{
		return length(p)-r;
	}
	]] },
}

DEF{
	"tx3d:translate",
	argfmt="3",
	glsl = { tx = [[
	vec3 $(vec3 p, vec3 r)
	{
		return p+r;
	}
	]] },
}

DEF{
	"tx3d:scale",
	argfmt = "1",
	glsl = {
		tx = [[
		vec3 $(vec3 p, float s)
		{
			return p/s;
		}
		]],
		d11 = [[
		float $(vec3 _p, float d, float s)
		{
			return d*s;
		}
	]],
	},
}

DEF{
	"d11:round",
	argfmt = "1",
	glsl = { d11 = [[
	float $(float d, float r)
	{
		return d-r;
	}
	]] },
}

DEF{
	"d21:union",
	glsl = {
		d21 = [[
		float $(float d0, float d1)
		{
			return min(d0, d1);
		}
		]],
		d2m21 = [[
		MAT $(float d0, float d1, MAT m0, MAT m1)
		{
			return d0 < d1 ? m0 : m1;
		}
		]],
	},
}

DEF{
	"d21:smooth_union",
	argfmt = "1",
	glsl = {
		d21 = [[
		float $(float d0, float d1, float k)
		{
			float h = clamp(0.5 + 0.5*(d1-d0)/k, 0.0, 1.0);
			return mix(d1, d0, h) - k*h*(1.0-h);
		}
		]],
	},
}

DEF{
	"volumize:extrude",
	argfmt = "1",
	glsl = {
		tx = [[
		vec2 $(vec3 p, float h)
		{
			return p.xy;
		}
		]],
		d11 = [[
		float $(vec3 p, float d, float h)
		{
			vec2 w = vec2(d, abs(p.z) - h);
			return min(max(w.x,w.y),0.0) + length(max(w,0.0));
		}
		]],
	},
}


DEF{
	"volumize:revolve",
	argfmt = "1",
	glsl = {
		tx = [[
		vec2 $(vec3 p, float o)
		{
			return vec2(length(p.xz) - o, p.y);
		}
		]],
	},
}
