project "RayTracer"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++17"
   targetdir "bin/%{cfg.buildcfg}"
   staticruntime "off"

   files { "src/**.h", "src/**.cpp", "src/**.hpp", "src/**.c", "src/**.comp", "src/**.spv", "src/**.glsl" }

   includedirs
   {
      "../vendor/imgui",
      "../vendor/glfw/include",

      "../Walnut/src",

      "src/Data",
      "src/Renderer",
      "src/Utils",
      "src/Shaders",

      "%{IncludeDir.VulkanSDK}",
      "%{IncludeDir.glm}",
      "%{IncludeDir.kompute}",
   }

   libdirs
   {
      "%{LibraryDir.kompute}",
   }

   links
   {
      "Walnut",
      "fmt",
      "kompute",
      "kp_logger",
   }


   targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
   objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

   filter "system:windows"
      buildoptions { "/utf-8" }
      systemversion "latest"
      defines { "WL_PLATFORM_WINDOWS" }
      
      filter "configurations:Debug"
      defines { "WL_DEBUG" }
      runtime "Debug"
      symbols "On"
      
      filter "configurations:Release"
      defines { "WL_RELEASE" }
      runtime "Release"
      optimize "On"
      symbols "On"

   filter "configurations:Dist"
      kind "WindowedApp"
      defines { "WL_DIST" }
      runtime "Release"
      optimize "On"
      symbols "Off"