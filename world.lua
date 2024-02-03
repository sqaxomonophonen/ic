require("lib")
--require("other")
RESET(3)
tx3d:translate{1,2,3}
tx3d:translate{1.1,2.2,3.3}
d11:round(0.1)

tx3d:translate{-10,0,0} sdf3d:sphere(1) pop()
sdf3d:sphere(2)
tx3d:translate{10,0,0} sdf3d:sphere(3) pop()
pop()
pop()
pop()
EMIT()
