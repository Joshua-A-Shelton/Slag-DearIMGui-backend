# Slag-DearImGui-backend #
Just a little library that enables use of [Slag](https://github.com/Joshua-A-Shelton/Slag) as a rendering backend for [Dear-ImGui](https://github.com/ocornut/imgui)

Including it in your project follows Dear-ImGui's philosophy. Just grab the two files (imgui_impl_slag.h/ imgui_impl_slag.cpp), and drop them into your project, no (extra) build system shenanigans (Slag itself still needs to be included, which may require build system tinkering, and the remaining dear-imgui files need to be copied to your project as well). 
# Usage #
An [example](https://github.com/Joshua-A-Shelton/Slag-DearIMGui-backend/tree/master/example) project has been provided to get you going, and show you what you need to do (using SDL2 as a windowing backend). The file you'll want to look at is [main.cpp](https://github.com/Joshua-A-Shelton/Slag-DearIMGui-backend/blob/master/example/main.cpp), which has an example that shows the Dear-ImGui demo window. All the other files are just the relevant Dear-ImGui files themselves. Good Luck!
# License #
ZLIB license is included in the files themselves, which meets the criteria for using the files. No need to add any extra files anywhere :)