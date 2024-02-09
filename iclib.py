import os, sys
from collections import namedtuple

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

_View = namedtuple("_View", ["dim", "name", "fn"])

def _register_view(dim, fn):
	name = fn.__name__
	assert name not in _viewset, ""
	_viewset.add(name)
	_views.append(_View(dim=dim, name=name, fn=fn))
	#print(_viewset)
	#print(_views)
	return fn

def view2d(fn): return _register_view(2,fn)
def view3d(fn): return _register_view(3,fn)

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

	def __call__(self): return self.v()

_isnum = lambda v: isinstance(v,(float,int))
_isvec = lambda n,v: (len(v)==n) and (False not in [_isnum(x) for x in v])

class _Node:
	argfmt = ""
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
				assert _isvec(2,a), "argument %d not a vec2" % i
			elif c == "3":
				assert _isvec(3,a), "argument %d not a vec3" % i
			elif c == "4":
				assert _isvec(4,a), "argument %d not a vec4" % i
			else:
				raise RuntimeError("unhandled argfmt char %s" % repr(c))
		self.args = args

	def name(self): return self.__class__.__name__

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
