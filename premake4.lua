debug_compiler = _ARGS[1] == "debug"
gcc = not _ARGS[1] or _ARGS[1] == "gcc"

require("pack")

solution "saurus"
   configurations { "Debug", "Release" }

   configuration "Debug"
      defines { "DEBUG" }
      flags { "Symbols" }
 
   configuration "Release"
      defines { "RELEASE" }
      flags { "Optimize" }

   project "saurus"
      kind "ConsoleApp"
      language "C"
      includedirs { "lua" }
      files {
         "src/vm/*.h", "src/vm/*.c",
         "src/repl/*.h", "src/repl/*.c",
         "src/compiler/*.h", "src/compiler/*.c",
         "lua/**.h", "lua/**.c"
      }

      if gcc then
         buildoptions { "-ansi", "-pedantic" }
         links { "m" }
      end

      if os.getenv("SU_OPT_NO_FILE_IO") then defines { "SU_OPT_NO_FILE_IO" } end
      if os.getenv("SU_OPT_DYNLIB") then defines { "SU_OPT_DYNLIB" } end
