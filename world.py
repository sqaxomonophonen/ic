from iclib import *

@view2d
def test1():
	with subtract:
		with chain(translate2(-2,0), scale2(2)): circle2(2)
		with chain(translate2( 2,0), scale2(2)): circle2(3)
	circle2(2)
