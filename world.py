from iclib import *

class m0(Material):
	albedo = (1,1,0)

class m1(Material):
	albedo = (1,0,1)

class mr(Material):
	albedo = (1,0,0)

class mg(Material):
	albedo = (0,1,0)
	emission = (0.5,0.5,0.5)

class mb(Material):
	albedo = (0,0,1)

class mw(Material):
	albedo = (1,1,1)

@view2d
def test2():
	with chain(m0, subtract):
		with chain(translate2(1,0.5)): circle2(2.0)
		with chain(translate2(2,0)): circle2(3)
	with m1: circle2(1.5)

@view2d
def testrgb():
	r = 1.1
	with chain(mr,translate2(0,  -1.0)): circle2(r)
	with chain(mg,translate2(-1.2,  1.0)): circle2(r)
	with chain(mb,translate2(1.2,   1.0)): circle2(r)


@view3d
def scene0():
	r = 0.8
	with subtract:
		with union:
			with chain(mr,translate3(0, -r, 0)): sphere3(1)
			with chain(mg,translate3(0,  0, 0)): sphere3(0.8)
			with chain(mb,translate3(0,  r, 0)): sphere3(1)
		cylinder3(0.5)
	with chain(mw, translate3(0,0,3)): box3(0.2,1,2)
	with chain(mw, translate3(0,0,0)): torus3(3,0.1)
	with chain(mw, translate3(0,0,0)): cappedtorus3((1,0), 3.0, 0.1)

@view3d
def stresstest0():
	r = 0.5
	n = 5
	with mw:
		for x in range(n):
			for y in range(n):
				for z in range(n):
					with translate3(x,y,z): sphere3(1)
