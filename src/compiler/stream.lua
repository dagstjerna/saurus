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

function create_stream(file, name, prefix, postfix)
	local stream = {_file = file, _idx = 1, _line = 1, _prefix = prefix, _postfix = postfix, _name = name or ""}

	local function pop_front(s)
		local ch = string.sub(s, 1, 1)
		s = string.sub(s, 2)
		if string.len(s) == 0 then
			s = nil
		end
		return ch, s
	end

	if type(file) == "userdata" then
		function stream:read(size)
			assert(size == 1)
			local ch = 	self._cache
			self._cache = nil
			if ch then
				if ch == '\n' then self._line = self._line + 1 end
				return ch
			end

			if self._prefix then
				ch, self._prefix = pop_front(self._prefix)
				if ch then
					return ch
				end
			end

			ch = self._file:read(size)
			if not ch and self._postfix then
				ch, self._postfix = pop_front(self._postfix)
				if ch then
					return ch
				end
			end

			if ch == '\n' then self._line = self._line + 1 end
			return ch
		end
		function stream:write(str)
			self._file:write(str)
			local nrlines = 0
			for line in string.gmatch(str, "\n") do
				nrlines = nrlines + 1
			end
			self._line = self._line + nrlines
		end
		function stream:unread(ch)
			self._cache = ch
			if ch == '\n' then self._line = self._line - 1 end
		end
	else
		error("Can't create stream from type: " .. type(file))
	end
	function stream:line()
		return self._line
	end
	return stream
end

function open_stream(name)
	return create_stream(io.open(name), name)
end
