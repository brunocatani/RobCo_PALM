#include "Fonts.h"
#include <Windows.h>
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <vector>
namespace wheel {
namespace { std::vector<char> bytes; }
void prepareFonts() {
 if(!bytes.empty()) return;
 wchar_t directory[MAX_PATH]{};
 if(!GetWindowsDirectoryW(directory, MAX_PATH)) return;
 const auto path=std::filesystem::path(directory)/"Fonts"/"consola.ttf";
 std::ifstream input(path,std::ios::binary|std::ios::ate);
 if(!input) return;
 const auto size=input.tellg();
 if(size<=0 || size>16*1024*1024) return;
 bytes.resize(static_cast<std::size_t>(size)); input.seekg(0);
 if(!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) bytes.clear();
}
ImFont* addPreparedFont(float size) {
 auto& io=ImGui::GetIO();ImFontConfig config;config.SizePixels=size;
 if(!bytes.empty()) {
  config.FontDataOwnedByAtlas=false;
  if(auto* font=io.Fonts->AddFontFromMemoryTTF(bytes.data(),static_cast<int>(bytes.size()),size,&config))return font;
 }
 return io.Fonts->AddFontDefaultVector(&config);
}
void installFonts() {
 auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.LogFilename=nullptr;
 io.FontDefault=addPreparedFont(24);
}
}
