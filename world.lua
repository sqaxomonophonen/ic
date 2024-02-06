
--[[
material("m0", {
	albedo = {1, 0.5, 0.2},
})
]]

view3d("main", function()
	do
		local _ <close> = tx3d:translate{1,2,3} tx3d:translate{1.1,2.2,3.3} d11:round(0.1)
		do
			local _ <close> = d21:smooth_union(42)
			tx3d:translate{-10,0,0} sdf3d:sphere(1) pop()
			sdf3d:sphere(2)
			tx3d:translate{10,0,0} sdf3d:sphere(3) pop()
		end
	end
end)

view2d("shape", function()
	sdf2d:circle(3)
	local x = 1.5
	tx2d:translate{5,3} sdf2d:circle(2) pop()
	tx2d:translate{-5,-3} sdf2d:circle(2) pop()
	tx2d:translate{8,0} sdf2d:circle(x) pop()
	tx2d:translate{-8,0} sdf2d:circle(x) pop()
end)

header("primitives")

view3d("sphere", function()
	sdf3d:sphere(1)
end)
