from iclib import *

@view2d
def test1():
	with chain(union, subtract, translate2(-1,-1)):
		with translate2(-2,5): circle2(1)
		with translate2(2,-5): circle2(1)

@view2d
def test2():
	with chain(union, subtract, translate2(-1,-1)):
		with translate2(-2,5): circle2(3)
		with translate2(2,-5): circle2(3)
