import os, sys
import gc

def _untab(txt):
	while len(txt) > 0 and txt[0] == "\n": txt = txt[1:]
	n = 0
	for c in txt:
		if c != "\t": break
		n += 1
	lines = txt.split("\n")
	out = []
	for line in lines:
		assert line[0:n].strip() == "", "bad input; not untabbable"
		out.append(line[n:])
	return "\n".join(out)

def gcreport(): return repr(gc.get_count()) # XXX not sure this is reliable; I see static counts even though I deliberately "leak" objects?

def watchlist():
	home = os.path.dirname(os.path.realpath(__file__))
	fs = []
	for name,module in sys.modules.items():
		if not hasattr(module, "__file__"): continue
		f = module.__file__
		if not f.startswith(home): continue
		fs.append(f)
	return fs
#print(watchlist())

_views = []
_viewset = set()
def viewlist(): return _views

_active_codegen = None
_active_mset = None
class _Codegen:
	def __init__(self):
		self.define_set = set()
		self.defines = []
		self.fns = []
		self.onceset = set()

	def once(self, x):
		if x in self.onceset:
			return False
		else:
			self.onceset.add(x)
			return True

	def push(self, node):
		self.stack.append(node)

	def pop(self):
		return self.stack.pop()

	def top(self):
		return self.stack[-1]

	def defined(self, name):
		return name in self.define_set

	def define(self, name, src):
		self.define_set.add(name)
		self.defines.append(src)

	def enter(self):
		self.ident_serials = {}
		self.const_map = {}
		self.stack = []
		self.lines = []

	def leave(self):
		assert len(self.stack) == 1, "expected stack to contain only root node"
		top = _cg().top()
		if hasattr(top, "mvar"):
			self.line("\tout_material = %s;" % top.mvar)
		if hasattr(top, "dvar"):
			self.line("\treturn %s;" % top.dvar)
		self.line("}")
		self.fns.append("\n".join(self.lines))
		self.lines = None
		self.stack = None
		self.const_map = None
		self.ident_serials = None

	def enter_map(self, fn, dim):
		self.enter()
		pvar = self.ident("p")
		mxtra = ""
		mxtra = ", out Material out_material"
		self.line("float %s(vec%d %s%s)" % (fn, dim, pvar, mxtra))
		self.line("{")
		self.push(_RootNode(pvar, dim))

	def source(self):
		return ("\n".join(self.defines)) + "\n\n" + ("\n".join(self.fns))

	def line(self, line):
		self.lines.append(line)

	def ident(self, prefix):
		if prefix not in self.ident_serials:
			self.ident_serials[prefix] = -1
		self.ident_serials[prefix] += 1
		return "%s%d" % (prefix, self.ident_serials[prefix])

	def constant(self, typ, literal):
		if literal not in self.const_map:
			i = self.ident("c")
			self.line("\t%s %s = %s;" % (typ, i, literal))
			self.const_map[literal] = i
		return self.const_map[literal]

def _cg():
	assert _active_codegen is not None, "codegen attempted outside of codegen scope"
	return _active_codegen

_wpp_todo = []
def _wpp_flush():
	# XXX monkey patching a _WithWithoutParentheses wrapper around classes that
	# request it by adding themselves to _wpp_todo in __init_subclass__. after
	# _wpp_flush() the class is replaced with an _WithWithoutParentheses
	# instance.
	global _wpp_todo
	for w in _wpp_todo:
		mod = sys.modules[w.__module__]
		nam = w.__name__
		setattr(mod, nam, _WithWithoutParentheses(w))
	_wpp_todo = []

class _ViewGen:
	def __init__(self, source):
		self.source = source

class MaterialSet:
	ff = [
		("albedo", "vec3"),
	]
	def __init__(self, z):
		self.z = z

	def empty(self):
		return len(self.z) == 0

	def mktype(self):
		fields = ""
		for (nam,typ) in self.ff:
			if nam in self.z: fields += ("\t%s %s;\n" % (typ, nam))
		return "struct Material {\n" + fields + "};\n"

	def format(self, material):
		s = "Material("
		for (nam,typ) in self.ff:
			if not nam in self.z: continue
			a = None
			if hasattr(material, nam):
				a = getattr(material, nam)

			if typ == "vec3":
				x = "vec3(0.0,0.0,0.0)"
				if a is not None:
					x = "vec3(%f,%f,%f)" % a
				s += x
			else:
				assert False, "no handler for typ=%s" % typ
		s += ")"
		return s

class _View:
	def __init__(self, dim, name, ctor):
		self.dim = dim
		self.name = name
		self.ctor = ctor

	def __call__(self):
		_wpp_flush()
		global _active_codegen, _active_mset
		_active_codegen = _Codegen()
		_active_mset = MaterialSet(set(("albedo",)))
		if not _active_mset.empty():
			_active_codegen.define("Material", _active_mset.mktype())
		_active_codegen.enter_map("map", self.dim)
		self.ctor()
		_active_codegen.leave()
		source = _active_codegen.source()
		print(source)
		_active_codegen = None
		_active_mset = None
		return _ViewGen(source)

