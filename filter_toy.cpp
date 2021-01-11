#include <random>
#include <Mahi/Gui.hpp>
#include <Mahi/Util.hpp>
#include <kiss_fftr.h>
#include <Iir.h>
#include "ETFE.hpp"

using namespace mahi::gui;
using namespace mahi::util;
using namespace etfe;

class FilterToy : public Application {
public:

    FilterToy() : 
        Application()
    { 
        ImGui::StyleColorsMahiDark1();
        styleGui();
    }

    void update() override {

        // ImGui::Begin("ImGui Style");
        // ImGui::ShowStyleEditor();
        // ImGui::End();
        // ImGui::Begin("ImPlot Style");
        // ImPlot::ShowStyleEditor();
        // ImGui::End();

        static bool init = true;
        static bool open = true;

        constexpr int N = 10000;
        constexpr double Fs = 1000;
        constexpr double dt = 1.0 / Fs;

        static double f1 = 25;
        static double f2 = 50;
        static double a1 = 0.5;
        static double a2 = 0.5;

        static double ng = 1;
        static std::vector<double> noise(N);
        static std::default_random_engine generator;
        static std::normal_distribution<double> distribution(0,1);

        static int filter = 0;
        static double Fc = 250;
        static double Fw = 100;
        static Iir::Butterworth::LowPass<2> butt2lp;  // 0
        static Iir::Butterworth::HighPass<2> butt2hp; // 1
        static Iir::Butterworth::BandPass<2> butt2bp; // 2

        if (init) {
            // generate static noise
            for (auto& n : noise)
                n = distribution(generator);
            // initialize filters
            butt2lp.setup(Fs,Fc);
            butt2hp.setup(Fs,Fc);
            butt2bp.setup(Fs,Fc,Fw);
            init = false;
        }

        // gui inputs
        ImGui::SetNextWindowSize(ImVec2(960,540), ImGuiCond_FirstUseEver);
        ImGui::Begin("Filter Toy",&open, ImGuiWindowFlags_NoCollapse);
        ImGui::BeginChild("ChildL", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.5f, -1));
        ImGui::Text("Filter Input / Output");
        ImGui::Separator();
        ImGui::SliderDouble2("f1 / f2 [Hz]",&f1,0,500,"%.0f");
        ImGui::SliderDouble2("a1 / a2",&a1,0,10,"%.1f");
        ImGui::SliderDouble("noise",&ng,0,10,"%.1f");

        // ImGui::Separator();
        ImGui::ModeSelector(&filter, {"Lowpass","Highpass","Bandpass"});
        if (filter == 2) {
            if (ImGui::SliderDouble2("Fc / Fw [Hz]",&Fc,1,500))
                butt2bp.setup(Fs,Fc,Fw);
        }
        else {
            if (ImGui::SliderDouble("Fc [Hz]",&Fc,1,500))
                butt2lp.setup(Fs,Fc);
        }
        // ImGui::Separator();
        // reset filters
        butt2lp.reset();
        butt2hp.reset();
        butt2bp.reset();
        // generate waveform
        static double x[N];
        static double y[N];
        static double t[N];
        for (int i = 0; i < N; ++i) {            
            t[i] = i * dt;
            x[i] = a1*sin(2*PI*f1*t[i]) + a2*sin(2*PI*f2*t[i]) + ng * noise[i];
            y[i] = filter == 0 ? butt2lp.filter(x[i]) : filter == 1 ? butt2hp.filter(x[i]) : butt2bp.filter(x[i]);
        }

