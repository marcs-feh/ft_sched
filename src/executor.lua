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
	return ('%04x.log'):format(math.random((1 << 20) - 1))
end

function Task:new(cmd)
	local o = {}
	setmetatable(o, self)
	self.__index = self

	assert(type(cmd) == 'string')

	o.error_log_path = randlog()
	o.cmd = cmd .. ' 2>' .. o.error_log_path

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
	local status = self.handle:close()
	self.running = false

	if not status then
		local f = io.open(self.error_log_path, 'rb')
		assert(f, 'command failed and could not read its output')
		output = f:read('*a')
		f:close()
		error(output, 2)
	end

	return output
end

function Executor:submit(cmd)
	local t = Task:new(cmd)
	self.tasks[#self.tasks+1] = t
	t:run()
end

function Executor:wait()
	for _, t in ipairs(self.tasks) do
		t:wait()
	end
	self.tasks = {}
end

M.Executor = Executor
M.Task = Task
return M