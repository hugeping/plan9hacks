-- some stubs for tiny-instead
-- fake game.gui
-- stat, menu
-- fake audio
-- fake input

local exec = os.execute

function plan9_plumb(f)
	f = f:gsub(";.*$", "")
	exec("plumb "..f)
end

local old_pict = false

function plan9_pict(f)
	local np
	if type(instead.get_picture) == 'function' then
		np = instead.get_picture()
	elseif type(stead.get_picture) == 'function' then
		np = stead.get_picture()
	end
	if np ~= old_pict and type(np) == 'string' then
		plan9_plumb(np)
	end
	old_pict = np
end

if API == 'stead3' then
	require 'tiny3'
	return
end

require 'tiny2'
