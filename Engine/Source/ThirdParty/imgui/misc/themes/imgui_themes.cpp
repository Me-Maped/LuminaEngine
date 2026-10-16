#include "imgui_themes.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <vector>

namespace ImGuiThemes
{
namespace
{
struct Scalar { const char* name; float ImGuiStyle::* member; float min, max; bool keep_pixel; };
const Scalar scalars[] = {
    {"WindowRounding", &ImGuiStyle::WindowRounding, 0, 24, false},
    {"WindowBorderSize", &ImGuiStyle::WindowBorderSize, 0, 4, true},
    {"ChildRounding", &ImGuiStyle::ChildRounding, 0, 24, false},
    {"ChildBorderSize", &ImGuiStyle::ChildBorderSize, 0, 4, true},
    {"PopupRounding", &ImGuiStyle::PopupRounding, 0, 24, false},
    {"PopupBorderSize", &ImGuiStyle::PopupBorderSize, 0, 4, true},
    {"FrameRounding", &ImGuiStyle::FrameRounding, 0, 24, false},
    {"FrameBorderSize", &ImGuiStyle::FrameBorderSize, 0, 4, true},
    {"ScrollbarSize", &ImGuiStyle::ScrollbarSize, 6, 40, false},
    {"ScrollbarRounding", &ImGuiStyle::ScrollbarRounding, 0, 24, false},
    {"GrabMinSize", &ImGuiStyle::GrabMinSize, 4, 64, false},
    {"GrabRounding", &ImGuiStyle::GrabRounding, 0, 24, false},
    {"TabRounding", &ImGuiStyle::TabRounding, 0, 24, false},
    {"TabBorderSize", &ImGuiStyle::TabBorderSize, 0, 4, true},
    {"TabBarBorderSize", &ImGuiStyle::TabBarBorderSize, 0, 4, true},
    {"SeparatorSize", &ImGuiStyle::SeparatorSize, 1, 4, true},
    {"IndentSpacing", &ImGuiStyle::IndentSpacing, 0, 80, false}
};
struct Vector { const char* name; ImVec2 ImGuiStyle::* member; };
const Vector vectors[] = {
    {"WindowPadding", &ImGuiStyle::WindowPadding},
    {"FramePadding", &ImGuiStyle::FramePadding},
    {"ItemSpacing", &ImGuiStyle::ItemSpacing},
    {"ItemInnerSpacing", &ImGuiStyle::ItemInnerSpacing},
    {"CellPadding", &ImGuiStyle::CellPadding}
};
struct Extra { const char* name; ImVec4 Theme::* member; };
const Extra extras[] = {
    {"accent", &Theme::accent}, {"accent_text", &Theme::accent_text},
    {"danger", &Theme::danger}, {"danger_text", &Theme::danger_text}, {"shadow", &Theme::shadow}
};
bool ValidId(Id id) { return static_cast<int>(id) >= 0 && id < Id::Count; }
bool Range(float f, float lo, float hi) { return std::isfinite(f) && f >= lo && f <= hi; }
bool ColorOK(const ImVec4& c) { return Range(c.x,0,1) && Range(c.y,0,1) && Range(c.z,0,1) && Range(c.w,0,1); }
ImVec4 Color(unsigned rgb) { return ImVec4(((rgb>>16)&255)/255.0f, ((rgb>>8)&255)/255.0f, (rgb&255)/255.0f, 1); }
ImVec4 Mix(ImVec4 a, ImVec4 b, float t)
{
    return ImVec4(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t);
}
bool Validate(const Theme& t, std::string& error)
{
    if (!ValidId(t.id)) { error = "Invalid preset"; return false; }
    for (const Scalar& f : scalars)
        if (!Range(t.style.*(f.member), f.min, f.max)) { error = std::string("Invalid style value: ") + f.name; return false; }
    for (const Vector& f : vectors)
    {
        ImVec2 v = t.style.*(f.member);
        if (!Range(v.x,0,40) || !Range(v.y,0,40)) { error = std::string("Invalid spacing: ") + f.name; return false; }
    }
    for (int i = 0; i < ImGuiCol_COUNT; ++i)
        if (!ColorOK(t.style.Colors[i])) { error = std::string("Invalid color: ") + ImGui::GetStyleColorName(i); return false; }
    for (const Extra& f : extras)
        if (!ColorOK(t.*(f.member))) { error = std::string("Invalid semantic color: ") + f.name; return false; }
    if (!Range(t.shadow_offset,0,8)) { error = "shadow_offset must be between 0 and 8"; return false; }
    return true;
}
std::string Trim(const std::string& s)
{
    const std::size_t begin = s.find_first_not_of(" \t\r\n");
    return begin == std::string::npos ? "" : s.substr(begin, s.find_last_not_of(" \t\r\n") - begin + 1);
}
bool Numbers(std::string text, float* values, int count)
{
    std::replace(text.begin(), text.end(), ',', ' ');
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    for (int i = 0; i < count; ++i)
        if (!(in >> values[i]) || !std::isfinite(values[i])) return false;
    in >> std::ws;
    return in.eof();
}
bool ReadColor(const std::string& value, ImVec4& c)
{
    if (value.size() == 7 || value.size() == 9)
    {
        if (value[0] == '#')
        {
            unsigned rgba = 0;
            for (std::size_t i = 1; i < value.size(); ++i)
            {
                char x = value[i];
                int digit = x >= '0' && x <= '9' ? x-'0' : x >= 'a' && x <= 'f' ? x-'a'+10 : x >= 'A' && x <= 'F' ? x-'A'+10 : -1;
                if (digit < 0) return false;
                rgba = rgba * 16 + static_cast<unsigned>(digit);
            }
            if (value.size() == 7) rgba = (rgba << 8) | 255;
            c = ImVec4(((rgba>>24)&255)/255.0f, ((rgba>>16)&255)/255.0f, ((rgba>>8)&255)/255.0f, (rgba&255)/255.0f);
            return true;
        }
    }
    float values[4];
    if (!Numbers(value, values, 4)) return false;
    c = ImVec4(values[0], values[1], values[2], values[3]);
    return ColorOK(c);
}
void WriteColor(std::ostream& out, const ImVec4& c) { out << c.x << ',' << c.y << ',' << c.z << ',' << c.w; }
}

const char* Name(Id id)
{
    static const char* names[] = {"00 / ImGui Default", "01 / Cyberpunk", "03 / Minimal", "04 / Precision Lines", "07 / Neo Brutalism", "10 / Dark Bento"};
    return ValidId(id) ? names[static_cast<int>(id)] : "Invalid theme";
}
const char* Slug(Id id)
{
    static const char* names[] = {"default", "cyberpunk", "minimal", "wireframe", "brutalist", "bento"};
    return ValidId(id) ? names[static_cast<int>(id)] : "invalid";
}

Theme Make(Id id)
{
    Theme t;
    t.id = ValidId(id) ? id : Id::Bento;
    if (t.id==Id::Default)
    {
        // ImGuiStyle's constructor supplies native geometry and StyleColorsDark.
        t.accent=t.style.Colors[ImGuiCol_Text];
        t.accent_text=t.style.Colors[ImGuiCol_Text];
        t.danger=t.style.Colors[ImGuiCol_Text];
        t.danger_text=t.style.Colors[ImGuiCol_Text];
        return t;
    }
    const bool light = t.id == Id::Minimal || t.id == Id::Brutalist;
    if (light) ImGui::StyleColorsLight(&t.style); else ImGui::StyleColorsDark(&t.style);
    ImVec4 bg, panel, text, border;
    switch (t.id)
    {
    case Id::Cyberpunk:
        bg=Color(0x090D18); panel=Color(0x121B2B); text=Color(0xE3F8FF); border=Color(0x284256);
        t.accent=Color(0x00E5FF); t.accent_text=Color(0x071018); t.danger=Color(0xFF3DA6); t.danger_text=Color(0x170D18);
        t.style.FrameRounding=2; t.style.ChildRounding=4; break;
    case Id::Minimal:
        bg=Color(0xF7F7F5); panel=Color(0xFFFFFF); text=Color(0x242729); border=Color(0xDADFD9);
        t.accent=Color(0x334B40); t.accent_text=Color(0xFFFFFF); t.danger=Color(0xA63D3D); t.danger_text=Color(0xFFFFFF);
        t.style.FrameRounding=3; t.style.ChildRounding=4; break;
    case Id::Wireframe:
        bg=Color(0x15181C); panel=Color(0x1A1E24); text=Color(0xDEE5EC); border=Color(0x46505E);
        t.accent=Color(0xF3BC59); t.accent_text=Color(0x15181C); t.danger=Color(0xF47B83); t.danger_text=Color(0x15181C);
        t.style.FrameRounding=0; t.style.ChildRounding=0; t.outline_buttons=true; break;
    case Id::Brutalist:
        bg=Color(0xFFF8E8); panel=Color(0xFFFDF5); text=Color(0x171717); border=Color(0x171717);
        t.accent=Color(0x375BFF); t.accent_text=Color(0xFFFFFF); t.danger=Color(0xFF8A7A); t.danger_text=Color(0x171717);
        t.style.FrameRounding=0; t.style.ChildRounding=0; t.shadow_offset=3; t.shadow=Color(0x171717); break;
    default:
        bg=Color(0x101316); panel=Color(0x1B2026); text=Color(0xE8EEF2); border=Color(0x303840);
        t.accent=Color(0xC9F27A); t.accent_text=Color(0x101316); t.danger=Color(0xF58C91); t.danger_text=Color(0x251012);
        t.style.FrameRounding=5; t.style.ChildRounding=8; break;
    }
    ImGuiStyle& s = t.style;
    s.WindowPadding=ImVec2(20,18); s.FramePadding=ImVec2(12,6);
    s.ItemSpacing=ImVec2(12,8); s.ItemInnerSpacing=ImVec2(8,6); s.CellPadding=ImVec2(10,6);
    s.WindowRounding=s.ChildRounding; s.PopupRounding=s.FrameRounding;
    s.GrabRounding=s.FrameRounding; s.TabRounding=s.FrameRounding;
    s.ScrollbarRounding=s.FrameRounding; s.ScrollbarSize=12; s.GrabMinSize=12;
    s.FrameBorderSize = (t.id==Id::Wireframe || t.id==Id::Brutalist) ? 1.0f : 0.0f;
    s.ChildBorderSize = t.id==Id::Minimal ? 0.0f : 1.0f;
    s.WindowBorderSize=1; s.PopupBorderSize=1; s.TabBorderSize=0; s.TabBarBorderSize=1;
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]=text; c[ImGuiCol_TextDisabled]=Mix(text,panel,0.48f);
    c[ImGuiCol_WindowBg]=bg; c[ImGuiCol_ChildBg]=panel; c[ImGuiCol_PopupBg]=panel;
    c[ImGuiCol_Border]=border; c[ImGuiCol_BorderShadow]=ImVec4(0,0,0,0);
    c[ImGuiCol_FrameBg]=Mix(panel,text,light?0.045f:0.065f);
    c[ImGuiCol_FrameBgHovered]=Mix(panel,t.accent,0.14f); c[ImGuiCol_FrameBgActive]=Mix(panel,t.accent,0.22f);
    c[ImGuiCol_TitleBg]=panel; c[ImGuiCol_TitleBgActive]=panel; c[ImGuiCol_TitleBgCollapsed]=bg; c[ImGuiCol_MenuBarBg]=panel;
    c[ImGuiCol_Button]=Mix(panel,text,light?0.065f:0.08f);
    c[ImGuiCol_ButtonHovered]=Mix(panel,t.accent,0.20f); c[ImGuiCol_ButtonActive]=Mix(panel,t.accent,0.30f);
    c[ImGuiCol_Header]=Mix(panel,t.accent,0.16f); c[ImGuiCol_HeaderHovered]=Mix(panel,t.accent,0.23f); c[ImGuiCol_HeaderActive]=Mix(panel,t.accent,0.32f);
    c[ImGuiCol_CheckMark]=t.accent; c[ImGuiCol_CheckboxSelectedBg]=c[ImGuiCol_FrameBg];
    c[ImGuiCol_SliderGrab]=t.accent; c[ImGuiCol_SliderGrabActive]=Mix(t.accent,text,0.15f);
    c[ImGuiCol_Separator]=border; c[ImGuiCol_SeparatorHovered]=t.accent; c[ImGuiCol_SeparatorActive]=t.accent;
    c[ImGuiCol_ScrollbarBg]=bg; c[ImGuiCol_ScrollbarGrab]=Mix(panel,text,0.22f);
    c[ImGuiCol_ScrollbarGrabHovered]=Mix(panel,text,0.35f); c[ImGuiCol_ScrollbarGrabActive]=Mix(panel,t.accent,0.5f);
    c[ImGuiCol_Tab]=panel; c[ImGuiCol_TabHovered]=c[ImGuiCol_HeaderHovered]; c[ImGuiCol_TabSelected]=c[ImGuiCol_Header];
    c[ImGuiCol_TabSelectedOverline]=t.accent; c[ImGuiCol_TabDimmed]=panel;
    c[ImGuiCol_TabDimmedSelected]=Mix(panel,t.accent,0.10f); c[ImGuiCol_TabDimmedSelectedOverline]=border;
    c[ImGuiCol_TableHeaderBg]=Mix(panel,t.accent,light?0.08f:0.12f);
    c[ImGuiCol_TableBorderStrong]=border; c[ImGuiCol_TableBorderLight]=Mix(panel,border,0.5f);
    c[ImGuiCol_TableRowBg]=ImVec4(0,0,0,0); c[ImGuiCol_TableRowBgAlt]=ImVec4(text.x,text.y,text.z,0.035f);
    c[ImGuiCol_PlotLines]=t.accent; c[ImGuiCol_PlotLinesHovered]=t.danger;
    c[ImGuiCol_PlotHistogram]=t.accent; c[ImGuiCol_PlotHistogramHovered]=Mix(t.accent,text,0.2f);
    c[ImGuiCol_TextLink]=t.accent; c[ImGuiCol_TextSelectedBg]=ImVec4(t.accent.x,t.accent.y,t.accent.z,0.30f);
    c[ImGuiCol_NavCursor]=t.accent; c[ImGuiCol_InputTextCursor]=text; c[ImGuiCol_TreeLines]=border;
    c[ImGuiCol_ResizeGrip]=ImVec4(t.accent.x,t.accent.y,t.accent.z,0.2f);
    c[ImGuiCol_ResizeGripHovered]=Mix(panel,t.accent,0.6f); c[ImGuiCol_ResizeGripActive]=t.accent;
    c[ImGuiCol_DragDropTarget]=t.accent; c[ImGuiCol_DragDropTargetBg]=ImVec4(t.accent.x,t.accent.y,t.accent.z,0.15f);
    if (t.id==Id::Brutalist)
    {
        s.FrameBorderSize=2; s.ChildBorderSize=2; s.WindowBorderSize=2; s.PopupBorderSize=2;
        c[ImGuiCol_Header]=Color(0xFFE15B); c[ImGuiCol_HeaderHovered]=Color(0xFFEB88); c[ImGuiCol_HeaderActive]=Color(0xF4D035);
        c[ImGuiCol_TabSelected]=Color(0xFFE15B); c[ImGuiCol_TableHeaderBg]=Color(0xFFE15B);
    }
    return t;
}

