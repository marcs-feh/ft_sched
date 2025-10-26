local M = {}

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
return M
