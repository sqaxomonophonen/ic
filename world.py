from iclib import *

class m0(Material):
	albedo = (1,1,0)

class m1(Material):
	albedo = (0,1,1)

@view2d
def test2():
	with chain(m0, subtract):
		with chain(translate2(1,0.5)): circle2(2.0)
		with chain(translate2(2,0)): circle2(3)
	with m1: circle2(1.1)
