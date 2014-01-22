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

require("stream")
require("read")
require("gen")
require("compile")

SAURUS_VERSION = {0, 0, 1}
SAURUS_VERSION_STRING = "0.0.1"

local function start(src, dest)
	local input = create_stream(io.open(src, "r"), src, "(lambda _ARGS ", ")")
	local ast = read(input)
	local tree = gen_sexp(ast)

	if dest then
		local output = io.open(dest, "wb")
		compile(tree, dest, output)
		output:close()
	else
		compile(tree)
	end
end

function entry(src, dest)
	saurus_error = nil
	--macro_state = writebin.su_open()
	local res, msg = pcall(start, src, dest)
	--writebin.su_close(macro_state)
	assert(res, msg)
end