def _register_view(dim, ctor):
	name = ctor.__name__
	assert name not in _viewset, "view %s is already defined" % name
	_viewset.add(name)
	view = _View(dim, name, ctor)
	_views.append(view)
	return view

def view2d(ctor): return _register_view(2,ctor)
def view3d(ctor): return _register_view(3,ctor)

# allow chains of _Scope nodes
class chain:
	def __init__(self, *xs):
		self.xs = list(filter(lambda x: x is not None, xs))

	def __enter__(self):
		for x in self.xs: x.__enter__()

	def __exit__(self,type,value,tb):
		for x in reversed(self.xs): x.__exit__(type,value,tb)

# used to make `with foo: ...` equivalent to `with foo(): ...`, either as an
# annotation (@_WithWithoutParentheses), or as a subclass using
# __init_subclass__/_wpp_todo/_wpp_flush
class _WithWithoutParentheses:
	def __init__(self, v):
		self.v = v
		self.stack = []

	def __enter__(self):
		i = self.v()
		self.stack.append(i)
		i.__enter__()

	def __exit__(self,type,value,tb):
		self.stack.pop().__exit__(type,value,tb)

	def __call__(self):
		return self.v()

_isnum  = lambda v: isinstance(v,(float,int))
_isvecn = lambda n,v: (len(v)==n) and (False not in [_isnum(x) for x in v])

class _Node:
	argfmt = ""

	@classmethod
	def nname(t):
		return t.__name__

	@classmethod
	def typd(t):
		cg = _cg()
		if not cg.once(t): return

		n_resolved = 0
		def resolv(fn):
			nam = "glsl_%s" % fn
			if not hasattr(t, nam): return None
			r = "%s_%s" % (t.nname(), fn)
			if not cg.defined(r):
				cg.define(r, _untab(getattr(t, nam) % {"fn": r}))
				nonlocal n_resolved
				n_resolved += 1
			return r

		t.fn_d21 = resolv("d21")
		t.fn_d11 = resolv("d11")
		t.fn_p22 = resolv("p22")
		t.fn_p33 = resolv("p33")
		t.fn_p2d1 = resolv("p2d1")
		t.fn_p3d1 = resolv("p3d1")
		node_is_2d = t.fn_p22 or t.fn_p2d1
		node_is_3d = t.fn_p33 or t.fn_p3d1
		assert not (node_is_2d and node_is_3d), "mixed dimensions in definition"
		t.fn_tx = t.fn_p22 or t.fn_p33
		t.fn_map = t.fn_p2d1 or t.fn_p3d1
		t.is_leaf = bool(t.fn_map)
		t.dim = (node_is_2d and 2) or (node_is_3d and 3) or 0

	def rjoin(self, o):
		cg = _cg()
		if hasattr(o, "dvar"):
			dvar1 = o.dvar

			if not hasattr(self, "dvar"):
				self.dvar = dvar1
			else:
				dvar2 = cg.ident("d")
				type(self).typd()
				if self.fn_d21:
					cg.line("\tfloat %s = %s(%s, %s%s);" % (dvar2, self.fn_d21, self.dvar, dvar1, self.glsl_argstr))
				else:
					u = union.v
					u.typd()
					cg.line("\tfloat %s = %s(%s, %s);" % (dvar2, u.fn_d21, self.dvar, dvar1))
				self.dvar = dvar2

			if hasattr(o, "mvar"):
				mvar1 = o.mvar
				if not hasattr(self, "mvar"):
					self.mvar = mvar1
				elif self.mvar != mvar1:
					mvar2 = cg.ident("m")
					cg.line("\tMaterial %s = %s < %s ? %s : %s;" % (mvar2, self.dvar, dvar1, self.mvar, mvar1))
					self.mvar = mvar2

	def __init__(self):
		pass

	def exec(self, args):
		type(self).typd()
		cg = _cg()

		#print(self.name(), args, "argfmt", self.argfmt)
		argfmt = self.argfmt
		n = len(argfmt)
		if n == 1:
			c = argfmt[0]
			if (c == "2" and len(args) == 2) or (c == "3" and len(args) == 3) or (c == "4" and len(args) == 4):
				args = [args]
		if len(args) != n: raise RuntimeError("invalid number of arguments; wanted %d; got %d" % (n, len(args)))
		glsl_argstr = ""
		for i in range(n):
			c = argfmt[i]
			a = args[i]
			tt = None
			lit = None
			if c == "1":
				assert _isnum(a), "argument %d not a number" % i
				tt = "float"
				lit = "%f" % a
			elif c == "2":
				assert _isvecn(2,a), "argument %d not a vec2" % i
				tt = "vec2"
				lit = "vec2(%f,%f)" % a
			elif c == "3":
				assert _isvecn(3,a), "argument %d not a vec3" % i
				tt = "vec3"
				lit = "vec3(%f,%f,%f)" % a
			elif c == "4":
				assert _isvecn(4,a), "argument %d not a vec4" % i
				tt = "vec4"
				lit = "vec4(%f,%f,%f,%f)" % a
			else:
				raise RuntimeError("unhandled argfmt char %s" % repr(c))
			glsl_argstr += ", %s" % cg.constant(tt,lit)
		self.glsl_argstr = glsl_argstr
		self.args = args

		top = cg.top()
		self.pvar = top.pvar
		self.dim = top.dim

		if not _active_mset.empty() and hasattr(self, "mdef"): self.mdef(_active_mset)

		if self.fn_tx:
			pvar1 = cg.ident("p");
			cg.line("\tvec%d %s = %s(%s%s);" % (self.dim, pvar1, self.fn_tx, self.pvar, glsl_argstr))
			self.pvar = pvar1

		if self.fn_map:
			# TODO if layerselect()
			dvar = cg.ident("d")
			cg.line("\tfloat %s = %s(%s%s);" % (dvar, self.fn_map, self.pvar, glsl_argstr))
			assert not hasattr(self, "dvar")
			self.dvar = dvar

		if self.is_leaf:
			if hasattr(self, "dvar"):
				top.rjoin(self)
		else:
			cg.push(self)

	def name(self):
		return type(self).nname()