        // plot waveforms
        ImPlot::SetNextPlotLimits(0,10,-10,10);
        if (ImPlot::BeginPlot("##Filter","Time [s]","Signal",ImVec2(-1,-1))) {
            ImPlot::PlotLine("x(t)", t, x, N);
            ImPlot::PlotLine("y(t)", t, y, N);
            ImPlot::EndPlot();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("ChildR", ImVec2(0, -1));

        // perform ETFE
        static int window         = 0;
        static int inwindow        = 4;
        static int nwindow_opts[] = {100, 200, 500, 1000, 2000, 5000, 10000};
        static int infft          = 4;
        static int nfft_opts[]    = {100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000};
        static float overlap      = 0.5f;
        static bool need_update   = false;

        static ETFE etfe(N, Fs, hamming(nwindow_opts[inwindow]), nwindow_opts[inwindow]/2, nfft_opts[infft]);        

        ImGui::Text("Transfer Function");
        ImGui::Separator();
        if (ImGui::Combo("Window Method", &window,"hamming\0hann\0winrect\0"))                                           
            need_update = true;
        if (ImGui::Combo("Window Size", &inwindow,"100\0""200\0""500\0""1000\0""2000\0""5000\0""10000\0"))               
            need_update = true;
        if (ImGui::SliderFloat("Window Overlap",&overlap,0,1))                                                                  
            need_update = true;
        if (ImGui::Combo("FFT Size", &infft, "100\0""200\0""500\0""1000\0""2000\0""5000\0""10000\0""20000\0""50000\0")) {
            inwindow = infft < inwindow ? infft : inwindow;
            need_update = true;
        }
            
        if (need_update) {
            infft = inwindow > infft ? inwindow : infft;
            int nwindow  = nwindow_opts[inwindow];
            int noverlap = (int)(nwindow * overlap);
            int nfft     = nfft_opts[infft];
            etfe.setup(N,Fs,window == 0 ? hamming(nwindow) : window == 1 ? hann(nwindow) : winrect(nwindow), noverlap, nfft);
            need_update = false;
        }

        auto& result = etfe.estimate(x,y);    

        // ImGui::Separator();
        
        if (ImGui::BeginTabBar("Plots")) {
            if (ImGui::BeginTabItem("Magnitude")) {
                ImPlot::SetNextPlotLimits(1,500,-100,10);
                if (ImPlot::BeginPlot("##Bode1","Frequency [Hz]","Magnitude [dB]",ImVec2(-1,-1))) {
                    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.250f);
                    ImPlot::PlotShaded("##Mag1",result.f.data(),result.mag.data(),(int)result.f.size(),-100000);
                    ImPlot::PlotLine("##Mag2",result.f.data(),result.mag.data(),(int)result.f.size());
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }            
            if (ImGui::BeginTabItem("Phase")) {
                ImPlot::SetNextPlotLimits(1,500,-180,10);
                if (ImPlot::BeginPlot("##Bode2","Frequency [Hz]","Phase Angle [deg]",ImVec2(-1,-1))) {
                    ImPlot::PlotLine("##Phase",result.f.data(),result.phase.data(),(int)result.f.size());
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }   
            if (ImGui::BeginTabItem("Amplitude")) {
                ImPlot::SetNextPlotLimits(0,500,0,0.5);
                if (ImPlot::BeginPlot("##Amp","Frequency [Hz]","Amplitude", ImVec2(-1,-1))) {
                    ImPlot::SetLegendLocation(ImPlotLocation_NorthEast);
                    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.75f);
                    ImPlot::PlotShaded("x(f)",result.f.data(),result.ampx.data(),(int)result.f.size());
                    ImPlot::PlotLine("x(f)",result.f.data(),result.ampx.data(),(int)result.f.size());
                    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.75f);
                    ImPlot::PlotShaded("y(f)",result.f.data(),result.ampy.data(),(int)result.f.size());
                    ImPlot::PlotLine("y(f)",result.f.data(),result.ampy.data(),(int)result.f.size());
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Power")) {
                static std::vector<double> pxx10, pyy10;
                pxx10.resize(result.pxx.size());
                pyy10.resize(result.pyy.size());
                for (int i = 0; i < pxx10.size(); ++i) {
                    pxx10[i] = 10*std::log10(result.pxx[i]);
                    pyy10[i] = 10*std::log10(result.pyy[i]);
                }
                ImPlot::SetNextPlotLimits(0,500,-100,0);
                if (ImPlot::BeginPlot("##Power","Frequency [Hz]","PSD (dB/Hz)",ImVec2(-1,-1))) {
                    ImPlot::SetLegendLocation(ImPlotLocation_NorthEast);
                    ImPlot::PlotLine("x(f)",result.f.data(),pxx10.data(),(int)result.f.size());
                    ImPlot::PlotLine("y(f)",result.f.data(),pyy10.data(),(int)result.f.size());
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }
  
            ImGui::EndTabBar();
        }   
        
        ImGui::EndChild();

        ImGui::End();
        if (!open)
            quit();
    }

    void styleGui() {

        ImVec4 dark_accent  = ImVec4(0.00f, 0.70f, 0.16f, 1.00f); //(0.700f, 0.302f, 0.000f, 1.000f)
        ImVec4 light_accent = ImVec4(0.50f, 1.00f, 0.00f, 1.00f); //(1.000f, 0.630f, 0.000f, 1.000f)

        auto& style = ImGui::GetStyle();
        style.WindowPadding = {6,6};
        style.FramePadding  = {6,3};
        style.CellPadding   = {6,3};
        style.ItemSpacing   = {6,6};
        style.ItemInnerSpacing = {6,6};
        style.ScrollbarSize = 16;
        style.GrabMinSize = 8;
        style.WindowBorderSize = style.ChildBorderSize = style.PopupBorderSize = style.FrameBorderSize = style.TabBorderSize = 0;
        style.WindowRounding = style.ChildRounding = style.PopupRounding = style.ScrollbarRounding = style.GrabRounding = style.TabRounding = 4;


        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text]                   = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.21f, 0.21f, 0.24f, 1.00f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.07f, 0.07f, 0.10f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.21f, 0.21f, 0.24f, 1.00f);
        colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
        colors[ImGuiCol_FrameBgHovered]         = dark_accent;
        colors[ImGuiCol_FrameBgActive]          = light_accent;
        colors[ImGuiCol_TitleBg]                = dark_accent;
        colors[ImGuiCol_TitleBgActive]          = dark_accent;
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.21f, 0.21f, 0.24f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_CheckMark]              = dark_accent;
        colors[ImGuiCol_SliderGrab]             = dark_accent;
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_Button]                 = dark_accent;
        colors[ImGuiCol_ButtonHovered]          = dark_accent;
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_Separator]              = dark_accent;
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
        colors[ImGuiCol_ResizeGripHovered]      = dark_accent;
        colors[ImGuiCol_ResizeGripActive]       = light_accent;
        colors[ImGuiCol_Tab]                    = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
        colors[ImGuiCol_TabActive]              = dark_accent;
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
        colors[ImGuiCol_DockingPreview]         = ImVec4(0.85f, 0.85f, 0.85f, 0.28f);
        colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);

        ImVec4* pcolors = ImPlot::GetStyle().Colors;
        pcolors[ImPlotCol_Line]          = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_Fill]          = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_MarkerOutline] = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_MarkerFill]    = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_ErrorBar]      = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_FrameBg]       = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_PlotBg]        = ImVec4(0.07f, 0.07f, 0.10f, 0.00f);
        pcolors[ImPlotCol_PlotBorder]    = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        pcolors[ImPlotCol_LegendBg]      = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_LegendBorder]  = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_LegendText]    = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_TitleText]     = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_InlayText]     = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_XAxis]         = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_XAxisGrid]     = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_YAxis]         = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_YAxisGrid]     = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_YAxis2]        = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_YAxisGrid2]    = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_YAxis3]        = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_YAxisGrid3]    = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_Selection]     = ImVec4(1.00f, 0.00f, 0.91f, 1.00f);
        pcolors[ImPlotCol_Query]         = IMPLOT_AUTO_COL;
        pcolors[ImPlotCol_Crosshairs]    = IMPLOT_AUTO_COL;

        static const ImVec4 colormap[2] = {
            dark_accent,
            light_accent
        };
        ImPlot::SetColormap(colormap,2);

        auto& io = ImGui::GetIO();
        io.Fonts->Clear();
        ImFontConfig font_cfg;
        font_cfg.PixelSnapH           = true;
        font_cfg.OversampleH          = 1;
        font_cfg.OversampleV          = 1;
        font_cfg.FontDataOwnedByAtlas = false;
        strcpy(font_cfg.Name, "Roboto Bold");
        io.Fonts->AddFontFromMemoryTTF(Roboto_Bold_ttf, Roboto_Bold_ttf_len, 15.0f, &font_cfg);

    }

};

int main(int, char**) { 
    FilterToy toy;
    toy.run();
}