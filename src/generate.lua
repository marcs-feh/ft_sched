local platform = arg[1] or error('Required platform')
local build_mode = arg[2] or 'debug'
local now = os.date('%Y-%m-%d %H:%M:%S', os.time())

function generate_ninja()
	local cc = 'clang++'
	local cflags = {'-std=c++20', '-fwrapv', '-fno-strict-aliasing', '-fno-rtti', '-fno-exceptions'}
	local wflags = {'-Wall', '-Wextra', '-Werror=return-type'}
	local ldflags = {'-fuse-ld=lld'}
	local ar = 'ar'

	local sb = Builder:new()
	local sources = {
		'main.cpp',
		'base.cpp',
		'ft_sched.cpp',
	}
	local objects = {}

	if platform == 'linux' then
		cflags[#cflags+1] = '-fPIC'
		sources[#sources+1] = 'platform_linux.cpp'
		ldflags[#ldflags+1] = '-fuse-ld=mold'
	elseif platform == 'windows' then
		cflags[#cflags+1] = '-D_CRT_SECURE_NO_WARNINGS'
		sources[#sources+1] = 'platform_windows.cpp'
		ldflags[#ldflags+1] = '-fuse-ld=lld'
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
			'-fstack-usage',

			'-nostdlib',
		}
		cflags = join_list(cflags, stm32flags)
	end

	if build_mode == 'debug' then
		ldflags[#ldflags+1] = '-g3'
		cflags[#cflags+1] = '-g3'
		cflags[#cflags+1] = '-O0'
	elseif build_mode == 'release' then
		cflags[#cflags+1] = '-Os'
	end

	sb:line('cc = %s', cc)
	sb:line('ar = %s', ar)
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

	if platform ~= 'stm32blackpill' then
		sb:line('build ft_sched.exe: link %s', join_space(objects))
	else
		sb:line('build ft_sched.a: archive %s', join_space(objects))
	end

	-- Ninja is picky about ending with a newline
	sb:line():line()
	local f = io.open('build.ninja', 'wb')
	f:write(sb:to_string())
	print(('Generated build.ninja (%s/%s)'):format(titlecase(build_mode), titlecase(platform)))
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
	print(('Generated crc32_lut.gen.cpp (P = 0x%08X)'):format(polynomial))
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
                remainder = ((remainder << 1) & 0xFFFFFFFF) -- Truncate to 32bit
                remainder = remainder ~ polynomial
            else
                remainder = (remainder << 1) & 0xFFFFFFFF  -- Truncate to 32bit
            end
        end

        lut[dividend + 1] = remainder
    end

    return lut
end

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

local valid_platforms = {linux = true, windows = true, stm32blackpill = true}

if not valid_platforms[platform] then
	error('Invalid platform', 1)
end

generate_ninja()
generate_crc32()

