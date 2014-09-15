--******************************************************************************--
--* S A U R U S                                                                *--
--* Copyright (c) 2009-2014 Andreas T Jonsson <andreas@saurus.org>             *--
--*                                                                            *--
--* This software is provided 'as-is', without any express or implied          *--
--* warranty. In no event will the authors be held liable for any damages      *--
--* arising from the use of this software.                                     *--
--*                                                                            *--
--* Permission is granted to anyone to use this software for any purpose,      *--
--* including commercial applications, and to alter it and redistribute it     *--
--* freely, subject to the following restrictions:                             *--
--*                                                                            *--
--* 1. The origin of this software must not be misrepresented; you must not    *--
--*    claim that you wrote the original software. If you use this software    *--
--*    in a product, an acknowledgment in the product documentation would be   *--
--*    appreciated but is not required.                                        *--
--*                                                                            *--
--* 2. Altered source versions must be plainly marked as such, and must not be *--
--*    misrepresented as being the original software.                          *--
--*                                                                            *--
--* 3. This notice may not be removed or altered from any source               *--
--*    distribution.                                                           *--
--******************************************************************************--

local instruction_set = {
	"PUSH",
	"POP",
	"COPY",
	
	"ADD",
	"SUB",
	"MUL",
	"DIV",
	"MOD",
	"POW",
	"UNM",

	"EQ",
	"LESS",
	"LEQUAL",

	"NOT",
	"AND",
	"OR",

	"TEST",
	"JMP",

	"RETURN",
	"CALL",
	"TCALL",
	"LAMBDA",

	"GETGLOBAL",
	"SETGLOBAL",
	"LOAD"
}

local instruction_matrix = {}
for i,v in ipairs(instruction_set) do
	assert(not instruction_matrix[v])
	instruction_matrix[v] = i - 1
end

local function compile_func(func, fp)
	fp:write(writebin.uint32(#func.instructions))
	for _,v in ipairs(func.instructions) do
		local inst = assert(instruction_matrix[v[1]], "Invalid instruction: " .. v[1])
		fp:write(writebin.uint8(inst))
		fp:write(writebin.uint8(v[2] or 0))
		fp:write(writebin.int16(v[3] or 0))
	end

	fp:write(writebin.uint32(#func.const))
	for _,v in ipairs(func.const) do
		local t = type(v)
		if t == "string" then
			if v == "nil" then
				fp:write(writebin.uint8(0))
			else
				fp:write(writebin.uint8(4))
				fp:write(writebin.string(v))
			end
		elseif t == "number" then
			fp:write(writebin.uint8(3))
			fp:write(writebin.number(v))
		elseif t == "boolean" then
			fp:write(writebin.uint8(v and 2 or 1))
		else
			error("Invalid type!")
		end
	end

	fp:write(writebin.uint32(#func.up))
	for _,v in ipairs(func.up) do
		fp:write(writebin.uint16(v[1]))
		fp:write(writebin.uint16(v[2]))
	end

	fp:write(writebin.uint32(#func.prot))
	for _,v in ipairs(func.prot) do
		compile_func(v, fp)
	end

	fp:write(writebin.string(func.name))

	fp:write(writebin.uint32(#func.linenr))
	for _,v in ipairs(func.linenr) do
		fp:write(writebin.uint32(v))
	end
end

function inspect(func, fp, lv)
	lv = lv or ""
	fp:write(lv .. "; ------- inst -------\n")
	fp:write(lv .. tostring(#func.instructions) .. " ; Numumber of instructions.\n")
	for i,v in ipairs(func.instructions) do
		fp:write(lv)
		for _,param in ipairs(v) do
			fp:write(tostring(param) .. " ")
		end
		if i <= #func.linenr then
			fp:write(" ; " .. tostring(func.linenr[i]) .. "\n")
		else
			fp:write("\n")
		end
	end
	fp:write(lv .. "; ------- const -------\n")
	fp:write(lv .. tostring(#func.const) .. " ; Numumber of constants.\n")
	for _,v in ipairs(func.const) do
		fp:write(lv .. tostring(v) .. "\n")
	end
	fp:write(lv .. "; ------- up values -------\n")
	fp:write(lv .. tostring(#func.up) .. " ; Numumber of up values.\n")
	for _,v in ipairs(func.up) do
		fp:write(lv .. tostring(v[1]) .. ", " .. tostring(v[2]) .. "\n")
	end
	fp:write(lv .. "; ------- prot -------\n")
	fp:write(lv .. tostring(#func.prot) .. " ; Numumber of prototypes.\n")
	for _,v in ipairs(func.prot) do
		inspect(v, fp, lv .. "\t")
	end
	fp:write(lv .. "; ------- debug info -------\n")
	fp:write(lv .. (func.name or "nil") .. " ; Name\n")
	fp:write(lv .. tostring(#func.linenr) .. " ; Line info.\n")
	for _,v in ipairs(func.linenr) do
		fp:write(lv .. tostring(v) .. "\n")
	end
end

function compile(func, name, fp)
	func.name = name or "?"
	--inspect(func, io.stdout)
	fp = fp or io.stdout

	fp:write(writebin.header(SAURUS_VERSION[1], SAURUS_VERSION[2]))
	compile_func(func, fp)
end