bool Apply(const Theme& input, float scale)
{
    const Theme theme=input.id==Id::Default ? Make(Id::Default) : input;
    std::string error;
    if (!Range(scale,0.5f,4.0f) || !Validate(theme,error)) return false;
    ImGuiStyle scaled = theme.style;
    scaled.ScaleAllSizes(scale);
    ImGuiStyle& target = ImGui::GetStyle();
    for (const Scalar& f : scalars)
    {
        float value=scaled.*(f.member);
        if (f.keep_pixel && theme.style.*(f.member)>0) value=std::max(1.0f,value);
        target.*(f.member)=value;
    }
    for (const Vector& f : vectors) target.*(f.member) = scaled.*(f.member);
    for (int i = 0; i < ImGuiCol_COUNT; ++i) target.Colors[i] = theme.style.Colors[i];
    return true;
}

bool Load(const char* path, Theme& theme, std::string& error)
{
    error.clear();
    std::ifstream in(path);
    if (!in) { error=std::string("Cannot open: ")+path; return false; }
    struct Entry { std::string section, key, value; int line; };
    std::vector<Entry> entries;
    std::set<std::string> seen;
    std::string section, line;
    int number=0;
    Id preset=Id::Count;
    while (std::getline(in,line))
    {
        ++number;
        if (number > 2048 || line.size() > 4096) { error="Theme file exceeds size limits"; return false; }
        line=Trim(line);
        if (line.empty() || line[0]=='#' || line[0]==';') continue;
        if (line[0]=='[' && line.back()==']')
        {
            section=line.substr(1,line.size()-2);
            if (section!="theme" && section!="style" && section!="colors" && section!="extras")
            { error="Unknown section at line "+std::to_string(number); return false; }
            continue;
        }
        std::size_t eq=line.find('=');
        if (eq==std::string::npos || section.empty()) { error="Expected section and key=value at line "+std::to_string(number); return false; }
        Entry e={section,Trim(line.substr(0,eq)),Trim(line.substr(eq+1)),number};
        if (!seen.insert(section+"/"+e.key).second) { error="Duplicate key at line "+std::to_string(number); return false; }
        if (section=="theme" && e.key=="preset")
            for (int i=0;i<static_cast<int>(Id::Count);++i)
                if (e.value==Slug(static_cast<Id>(i))) preset=static_cast<Id>(i);
        entries.push_back(e);
    }
    if (in.bad()) { error="Failed to read theme file"; return false; }
    if (!ValidId(preset)) { error="Missing or unknown [theme] preset"; return false; }
    if (preset==Id::Default) { error="00 / ImGui Default is a read-only built-in baseline"; return false; }
    Theme candidate=Make(preset);
    for (const Entry& e : entries)
    {
        bool ok=false;
        if (e.section=="theme") ok=e.key=="preset";
        else if (e.section=="colors")
        {
            for (int i=0;i<ImGuiCol_COUNT;++i)
                if (e.key==ImGui::GetStyleColorName(i)) ok=ReadColor(e.value,candidate.style.Colors[i]);
        }
        else if (e.section=="style")
        {
            for (const Scalar& f : scalars)
                if (e.key==f.name) ok=Numbers(e.value,&(candidate.style.*(f.member)),1);
            for (const Vector& f : vectors)
                if (e.key==f.name)
                {
                    float values[2]; ok=Numbers(e.value,values,2);
                    if (ok) candidate.style.*(f.member)=ImVec2(values[0],values[1]);
                }
        }
        else if (e.section=="extras")
        {
            for (const Extra& f : extras)
                if (e.key==f.name) ok=ReadColor(e.value,candidate.*(f.member));
            if (e.key=="shadow_offset") ok=Numbers(e.value,&candidate.shadow_offset,1);
            if (e.key=="outline_buttons")
            {
                ok=e.value=="true" || e.value=="false";
                if (ok) candidate.outline_buttons=e.value=="true";
            }
        }
        if (!ok) { error="Unknown key or invalid value at line "+std::to_string(e.line)+": "+e.key; return false; }
    }
    if (!Validate(candidate,error)) return false;
    theme=candidate;
    return true;
}