class _RootNode(_Node):
	def __init__(self, pvar, dim):
		self.pvar = pvar
		self.dim = dim

class _Scope(_Node):
	def __init__(self, *args):
		self.args = args

	def __enter__(self):
		self.exec(self.args)

	def __exit__(self,type,value,tb):
		cg = _cg()
		cg.pop()
		if self.fn_d11 and hasattr(self, "dvar"):
			dvar1 = cg.ident("d")
			cg.line("\tfloat %s = %s(%s%s);" % (dvar1, self.fn_d11, self.dvar, self.glsl_argstr))
			self.dvar = dvar1
		if hasattr(self, "dvar"):
			cg.top().rjoin(self)

class _Leaf(_Node):
	def __init__(self, *args):
		self.exec(args)

class Material(_Scope):
	albedo = None
	@classmethod
	def __init_subclass__(subcls, **kwargs):
		super().__init_subclass__(**kwargs)
		_wpp_todo.append(subcls) # magic via _WithWithoutParentheses

	def mdef(self, mset):
		self.mvar = _cg().constant("Material", mset.format(self))

##############################################################################

class translate2(_Scope):
	argfmt = "2"
	glsl_p22 = """
	vec2 %(fn)s(vec2 p, vec2 r)
	{
		return p+r;
	}
	"""

class scale2(_Scope):
	argfmt = "1"
	glsl_p22 = """
	vec2 %(fn)s(vec2 p, float s)
	{
		return p/s;
	}
	"""
	glsl_d11 = """
	float %(fn)s(float d, float s)
	{
		return d*s;
	}
	"""

class circle2(_Leaf):
	argfmt = "1"
	glsl_p2d1 = """
	float %(fn)s(vec2 p, float r)
	{
		return length(p)-r;
	}
	"""

@_WithWithoutParentheses
class union(_Scope):
	glsl_d21 = """
	float %(fn)s(float d0, float d1)
	{
		return min(d0, d1);
	}
	"""
	# TODO join material

@_WithWithoutParentheses
class subtract(_Scope):
	glsl_d21 = """
	float %(fn)s(float d0, float d1)
	{
		return max(-d0, d1);
	}
	"""

@_WithWithoutParentheses
class intersect(_Scope):
	glsl_d21 = """
	float %(fn)s(float d0, float d1)
	{
		return max(d0, d1);
	}
	"""

class smooth_union(_Scope):
	argfmt = "1"
	glsl_d21 = """
	float %(fn)s(float d0, float d1, float k)
	{
		float h = clamp(0.5 + 0.5*(d1-d0)/k, 0.0, 1.0);
		return mix(d1, d0, h) - k*h*(1.0-h);
	}
	"""

class smooth_subtract(_Scope):
	argfmt = "1"
	glsl_d21 = """
	float %(fn)s(float d0, float d1, float k)
	{
		float h = clamp(0.5 - 0.5*(d1+d0)/k, 0.0, 1.0);
		return mix(d1, -d0, h) + k*h*(1.0-h);
	}
	"""


class smooth_intersect(_Scope):
	argfmt = "1"
	glsl_d21 = """
	float %(fn)s(float d0, float d1, float k)
	{
		float h = clamp(0.5 - 0.5*(d1-d0)/k, 0.0, 1.0);
		return mix(d1, d0, h) + k*h*(1.0-h);
	}
	"""
