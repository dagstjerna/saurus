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

local STRINGBOUND_CHARS = "\""
local STRINGESC_CHAR = "^"
local SPECIAL_CHARS = "[]{}()@',"
local COMMENT_CHARS = ";"

local function read_error(str, stream)
	saurus_error = str .. " Line: " .. tostring(stream and stream:line() or -1)
	error(saurus_error)
end

local function is_chars(chars, ch)
	return string.find(chars, ch, 1, true) ~= nil
end

local function is_space(ch)
	return string.find(ch, "%s") ~= nil
end

local function read_comment(stream)
	while true do
		local ch = stream:read(1)
		if not ch or ch == '\n' then
			return
		end
	end
end

local function read_atom(stream)
	local str = ""
	while true do
		local ch = stream:read(1)
		if not ch then
			return
		elseif not is_chars(SPECIAL_CHARS .. COMMENT_CHARS .. STRINGBOUND_CHARS, ch) and not is_space(ch) then
			str = str .. ch
		else
			stream:unread(ch)
			return str
		end
	end
end

local function read_space(stream)
	while true do
		local ch = stream:read(1)
		if not ch then
			return
		elseif not is_space(ch) then
			return ch
		end
	end
end

local function read_string_esc(stream, bound)
	local err = "Unexpected end of string!"
	local ch = stream:read(1)

	if not ch then
		read_error(err, stream)
	elseif ch == "\"" then
		return ch
	elseif ch == "n" then
		return "\n"
	elseif ch == "r" then
		return "\r"
	elseif ch == "t" then
		return "\t"
	elseif ch == "^" then
		return "^"
	elseif ch == "!" then
		ch = stream:read(1)
		if not ch then
			read_error(err, stream)
		else
			bound[1] = ch
		end
		return ""
	end
	read_error("Invalid escape character: '" .. ch .. "'", stream)
end

local function read_string(stream)
	local str = ""
	local bound = {STRINGBOUND_CHAR}
	while true do
		local ch = stream:read(1)
		if not ch then
			read_error("Unexpected end of string!", stream)
		elseif STRINGESC_CHAR == ch then
			str = str .. read_string_esc(stream, bound)
		elseif ch ~= bound[1] then
			str = str .. ch
		else
			return str
		end
	end
end

local function next_token(stream)
	while true do
		local ch = read_space(stream)
		if not ch then
			return
		elseif is_chars(COMMENT_CHARS, ch) then
			read_comment(stream)
		elseif is_chars(STRINGBOUND_CHARS, ch) then
			return {type = "STRING", atom = read_string(stream), line = stream:line()}
		elseif is_chars(SPECIAL_CHARS, ch) then
			return ch
		else
			local str, ch2 = ch, read_atom(stream)
			if ch2 then
				str = str .. ch2
			end
			local num = tonumber(str)
			if num then
				return {type = "NUMBER", atom = num, line = stream:line()}
			else
				return {type = "SYMBOL", atom = str, line = stream:line()}
			end
		end
	end
end

function read_list(stream, tok, lst)
	while tok do
		if tok == ')' or tok == '}' or tok == ']' then
			num_sexp = num_sexp - 1
			break
		end
		local sexp = read_sexp(stream, tok)
		if not sexp then
			break
		end
		table.insert(lst.data, sexp)
		tok = next_token(stream)
	end
	return lst
end

function read_sexp(stream, tok)
	if type(tok) == "string" then
		local oldtok, lst = tok, {type = "SEXP", data = {}, line = stream:line()}
		tok = next_token(stream)
		if not tok then
			return
		end
		if oldtok == '\'' then
			local sexp = read_sexp(stream, tok)
			if sexp then
				table.insert(lst.data, {type = "SYMBOL", atom = "quote", line = stream:line()})
				table.insert(lst.data, sexp)
				return lst
			end
		elseif oldtok == ',' then
			local sexp = read_sexp(stream, tok, lst)
			if sexp then
				table.insert(lst.data, {type = "SYMBOL", atom = "unquote", line = stream:line()})
				table.insert(lst.data, sexp)
				return lst
			end
		elseif oldtok == '@' then
			local sexp = read_sexp(stream, tok, lst)
			if sexp then
				table.insert(lst.data, {type = "SYMBOL", atom = "unref", line = stream:line()})
				table.insert(lst.data, sexp)
				return lst
			end
		elseif oldtok == '[' then
			local lv = num_sexp
			num_sexp = num_sexp + 1
			table.insert(lst.data, {type = "SYMBOL", atom = "vector", line = stream:line()})
			lst = read_list(stream, tok, lst)
			if num_sexp ~= lv then
				read_error("S-Expression is not closed!", stream)
			end
			return lst
		elseif oldtok == '{' then
			local lv = num_sexp
			num_sexp = num_sexp + 1
			table.insert(lst.data, {type = "SYMBOL", atom = "map", line = stream:line()})
			lst = read_list(stream, tok, lst)
			if num_sexp ~= lv then
				read_error("S-Expression is not closed!", stream)
			end
			return lst
		elseif oldtok == '(' then
			local lv = num_sexp
			num_sexp = num_sexp + 1
			lst = read_list(stream, tok, lst)
			if num_sexp ~= lv then
				read_error("S-Expression is not closed!", stream)
			end
			return lst
		else
			error("Bad AST tree!")
		end
	elseif tok.type then
		return tok
	else
		read_error("Syntax error in s-expression!", stream)
	end
end

function read(stream)
	num_sexp = 0
	local tok = next_token(stream)
	return tok and read_sexp(stream, tok) or nil
end