bool Save(const char* path, const Theme& theme, std::string& error)
{
    error.clear();
    if (theme.id==Id::Default) { error="00 / ImGui Default is read-only and cannot be saved"; return false; }
    if (!Validate(theme,error)) return false;
    const std::string temp=std::string(path)+".tmp";
    std::ofstream out(temp.c_str(),std::ios::trunc);
    if (!out) { error="Cannot write temporary file: "+temp; return false; }
    out.imbue(std::locale::classic()); out << std::setprecision(9);
    out << "# Unscaled appearance. Colors: #RRGGBB, #RRGGBBAA or r,g,b,a (0..1).\n[theme]\npreset=" << Slug(theme.id) << "\n\n[style]\n";
    for (const Scalar& f : scalars) out << f.name << '=' << theme.style.*(f.member) << '\n';
    for (const Vector& f : vectors)
    {
        const ImVec2 v=theme.style.*(f.member); out << f.name << '=' << v.x << ',' << v.y << '\n';
    }
    out << "\n[colors]\n";
    for (int i=0;i<ImGuiCol_COUNT;++i) { out << ImGui::GetStyleColorName(i) << '='; WriteColor(out,theme.style.Colors[i]); out << '\n'; }
    out << "\n[extras]\n";
    for (const Extra& f : extras) { out << f.name << '='; WriteColor(out,theme.*(f.member)); out << '\n'; }
    out << "shadow_offset=" << theme.shadow_offset << "\noutline_buttons=" << (theme.outline_buttons?"true":"false") << '\n';
    out.flush();
    bool ok=out.good(); out.close(); ok=ok && !out.fail();
    if (!ok || std::rename(temp.c_str(),path)!=0)
    {
        std::remove(temp.c_str()); error=std::string("Could not replace theme file: ")+path; return false;
    }
    return true;
}

