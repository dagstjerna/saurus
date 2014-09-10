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

local specials = {}
local macros = {}

local opt_tail = true

nil_substitute = {}

local function gen_error(msg, line)
	saurus_error = string.format("%s Line: %i", msg, line)
	error(saurus_error)
end

local function not_implemented(func, sexp)
	gen_error("Not implemented!", sexp.line)
end

local function gen_inst(func, inst, line, ...)
	local instidx = #func.instructions + 1
	func.instructions[instidx] = {inst, ...}
	func.linenr[instidx] = line and line or (#func.linenr == 0 and 1 or func.linenr[#func.linenr])
end

local function call_or_tail(func, tail, line, narg)
	gen_inst(func, (tail and opt_tail) and "TCALL" or "CALL", line, narg)
end

local function find_lable(func, lable)
	for i = #func.lables, 1, -1 do
		if func.lables[i] == lable then
			return i - 1
		end
	end
end

local function find_up(func, lable, lv)
	local idx = find_lable(func, lable)
	if idx then
		return idx, lv
	elseif func.parent then
		return find_up(func.parent, lable, (lv or 0) + 1)
	end
end

local function add_lable(func, lable)
	table.insert(func.lables, lable)
	local len = #func.lables
	if func.maxlables < len then
		func.maxlables = len
	end
end

local function push_const(const, const_list)
	local constid
	for i,v in ipairs(const_list) do
		if v == const then
			constid = i
			break
		end
	end
	if not constid then
		table.insert(const_list, const)
		constid = #const_list
	end
	return constid - 1
end

local function gen_fetch(func, sym)
	local id, lv = find_lable(func, sym.atom)
	local instidx = #func.instructions + 1

	if not id then
		id, lv = find_up(func, sym.atom)
		if id then
			table.insert(func.up, {lv, id, instidx})
			id = #func.lables + #func.up - 1
		end
	end

	if id then
		func.instructions[instidx] = {"LOAD", id}
	else
		func.instructions[instidx] = {"GETGLOBAL", push_const(sym.atom, func.const)}
	end
	func.linenr[instidx] = sym.line
end

local function gen_funccall(func, sexp, tail)
	local len = #sexp.data
	assert(len > 0)
	for i = 1, len do
		gen(func, sexp.data[i])
	end
	call_or_tail(func, tail, sexp.line, len - 1)
end

local function gen_rec(func, sexp, tail)
	local len = #sexp.data
	assert(len > 0)
	gen_inst(func, "COPY", sexp.line, 0)
	for i = 2, len do
		gen(func, sexp.data[i])
	end
	call_or_tail(func, tail, sexp.line, len - 1)
end

local function gen_const(func, const)
	local constid = push_const(const.atom, func.const)
	gen_inst(func, "PUSH", const.line, constid)
end

local function gen_do(func, sexp, tail)
	local len = #sexp.data
	assert(len > 0)
	for i = 2, len - 1 do
		gen(func, sexp.data[i])
		gen_inst(func, "POP", nil, 1)
	end
	gen(func, sexp.data[len], true)
end

local function gen_list(func, sexp, tail)
	if sexp.type ~= "SEXP" then
		gen(func, sexp, tail, true)
		return
	end
	local len = #sexp.data
	assert(len > 0)
	if sexp.data[1].type == "SYMBOL" and sexp.data[1].atom == "unquote" then
		gen(func, sexp.data[2], tail)
	else
		local lst = { atom = "list", line = sexp.line, type = "SYMBOL" }
		gen_fetch(func, lst)
		for i = 1, len do
			gen(func, sexp.data[i], false, true)
		end
		call_or_tail(func, tail, sexp.line, len)
	end
end

local function gen_quote(func, sexp, tail)
	local len = #sexp.data
	assert(len > 0)
	gen_list(func, sexp.data[2], tail)
end

local function gen_if(func, sexp, tail)
	local c, t, f = sexp.data[2], sexp.data[3], sexp.data[4]
	if not c or not t then
		gen_error("Expected at least 2 arguments to 'if' statement!", sexp.line)
	end
	
	gen(func, c)
	
	local condidx = #func.instructions + 1
	func.instructions[condidx] = {"TEST"}
	func.linenr[condidx] = c.line
	
	if f then
		gen(func, f, tail)
	else
		local n = { atom = "nil", line = t.line, type = "SYMBOL" }
		gen(func, n, tail)
	end

	local jmpidx = #func.instructions + 1
	func.instructions[jmpidx] = {"JMP"}
	func.linenr[jmpidx] = f and f.line or t.line
	
	func.instructions[condidx][2] = #func.instructions
	
	gen(func, t, tail)
	func.instructions[jmpidx][2] = #func.instructions
end

local function ismacro(sexp)
	if #sexp.data > 0 and sexp.data[1].type == "SYMBOL" and not in_macro_world then
		for k,v in pairs(macros) do
			if sexp.data[1].atom == k then
				return k
			end
		end
	end
end

local function gen_macro(func, sexp)
	if #sexp.data < 4 then
		gen_error("Expected at least 3 arguments to macro!", sexp.line)
	end
	if sexp.data[2].type ~= "SYMBOL" then
		gen_error("Macro definition expected symbol!", sexp.line)
	end

	local args, err = sexp.data[3], "Invalid macro!"
	if args.type == "SEXP" then
		for idx,arg in ipairs(args.data) do
			if arg.type ~= "SYMBOL" then
				gen_error(err, arg.line)
			end
		end
	elseif args.type ~= "SYMBOL" then
		gen_error(err, args.line)
	end

	local name = sexp.data[2].atom
	if macros[name] then
		gen_error("Redefinition of '" .. name .. "'.", sexp.line)
	end

	table.remove(sexp.data, 1)
	sexp.data[1].atom = "lambda"
	macros[name] = sexp
end

local function gen_macro_expansion(func, sexp)
	local name = ismacro(sexp)
	local macro = macros[name]
	local line = sexp.line
	table.remove(sexp.data, 1)

	local simple_stream = {
		write = function(self, str)
			self.data = self.data .. str
		end,
		data = ""
	}

	local code = {type = "SEXP", line = line, data = {macro}}
	for _,v in ipairs(sexp.data) do
		local arg = {type = "SEXP", line = line, data = {{type = "SYMBOL", line = line, atom = "quote"}, v}}
		table.insert(code.data, arg)
	end
	code = {type = "SEXP", line = line, data = {{type = "SYMBOL", line = line, atom = "lambda"}, {type = "SYMBOL", line = line, atom = "..."}, code}}

	in_macro_world = true
	compile(gen_sexp(code), name, simple_stream)
	in_macro_world = false
	local compiled_macro = simple_stream.data

	sexp = writebin.su_load_call(macro_state, #compiled_macro, compiled_macro)
	if not sexp then
		gen_error("Could not generate macro!", line)
	end

	return recreate_ast(sexp, line, name)
end

local function gen_lambda(func, sexp, tail)
	local new_func, len = {instructions = {}, linenr = {}, const = {}, lables = {}, maxlables = 0, prot = {}, up = {}}, #sexp.data
	if len < 2 then
		gen_error("Expected at least one arguments to lambda!", sexp.line)
	elseif len < 3 then
		not_implemented(func, sexp)
	end

	local args, num_args, err = sexp.data[2], -1, "Invalid lambda!"
	if args.type == "SEXP" then
		num_args = #args.data
		for idx,arg in ipairs(args.data) do
			if arg.type ~= "SYMBOL" then
				gen_error(err, arg.line)
			end
			add_lable(new_func, arg.atom)
		end
	elseif args.type == "SYMBOL" then
		add_lable(new_func, args.atom)
	else
		gen_error(err, arg.line)
	end

	new_func.parent = func
	for i = 3, len - 1 do
		gen(new_func, sexp.data[i])
		gen_inst(new_func, "POP", nil, 1)
	end
	gen(new_func, sexp.data[len], true)
	gen_inst(new_func, "RETURN")
	table.insert(func.prot, new_func)
	gen_inst(func, "LAMBDA", sexp.line, #func.prot - 1, num_args)
end

local function gen_let(func, sexp, tail)
	local len = #sexp.data
	if len < 3 then
		gen_error("Expected at least two arguments to let.", sexp.line)
	end
	if sexp.data[2].type ~= "SEXP" then
		gen_error("Expected s-expression.", sexp.line)
	end
	local args = sexp.data[2].data
	if #args % 2 ~= 0 then
		gen_error("Expected key value pairs.", sexp.line)
	end

	local keys, values = {}, {}
	for i = 1, #args - 1, 2 do
		if args[i].type ~= "SYMBOL" then
			gen_error("Expected key to be symbol.", sexp.line)
		end
		table.insert(keys, args[i])
		table.insert(values, args[i + 1])
	end

	local lm = { line = sexp.line, type = "SEXP", data = {
		{ atom = "lambda", line = sexp.line, type = "SYMBOL" },
		{ line = sexp.line, type = "SEXP", data = keys }
	}}

	for i = 3, len do
		table.insert(lm.data, sexp.data[i])
	end

	gen_lambda(func, lm)
	for i = 1, #values do
		gen(func, values[i])
	end
	call_or_tail(func, tail, sexp.line, #values)
end

local function gen_define(func, sexp, tail)
	local len = #sexp.data
	if len ~= 3 then
		gen_error("Expected 2 arguments to define!", sexp.line)
	end
	local sym = sexp.data[2]
	if sym.type ~= "SYMBOL" then
		gen_error("Expected symbol!", sym.line)
	end
	gen(func, sexp.data[3])
	gen_inst(func, "SETGLOBAL", sym.line, push_const(sym.atom, func.const))
end

local function gen_arith(op, func, sexp, tail)
	if op == "SUB" and #sexp.data == 2 then
		gen(func, sexp.data[2])
		gen_inst(func, "UNM", sexp.line)
		return
	end
	if #sexp.data < 3 then
		gen_error("Expected at least 2 arguments in arithmetic operation!", sexp.line)
	elseif #sexp.data > 3 then
		gen_funccall(func, sexp, tail)
	else
		for i = 2, #sexp.data do
			gen(func, sexp.data[i])
		end
		gen_inst(func, op, sexp.line)
	end
end

local function gen_logical(op, func, sexp, tail)
	if #sexp.data < 3 then
		gen_error("Expected at least 2 arguments in logical operation!", sexp.line)
	elseif #sexp.data > 3 then
		gen_funccall(func, sexp, tail)
	else
		for i = 2, #sexp.data do
			gen(func, sexp.data[i])
		end
		gen_inst(func, op, sexp.line)
	end
end

local function gen_greater(op, func, sexp)
	if #sexp.data < 3 then
		gen_error("Expected at least 2 arguments in logical operation!", sexp.line)
	elseif #sexp.data > 3 then
		gen_funccall(func, sexp, tail)
	else
		for i = #sexp.data, 2, -1 do
			gen(func, sexp.data[i])
		end
		gen_inst(func, "LESS", sexp.line)
	end
end

local function gen_greater_eq(op, func, sexp)
	if #sexp.data < 3 then
		gen_error("Expected at least 2 arguments in logical operation!", sexp.line)
	elseif #sexp.data > 3 then
		gen_funccall(func, sexp, tail)
	else
		for i = #sexp.data, 2, -1 do
			gen(func, sexp.data[i])
		end
		gen_inst(func, "LEQUAL", sexp.line)
	end
end

local function isspecial(sexp)
	local spe = sexp.data[1]
	if spe then
		for k,v in pairs(specials) do
			if k == spe.atom then
				return v
			end
		end
	end
end

local function list_to_vec(sexp, line, macro_name)
	local vec = {}
	while sexp ~= nil_substitute do
		table.insert(vec, recreate_ast(sexp[1], line, macro_name))
		sexp = sexp[2]
	end
	return vec
end

function recreate_ast(sexp, line, macro_name)
	local sexp_type = type(sexp)
	if sexp == nil_substitute then
		return {type = "SYMBOL", line = line, atom = "nil"}
	elseif sexp_type == "string" or sexp_type == "number" then
		return {type = "STRING", line = line, atom = sexp}
	elseif sexp_type == "boolean" then
		return {type = "SYMBOL", line = line, atom = sexp and "true" or "false"}
	elseif sexp_type == "table" then
		return {type = "SEXP", line = line, data = list_to_vec(sexp, line, macro_name)}
	else
		gen_error("Invalid expansion result from macro '" .. macro_name .. "'.", line)
	end
end

function gen(func, sexp, tail, quoted)
	if sexp.type == "NUMBER" or sexp.type == "STRING" then
		gen_const(func, sexp)
	elseif sexp.type == "SYMBOL" then
		if quoted or sexp.atom == "nil" or sexp.atom == "true" or sexp.atom == "false" then
			gen_const(func, sexp)
		else
			gen_fetch(func, sexp)
		end
	else
		if quoted then
			gen_list(func, sexp, tail)
		elseif ismacro(sexp) then
			sexp = assert(gen_macro_expansion(func, sexp))
			gen(func, sexp, tail)
		else
			local spe_func = isspecial(sexp)
			if spe_func then
				spe_func(func, sexp, tail)
			else
				gen_funccall(func, sexp, tail)
			end
		end
	end
end

function gen_sexp(sexp)
	local func = {instructions = {}, linenr = {}, const = {}, lables = {}, maxlables = 0, prot = {}, up = {}}
	gen(func, sexp)
	gen_inst(func, "LOAD", nil, 0)
	call_or_tail(func, false, nil, 1)
	gen_inst(func, "RETURN", nil)
	return func
end

specials["if"] = gen_if
specials["lambda"] = gen_lambda
specials["define"] = gen_define
specials["undef"] = not_implemented
specials["macro"] = gen_macro
specials["rec"] = gen_rec
specials["do"] = gen_do
specials["quote"] = gen_quote

specials["let"] = gen_let

specials["req"] = function(...) gen_logical("EQ", ...) end
specials["<"] = function(...) gen_logical("LESS", ...) end
specials[">"] = function(...) gen_greater("LESS", ...) end
specials["<="] = function(...) gen_logical("LEQUAL", ...) end
specials[">="] = function(...) gen_greater_eq("LEQUAL", ...) end

specials["not"] = function(...) gen_logical("NOT", ...) end
specials["and"] = function(...) gen_logical("AND", ...) end
specials["or"] = function(...) gen_logical("OR", ...) end

specials["+"] = function(...) gen_arith("ADD", ...) end
specials["-"] = function(...) gen_arith("SUB", ...) end
specials["*"] = function(...) gen_arith("MUL", ...) end
specials["/"] = function(...) gen_arith("DIV", ...) end
specials["%"] = function(...) gen_arith("MOD", ...) end
specials["^"] = function(...) gen_arith("POW", ...) end
