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
 const auto path=std::filesystem::path(directory)/"Fonts"/"segoeui.ttf";
 std::ifstream input(path,std::ios::binary|std::ios::ate);
 if(!input) return;
 const auto size=input.tellg();
 if(size<=0 || size>16*1024*1024) return;
 bytes.resize(static_cast<std::size_t>(size)); input.seekg(0);
 if(!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) bytes.clear();
}
void installFonts() {
 auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.LogFilename=nullptr;
 ImFontConfig config; config.SizePixels=24; config.FontDataOwnedByAtlas=false;
 if(bytes.empty() || !io.Fonts->AddFontFromMemoryTTF(bytes.data(),static_cast<int>(bytes.size()),24,&config))
  io.Fonts->AddFontDefaultVector();
 io.FontDefault=io.Fonts->Fonts[0];
}
}