bool Button(const Theme& theme, ButtonRole role, const char* label, ImVec2 size, float scale)
{
    if (theme.id==Id::Default) return ImGui::Button(label,size);
    const ImGuiStyle& style=ImGui::GetStyle();
    const ImVec2 label_size=ImGui::CalcTextSize(label,nullptr,true);
    if (size.x==0) size.x=label_size.x+style.FramePadding.x*2;
    else if (size.x<0) size.x=std::max(4.0f,ImGui::GetContentRegionAvail().x+size.x);
    if (size.y==0) size.y=label_size.y+style.FramePadding.y*2;
    else if (size.y<0) size.y=std::max(4.0f,ImGui::GetContentRegionAvail().y+size.y);
    const bool secondary=role==ButtonRole::Secondary;
    ImVec4 fill=secondary?style.Colors[ImGuiCol_Button]:role==ButtonRole::Danger?theme.danger:theme.accent;
    ImVec4 text=secondary?style.Colors[ImGuiCol_Text]:role==ButtonRole::Danger?theme.danger_text:theme.accent_text;
    ImVec4 border=style.Colors[ImGuiCol_Border];
    if (theme.outline_buttons)
    {
        border=secondary?border:fill; text=secondary?text:fill; fill=style.Colors[ImGuiCol_ChildBg];
    }
    if (theme.shadow_offset>0 && Range(scale,0.5f,4))
    {
        const float d=theme.shadow_offset*scale;
        ImVec2 p=ImGui::GetCursorScreenPos();
        const ImVec2 lo(p.x+d,p.y+d), hi(p.x+size.x+d,p.y+size.y+d);
        if (ImGui::IsRectVisible(lo,hi))
            ImGui::GetWindowDrawList()->AddRectFilled(lo,hi,ImGui::GetColorU32(theme.shadow),style.FrameRounding);
    }
    ImGui::PushStyleColor(ImGuiCol_Button,fill);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,Mix(fill,text,0.12f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,Mix(fill,text,0.23f));
    ImGui::PushStyleColor(ImGuiCol_Text,text);
    ImGui::PushStyleColor(ImGuiCol_Border,border);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,theme.outline_buttons?std::max(1.0f,style.FrameBorderSize):style.FrameBorderSize);
    const bool clicked=ImGui::Button(label,size);
    ImGui::PopStyleVar(); ImGui::PopStyleColor(5);
    return clicked;
}
}
