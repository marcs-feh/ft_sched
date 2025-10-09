local cc = 'clang++'
local cflags = {'-std=c++20', '-fno-exceptions', '-fno-rtti', '-fno-strict-aliasing', '-fwrapv'}
local wflags = {'-Wall', '-Wextra', '-Werror=return-type'}

local files = {
	'base.cpp',
	'main.cpp',
	'ft_sched.cpp',
}

local process_queue = {}
local platform = arg[1] or '<Empty>'

function build()
	local args = _G.arg

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
		run(cmd)
	end
	wait()

	local cmd = ('%s %s -o ft_sched.exe'):format(cc, join_space(objects))
	run(cmd)

	wait()
end

function join_space(tbl)
	return table.concat(tbl, ' ')
end

function run(cmd)
	print('-> ' .. cmd)

	if platform == 'linux' or platform == 'windows' then
		cmd = cmd .. ' 2>&1'
	else
		error('Unknown platform: ' .. platform)
	end

	process_queue[#process_queue+1] = io.popen(cmd)
end

function wait()
	local fail = false
	for _, proc in ipairs(process_queue) do
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
	process_queue = {}
end

function append(dest, src)
	assert(type(dest) == 'table' and type(src) == 'table', 'Expected table')
	for _, v in ipairs(src) do
		dest[#dest+1] = v
	end
	return dest
end


build()