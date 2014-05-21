debug_compiler = _ARGS[1] == "debug"
gcc = not _ARGS[1] or _ARGS[1] == "gcc"

require("pack")

solution "saurus"
   configurations { "Debug", "Release" }
 
   project "saurus"
      kind "ConsoleApp"
      language "C"
      includedirs { "lua" }
      files { "src/**.h", "src/**.c", "lua/**.h", "lua/**.c" }
 
      if gcc then
         buildoptions { "-ansi", "-pedantic" }
         links { "m" }
      end

      if os.getenv("SU_OPT_NO_FILE_IO") then defines { "SU_OPT_NO_FILE_IO" } end
      if os.getenv("SU_OPT_DYNLIB") then defines { "SU_OPT_DYNLIB" } end
      
      configuration "Debug"
         defines { "DEBUG" }
         flags { "Symbols" }
 
      configuration "Release"
         defines { "RELEASE" }
         flags { "Optimize" }
