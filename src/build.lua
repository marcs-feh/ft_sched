local u = require 'build_utility'

local platform = false
local build_mode = 'debug'
local now = os.date('%Y-%m-%d %H:%M:%S', os.time())

function main()
	local arg_parser = u.Parser:new(arg[0] .. ' <platform> [options]')
	arg_parser
		:add('generate', 'Generate code before compiling')
		:add('mode', 'Build mode [debug|release]. Default is debug', true)
		:add('flash', 'Flash STM32 with provided serial number', true)

	platform = arg[1]
	local valid_platforms = {linux = true, windows = true, stm32blackpill = true}

	if not valid_platforms[platform] then
		print('error: invalid platform ' .. tostring(platform))
		print(arg_parser:usage_message())
		os.exit(1)
	end

	local ok, flags = pcall(function() return arg_parser:parse(arg) end)
	if not ok then
		print('error: ' .. tostring(flags))
		print(arg_parser:usage_message())
		os.exit(1)
	end

	if flags.mode then
		if (flags.mode.value ~= 'debug') and (flags.mode.value ~= 'release') then
			error('invalid build mode: ' .. flags.mode.value)
		end
		build_mode = flags.mode.value
	end

	local flash_serial = nil
	if flags.flash then
		flash_serial = flags.flash.value
	end

	if flags.generate then
		generate_crc32()
	end

	local target_name = ('%s/%s'):format(titlecase(build_mode), titlecase(platform))
	print('Building for ' .. target_name)
	execute_build(flash_serial)
end

function execute_build(flash_serial)
	local cc = 'clang++'
	local cflags = {'-std=c++20', '-fwrapv', '-fno-strict-aliasing', '-fno-rtti', '-fno-exceptions', '-I.'}
	local wflags = {'-Wall', '-Wextra', '-Werror=return-type'}
	local ldflags = {}
	local ar = 'ar'

	local exec = u.Executor:new()

	local sources = {
		'main.cpp',
	}
	local objects = {}

	if platform == 'linux' then
		cflags[#cflags+1] = '-fPIC'
		sources[#sources+1] = 'platform_linux.cpp'
	elseif platform == 'windows' then
		cflags[#cflags+1] = '-D_CRT_SECURE_NO_WARNINGS'
		sources[#sources+1] = 'platform_windows.cpp'
	elseif platform == 'stm32blackpill' then
		cc = 'arm-none-eabi-g++'
		ar = 'arm-none-eabi-ar'

		local stm32flags = {
			-- Machine specfics
			'-mcpu=cortex-m4',
			'-mfpu=fpv4-sp-d16',
			'-mfloat-abi=hard',
			'-mthumb',

			-- This is to ensure better code elimination in the linker later.
			-- This is usually the default but it's better to be sure
			'-ffunction-sections',
			'-fdata-sections',
			-- '-fstack-usage',

			'-nostdlib',

			'-I./stm32/Core/Inc',
			'-I./stm32/Middlewares/Third_Party/FreeRTOS/Source/include',
			'-I./stm32/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2',
			'-I./stm32/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F',

			'-DFT_SCHED_NO_MAIN',
		}

		cflags = join_list(cflags, stm32flags)
		sources[#sources+1] = 'platform_stm32f411ceu6.cpp'
	end

	if build_mode == 'debug' then
		cflags[#cflags+1] = '-g3'
		cflags[#cflags+1] = '-O0'
	elseif build_mode == 'release' then
		cflags[#cflags+1] = '-Os'
	end

	cflags = join_list(cflags, wflags)

	local output = 'ft_sched.exe'
	if platform == 'stm32blackpill' then
		output = 'libft_sched.a'
	end

	pcall(function () u.Task:new('mkdir build'):wait() end)

	for _, file in ipairs(sources) do
		local obj = ('build/%s.o'):format(file)
		exec:submit('%s %s -c %s -o %s', cc, join_space(cflags), file, obj)
		objects[#objects+1] = obj
		print(obj)
	end

	exec:wait()

	local output = 'build/ft_sched.exe'

	if platform == 'stm32blackpill' then
		output = 'build/libft_sched.a'
		exec:submit('%s rcs %s %s', ar, output, join_space(objects), join_space(ldflags)):wait()
		print(output)

		local dest = output:gsub('build/', 'stm32/', 1)
		os.rename(output, dest)
		print(output..' -> '..dest)

		print('Building image')
		exec:submit('make -C stm32 -j4'):wait(true)

		if flash_serial then
			print('Flashing to ' .. tostring(flash_serial))
			exec:submit('st-flash --serial %s --connect-under-reset write stm32/build/stm32.bin 0x08000000', flash_serial)
		end
	else
		exec:submit('%s -o %s %s', cc, output, join_space(objects), join_space(ldflags))
		print(output)
	end

	exec:wait()
end

function generate_crc32()
	local sb = Builder:new()
	local polynomial = 0xEDB88320
	local lut = new_c32_lut(polynomial)
	assert(#lut == 256, 'Invalid LUT')

	sb:line(('/* File auto generated at %s */'):format(now))
	sb:line('constexpr u32 CRC32_POLYNOMIAL = 0x%08X;', polynomial)
	sb:line('constexpr u32 crc32_lut[] = {')
	sb:line('\t')
	for i, v in ipairs(lut) do
		sb:append('0x%08x,', v)

		if (i ~= 0) and (i % 8 == 0) then
			sb:line()
			sb:append('\t')
		end
	end
	sb:line('};')

	local f = io.open('crc32_lut.gen.cpp', 'wb')
	f:write(sb:to_string())
	f:close()
	print(('Generated crc32_lut.gen.cpp [P = 0x%08X]'):format(polynomial))
end

function new_c32_lut(polynomial)
    local BIT_WIDTH = 32
    local TOP_BIT = (1 << (BIT_WIDTH - 1))  -- 0x80000000

    local lut = {}
    local remainder = 0

    for dividend = 0, 255 do
        remainder = dividend << (BIT_WIDTH - 8)

        for bit = 8, 1, -1 do
            if (remainder & TOP_BIT) ~= 0 then
                remainder = (remainder << 1) & 0xFFFFFFFF -- Truncate to 32bit
                remainder = remainder ~ polynomial
            else
                remainder = (remainder << 1) & 0xFFFFFFFF  -- Truncate to 32bit
            end
        end

        lut[dividend + 1] = remainder
    end

    return lut
end

--- String builder
Builder = {}

function Builder:new(o)
	o = o or {}
	setmetatable(o, self)
	self.__index = self

	o.lines = {}

	return o
end

function Builder:append(fmt, ...)
	fmt = fmt or ''
	self.lines[#self.lines] = (self.lines[#self.lines] or '') .. fmt:format(...)
	return self
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

function join_list(a, b)
	res = {}
	for _, v in ipairs(a) do
		res[#res+1] = v
	end
	for _, v in ipairs(b) do
		res[#res+1] = v
	end
	return res
end

local ok, err = pcall(main)
if not ok then
	print('error: '.. tostring(err))
	os.exit(1)
end
