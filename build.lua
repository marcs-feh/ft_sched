platform = arg[1] or '<Empty>'

function build()
	local exec = Executor:new()

	local cc = 'clang++'
	local cflags = {'-std=c++20', '-fno-exceptions', '-fno-rtti', '-fno-strict-aliasing', '-fwrapv'}
	local wflags = {'-Wall', '-Wextra', '-Werror=return-type'}

	local files = {
		'base.cpp',
		'main.cpp',
		'ft_sched.cpp',
	}

	if platform == 'linux' then
		append(cflags, {'-fPIC', '-DTARGET_HOSTED_LINUX'})
	elseif platform == 'windows' then
		append(cflags, {'-D_CRT_SECURE_NO_WARNINGS', '-DTARGET_HOSTED_WINDOWS'})
	else
		error('Unknown platform: ' .. platform)
	end

	append(cflags, wflags)

	local objects = {}
	for _, f in ipairs(files) do
		local obj = f .. '.o'
		objects[#objects+1] = obj
		local cmd = ('%s %s -c %s -o %s'):format(cc, join_space(cflags), f, obj)
		exec:submit(cmd)
	end
	exec:wait()

	local cmd = ('%s %s -o ft_sched.exe'):format(cc, join_space(objects))
	exec:submit(cmd):wait()
end

Executor = {}

function Executor:new(o)
	o = o or {}
	setmetatable(o, self)
	self.__index = self

	o.process_queue = {}

	return o
end

function Executor:submit(cmd)
	print('-> ' .. cmd)

	if platform == 'linux' or platform == 'windows' then
		cmd = cmd .. ' 2>&1'
	else
		error('Unknown platform: ' .. platform)
	end

	self.process_queue[#self.process_queue+1] = io.popen(cmd)

	return self
end

function Executor:wait()
	local fail = false
	for _, proc in ipairs(self.process_queue) do
		local output = proc:read('*all')
		ok, reason, exit_code = proc:close()
		if not ok then
			print('--- SUBPROCESS FAILED ---\n' .. output .. '\n')
			fail = true
		end
	end
	if fail then
		error('one or more subprocesses have failed')
	end
	self.process_queue = {}

	return self
end

function join_space(tbl)
	return table.concat(tbl, ' ')
end

function append(dest, src)
	assert(type(dest) == 'table' and type(src) == 'table', 'Expected table')
	for _, v in ipairs(src) do
		dest[#dest+1] = v
	end
	return dest
end

build()
