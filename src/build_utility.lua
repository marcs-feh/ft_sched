local M = {}

local Executor = {}

M.REDIRECT_STDERR = '2>'

function Executor:new()
	local o = {}
	setmetatable(o, self)
	self.__index = self

	o.tasks = {}
	o.running = false

	return o
end

local Task = {}

local function randlog()
	return ('%06x.log'):format(math.random((1 << 20) - 1))
end

function Task:new(cmd)
	local o = {}
	setmetatable(o, self)
	self.__index = self

	assert(type(cmd) == 'string')

	o.error_log_path = randlog()

	o.cmd = cmd .. ' ' .. M.REDIRECT_STDERR  .. o.error_log_path

	return o
end

function Task:run()
	self.running = true
	self.handle = io.popen(self.cmd)
	assert(self.handle, 'failed to start command')
	return self
end

function Task:wait()
	if not self.running then
		self:run()
	end

	local output = self.handle:read('*a')
	local ok = true;
	local status = self.handle:close()
	self.running = false

	local f = io.open(self.error_log_path, 'rb')
	if f then
		outerr = f:read('*a')
		f:close()
	end

	if not status then
		outerr = output .. '\n' .. outerr
		ok = false
	end

	os.remove(self.error_log_path)
	if not ok then
		error(output, 2)
	end

	return output, outerr
end

function Executor:submit(cmd, ...)
	local t = Task:new(cmd:format(...))
	self.tasks[#self.tasks+1] = t
	t:run()
	return self
end

function Executor:wait(verbose)
	local results = {}
	local errors = {}

	for i, t in ipairs(self.tasks) do
		local ok, result, err = pcall(function() return t:wait() end)
		results[#results+1] = {ok, result}

		if not ok then
			errors[#errors+1] = ('task `%s` has failed: %s'):format(t.cmd, result)
		end

		if verbose then
			print(result, err)
		end
	end

	if #errors > 0 then
		error(table.concat(errors, '\n'), 	2)
	end

	self.tasks = {}

	return results
end

local Flag = {}

local function filter(fn, list)
	local res = {}
	for _, v in ipairs(list) do
		if fn(v) then
			res[#res+1] = v
		end
	end
	return res
end

local function copy_range(tbl, a, b)
	local res = {}
	for i = a, b, 1 do
		res[#res+1] = tbl[i]
	end
	return res
end

function Flag:new(name, info, require_arg)
	local o = {}
	setmetatable(o, self)
	self.__index = self

	o.name = name
	o.info = info
	o.require_arg = require_arg or false
	o.value = nil

	return o
end

function Flag:parse(s)
	local f = Flag:new()
	if s:sub(1, 1) ~= '-' then
		error('invalid flag ' .. s)
	end

	local name_begin, name_end = s:find('%-%w+')

	if name_begin then
		local arg_begin, _ = s:find(':')
		f.require_arg = not not arg_begin
		f.name = s:sub(name_begin + 1, name_end)

		if f.require_arg then
			local val = s:sub(arg_begin + 1)
			f.value = val
		end

	else
		error('invalid flag')
	end

	return f
end

local Parser = {}

function Parser:new(description)
	local o = {}
	setmetatable(o, self)
	self.__index = self

	o.description = description or ''
	o.known_flags = {}
	o.flags_index = {}

	return o
end

function Parser:add(flag, info, required_arg)
	assert(not self.flags_index[flag], 'flag already exists: ' .. flag)
	self.known_flags[#self.known_flags + 1] = Flag:new(flag, info, required_arg or false)
	self.flags_index[flag] = #self.known_flags
	return self
end

function Parser:usage_message()
	local lines = {
		'usage:',
		'    ' .. self.description,
		'options:',
	}

	-- Get maximum flag length
	local max_flag_length = 1
	for _, f in ipairs(self.known_flags) do
		local name = ('-%s%s'):format(f.name, f.require_arg and ':?' or '')
		max_flag_length = math.max(max_flag_length, #name)
	end

	for _, f in ipairs(self.known_flags) do
		local name = ('-%s%s'):format(f.name, f.require_arg and ':?' or '')
		local pad = (' '):rep(max_flag_length - #name + 4)
		lines[#lines+1] = '    ' .. name .. pad .. f.info
	end

	return table.concat(lines, '\n')

end

function Parser:parse(arglist)
	local ignore_index = #arglist + 1
	for i, a in ipairs(arglist) do
		if a == '--' then
			ignore_index = i
		end
	end

	local args = copy_range(arglist, 1, ignore_index - 1)

	local flags = filter(function (s)
		return s:sub(1, 1) == '-' and #s > 1
	end, args)

	local res = {}

	for _, f in ipairs(flags) do
		local flag = Flag:parse(f)
		local known_flag = self.known_flags[self.flags_index[flag.name] or 0]

		if not known_flag then
			error('unknown flag: ' .. f)
		end

		if known_flag.require_arg and (flag.value == nil) then
			error('missing argument for flag: ' .. f)
		end

		flag.info = known_flag.info

		res[flag.name] = flag
	end

	return res
end

M.Flag = Flag
M.Parser = Parser
M.Executor = Executor
M.Task = Task
return M