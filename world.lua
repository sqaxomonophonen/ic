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

view3d("sphere", function()
	sdf3d:sphere(1)
end)
