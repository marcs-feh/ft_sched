local platform = arg[1] or error('Required platform')
local build_mode = arg[2] or 'debug'
local now = os.date('%Y-%m-%d %H:%M:%S', os.time())

function generate_ninja()
	local cc = 'clang++'
	local cflags = {'-std=c++20', '-fwrapv', '-fno-strict-aliasing', '-fno-rtti', '-fno-exceptions'}
	local wflags = {'-Wall', '-Wextra', '-Werror=return-type'}
	local ldflags = {}

	local sb = Builder:new()
	local sources = {
		'main.cpp',
		'base.cpp',
		'ft_sched.cpp',
	}
	local objects = {}

	if platform == 'linux' then
		cflags[#cflags+1] = '-fPIC'
		sources[#sources+1] = 'task_linux.cpp'
	elseif platform == 'windows' then
		cflags[#cflags+1] = '-D_CRT_SECURE_NO_WARNINGS'
		sources[#sources+1] = 'task_windows.cpp'
	end

	if build_mode == 'debug' then
		cflags[#cflags+1] = '-g'
		cflags[#cflags+1] = '-O0'
	elseif build_mode == 'release' then
		cflags[#cflags+1] = '-O2'
	end

	sb:line('cc = %s', cc)
	sb:line('cflags = %s', join_space(cflags))
	sb:line('wflags = %s', join_space(wflags))
	sb:line('ldflags = %s', join_space(ldflags))
	sb:line('rule compile')
	sb:line('  command = $cc $cflags $wflags -c $in -o $out -MD -MF $out.d')
	sb:line('  deps = gcc')
	sb:line('  depfile = $out.d')
	sb:line('rule archive')
	sb:line('  command = $ar rcs $out $in')
	sb:line('rule link')
	sb:line('  command = $cc -o $out $in $ldflags')

	for _, file in ipairs(sources) do
		local obj = file .. '.o'
		sb:line('build %s: compile %s', obj, file)
		objects[#objects+1] = obj
	end
	sb:line('build ft_sched.exe: link %s', join_space(objects))

	-- Ninja is picky about ending with a newline
	sb:line():line()
	local f = io.open('build.ninja', 'wb')
	f:write(sb:to_string())
	print(('Generated build.ninja (%s/%s)'):format(titlecase(build_mode), titlecase(platform)))
end

Builder = {}

function Builder:new(o)
	o = o or {}
	setmetatable(o, self)
	self.__index = self

	o.lines = {}

	return o
end

function Builder:line(fmt, ...)
	fmt = fmt or ''
	self.lines[#self.lines+1] = fmt:format(...)
	return self
end

function Builder:to_string()
	return table.concat(self.lines, '\n')
end

function join_space(tbl)
	return table.concat(tbl, ' ')
end

function titlecase(k)
	assert(#k > 0)
	return k:sub(1, 1):upper() .. k:sub(2)
end

generate_ninja()
