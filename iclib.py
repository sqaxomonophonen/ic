import os, sys
import gc

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
class _Codegen:
	def __init__(self):
		self.define_set = set()
		self.defines = []
		self.lines = []
		self.ident_serials = {}
		self.const_map = {}

	def defined(self, name):
		return name in self.define_set

	def define(self, name, src):
		self.define_set.add(name)
		self.defines.append(src)

	def done(self):
		pass

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

class _View:
	def __init__(self, dim, name, ctor):
		self.dim = dim
		self.name = name
		self.ctor = ctor

	def __call__(self):
		global _active_codegen
		_active_codegen = _Codegen()
		self.ctor()
		_active_codegen.done()
		_active_codegen = None

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


# used to make `with union: ...` equivalent to `with union(): ...`
class _NoArgScope:
	def __init__(self, v):
		self.v = v
		self.i = None

	def __enter__(self):
		self.i = self.v()
		self.i.__enter__()

	def __exit__(self,type,value,tb):
		self.i.__exit__(type,value,tb)

	def __call__(self):
		return self.v()

_isnum  = lambda v: isinstance(v,(float,int))
_isvecn = lambda n,v: (len(v)==n) and (False not in [_isnum(x) for x in v])

class _Node:
	argfmt = ""

	def resolv(self, fn):
		n = "glsl_%s" % fn
		if not hasattr(self, n): return None
		r = "%s_%s" % (self.name(), fn)
		cg = _cg()
		if not cg.defined(r):
			cg.define(r, getattr(self, n)() % {"fn": r})
		return r

	def __init__(self, *args):
		#print(self.name(), args, "argfmt", self.argfmt)
		argfmt = self.argfmt
		n = len(argfmt)
		if n == 1:
			c = argfmt[0]
			if (c == "2" and len(args) == 2) or (c == "3" and len(args) == 3) or (c == "4" and len(args) == 4):
				args = [args]
		if len(args) != n: raise RuntimeError("invalid number of arguments; wanted %d; got %d" % (n, len(args)))
		for i in range(n):
			c = argfmt[i]
			a = args[i]
			if c == "1":
				assert _isnum(a), "argument %d not a number" % i
			elif c == "2":
				assert _isvecn(2,a), "argument %d not a vec2" % i
			elif c == "3":
				assert _isvecn(3,a), "argument %d not a vec3" % i
			elif c == "4":
				assert _isvecn(4,a), "argument %d not a vec4" % i
			else:
				raise RuntimeError("unhandled argfmt char %s" % repr(c))
		self.args = args

		p22 = self.resolv("p22")
		p33 = self.resolv("p33")
		p2d1 = self.resolv("p2d1")
		p3d1 = self.resolv("p3d1")
		is_2d = p22 or p2d1
		is_3d = p33 or p3d1
		assert not (is_2d and is_3d)

		print(p22,p33,p2d1,p3d1)

		if p22 is not None:
			pass

		if p33 is not None:
			pass

		if p2d1 is not None:
			pass

		if p3d1 is not None:
			pass

	def name(self):
		return self.__class__.__name__

class _Scope(_Node):
	def __enter__(self):
		#print("enter", self.name())
		pass # TODO push

	def __exit__(self,type,value,tb):
		#print("exit", self.name())
		pass # TODO pop

class _Leaf(_Node):
	pass

class translate2(_Scope):
	argfmt = "2"
	def glsl_p22(self):
		return """
		vec2 %(fn)s(vec2 p, vec2 r)
		{
			return p+r;
		}
		"""

class scale2(_Scope):
	argfmt = "1"
	def glsl_p22(self):
		return """
		vec2 %(fn)s(vec2 p, float s)
		{
			return p/s;
		}
		"""
	def glsl_d11(self):
		return """
		float %(fn)s(vec2 _p, float d, float s)
		{
			return d*s;
		}
		"""

class circle2(_Leaf):
	argfmt = "1"
	def glsl_p2d1(self):
		return """
		float %(fn)s(vec2 p, float r)
		{
			return length(p)-r;
		}
		"""

@_NoArgScope
class union(_Scope):
	def glsl_d21(self):
		return """
		float %(fn)s(float d0, float d1)
		{
			return min(d0, d1);
		}
		"""
	# TODO join material

@_NoArgScope
class subtract(_Scope):
	def glsl_d21(self):
		return """
		float %(fn)s(float d0, float d1)
		{
			return max(-d0, d1);
		}
		"""

@_NoArgScope
class intersect(_Scope):
	def glsl_d21(self):
		return """
		float %(fn)s(float d0, float d1)
		{
			return max(d0, d1);
		}
		"""

class smooth_union(_Scope):
	argfmt = "1"
	def glsl_d21(self):
		return """
		float %(fn)s(float d0, float d1, float k)
		{
			float h = clamp(0.5 + 0.5*(d1-d0)/k, 0.0, 1.0);
			return mix(d1, d0, h) - k*h*(1.0-h);
		}
		"""

class smooth_subtract(_Scope):
	argfmt = "1"
	def glsl_d21(self):
		return """
		float %(fn)s(float d0, float d1, float k)
		{
			float h = clamp(0.5 - 0.5*(d1+d0)/k, 0.0, 1.0);
			return mix(d1, -d0, h) + k*h*(1.0-h);
		}
		"""


class smooth_intersect(_Scope):
	argfmt = "1"
	def glsl_d21(self):
		return """
		float %(fn)s(float d0, float d1, float k)
		{
			float h = clamp(0.5 - 0.5*(d1-d0)/k, 0.0, 1.0);
			return mix(d1, d0, h) + k*h*(1.0-h);
		}
		"""
