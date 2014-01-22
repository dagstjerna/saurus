debug_compiler = _ARGS[1] == "debug"
gcc = not _ARGS[1] or _ARGS[1] == "gcc"

require("pack")

solution "saurus"
   configurations { "Debug", "Release" }
 
   project "saurus"
      kind "ConsoleApp"
      language "C"
      includedirs { "src/lua" }
      files { "src/**.h", "src/**.c" }
 
      if gcc then
         buildoptions { "-ansi", "-pedantic" }
      end

      configuration "Debug"
         defines { "DEBUG" }
         flags { "Symbols" }
 
      configuration "Release"
         defines { "RELEASE" }
         flags { "Optimize" }
