#pragma once

#include <Vector/Vectors.h>
#include <imgui/imgui.h>

#include "icons/icons.h"

using namespace ImGui;
using namespace std;

#include "include/includes.h"

#include "game.h"
#include "game/Ruleset.h"
#include "imgui/inc/8bp.h"
#include "mod/keylogin.h"
#include "oxorany/oxorany.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <sys/system_properties.h>
#include <ctime>

struct MenuState {
    bool isOpen = false;
    int currentTab = 0;
    float sidebarWidth = 750.0f;
    float animProgress = 0.0f;
    float menuAlpha = 0.0f;   // 0..1 — يتحرك بـ animation حتى عند الإغلاق (fade-out)
    float menuScale = 0.9f;
    float tabIndicatorX = 0.0f; // موقع indicator أفقي لـ animation
    ImVec4 accentColor = ImVec4(0.35f, 0.65f, 0.95f, 1.0f);
};
static MenuState g_menu;

// ─── Root / Access Detection ──────────────────────────────────────────────────
#include <unistd.h>

enum class RootType { NONE, MAGISK, KSU, MAGISK_AND_KSU };

static RootType DetectRootType() {
    bool hasMagisk = (access("/sbin/.magisk",    F_OK) == 0) ||
                     (access("/data/adb/magisk",  F_OK) == 0) ||
                     (access("/data/adb/magisk.db", F_OK) == 0);
    bool hasKSU    = (access("/data/adb/ksu",     F_OK) == 0) ||
                     (access("/sys/kernel/ksu",    F_OK) == 0) ||
                     (access("/data/adb/ksud",     F_OK) == 0);
    if (hasMagisk && hasKSU) return RootType::MAGISK_AND_KSU;
    if (hasMagisk)           return RootType::MAGISK;
    if (hasKSU)              return RootType::KSU;
    return RootType::NONE;
}

static const char* RootTypeLabel(RootType t) {
    switch (t) {
        case RootType::MAGISK:         return "Magisk";
        case RootType::KSU:            return "KernelSU";
        case RootType::MAGISK_AND_KSU: return "Magisk + KSU";
        default:                       return "No Root";
    }
}

static ImVec4 RootTypeColor(RootType t) {
    switch (t) {
        case RootType::MAGISK:         return ImVec4(1.0f, 0.6f, 0.0f, 1.0f); // برتقالي
        case RootType::KSU:            return ImVec4(0.3f, 0.8f, 1.0f, 1.0f); // أزرق فاتح
        case RootType::MAGISK_AND_KSU: return ImVec4(0.4f, 1.0f, 0.5f, 1.0f); // أخضر
        default:                       return ImVec4(0.6f, 0.6f, 0.65f, 1.0f); // رمادي
    }
}
// ─────────────────────────────────────────────────────────────────────────────

// Obfuscated expiry date: 13.04.2026 00:00:00 UTC
static const int64_t EXPIRY_TS = O(1780099200LL);

static bool DEBUG_BYPASS_LOGIN = true;

static float EaseOutBack(float x) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(x - 1.0f, 3.0f) + c1 * powf(x - 1.0f, 2.0f);
}

static float EaseOutQuart(float x) {
    return 1.0f - powf(1.0f - x, 4.0f);
}

static void DrawGradientRect(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col1, ImU32 col2, bool horizontal = true) {
    if (horizontal) {
        dl->AddRectFilledMultiColor(p1, p2, col1, col2, col2, col1);
    } else {
        dl->AddRectFilledMultiColor(p1, p2, col1, col1, col2, col2);
    }
}

// ─── Tab button للـ Bottom Navigation الجديدة ────────────────────────────────
static bool NavTabButton(const char* label, GLuint iconTex, bool selected,
                         float btnW, float btnH, float indicatorAnimX, bool isFirst) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos  = window->DC.CursorPos;
    ImVec2 size = ImVec2(btnW, btnH);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;

    // Hover glow
    if (hovered && !selected) {
        dl->AddRectFilled(bb.Min, bb.Max,
            IM_COL32(255,255,255, 18), 0.0f);
    }

    float iconSize = 52.0f;
    float vPad     = 14.0f;
    float fontSize = g.FontSize * 0.85f;

    ImVec2 center(bb.Min.x + btnW * 0.5f, bb.Min.y + vPad + iconSize * 0.5f);

    // Selected: glowing pill behind icon
    if (selected) {
        float pillW = iconSize + 28.0f;
        float pillH = iconSize + 14.0f;
        // Shadow
        dl->AddRectFilled(
            ImVec2(center.x - pillW*0.5f + 3, center.y - pillH*0.5f + 4),
            ImVec2(center.x + pillW*0.5f + 3, center.y + pillH*0.5f + 4),
            IM_COL32(220, 30, 30, 60), 16.0f);
        // Pill
        dl->AddRectFilled(
            ImVec2(center.x - pillW*0.5f, center.y - pillH*0.5f),
            ImVec2(center.x + pillW*0.5f, center.y + pillH*0.5f),
            IM_COL32(220, 40, 40, 255), 16.0f);
        // Inner highlight
        dl->AddRectFilled(
            ImVec2(center.x - pillW*0.5f + 2, center.y - pillH*0.5f + 2),
            ImVec2(center.x + pillW*0.5f - 2, center.y - pillH*0.5f + 8),
            IM_COL32(255, 100, 100, 80), 12.0f);
    }

    // Icon
    if (iconTex) {
        ImVec2 iMin = ImVec2(center.x - iconSize*0.5f, center.y - iconSize*0.5f);
        ImVec2 iMax = ImVec2(center.x + iconSize*0.5f, center.y + iconSize*0.5f);
        ImU32 tint  = selected ? IM_COL32(255,255,255,255) : IM_COL32(160,160,170,200);
        dl->AddImage((void*)(intptr_t)iconTex, iMin, iMax,
                     ImVec2(0,0), ImVec2(1,1), tint);
    }

    // Label
    const char* shortLabel = label;
    ImVec2 ts = CalcTextSize(shortLabel);
    ImVec2 tp = ImVec2(bb.Min.x + (btnW - ts.x)*0.5f,
                       bb.Min.y + vPad + iconSize + 6.0f);
    ImU32 textCol = selected
        ? IM_COL32(255, 255, 255, 255)
        : IM_COL32(120, 120, 135, 220);
    dl->AddText(tp, textCol, shortLabel);

    return pressed;
}
// ─────────────────────────────────────────────────────────────────────────────

static bool ToggleSwitch(const char* label, bool* v) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float scale = 1.5f; // 1.5f is with 50% bigger than writen values
    float height = 32.0f * scale;
    float width = 56.0f * scale;
    float radius = height * 0.5f;

    ImVec2 textSize = CalcTextSize(label);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, ImMax(height, textSize.y) + style.FramePadding.y * 2 + 10.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) *v = !*v;

    static std::map<ImGuiID, float> switchAnim;
    float& animT = switchAnim[id];
    float targetT = *v ? 1.0f : 0.0f;
    animT += (targetT - animT) * g.IO.DeltaTime * 14.0f;

    ImDrawList* dl = window->DrawList;
    
    if (hovered) {
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(45, 45, 55, 100), 10.0f);
    }
    
    ImVec2 togglePos = ImVec2(bb.Max.x - width - 15.0f, bb.Min.y + (size.y - height) * 0.5f);
    ImVec2 toggleEnd = ImVec2(togglePos.x + width, togglePos.y + height);
    
    ImVec4 offColor = ImVec4(0.27f, 0.27f, 0.31f, 1.0f);
    ImVec4 onColor = ImVec4(1.0f, 0.f, 0.f, 1.0f);
    ImVec4 bgColorV = ImLerp(offColor, onColor, animT);
    dl->AddRectFilled(togglePos, toggleEnd, ImColor(bgColorV), radius);
    
    float knobX = togglePos.x + radius + (width - height) * animT;
    float knobY = togglePos.y + radius;
    float knobR = radius - 4.0f;
    
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR + 2.0f, IM_COL32(0, 0, 0, 40));
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, IM_COL32(255, 255, 255, 255));

    dl->AddText(ImVec2(bb.Min.x + 15.0f, bb.Min.y + (size.y - textSize.y) * 0.5f), IM_COL32(230, 230, 240, 255), label);

    return pressed;
}

// File-scope so DrawToggleButton cancel can also reset countdown
static bool g_aqCounting = false;
static std::chrono::steady_clock::time_point g_aqLastCall;
static std::chrono::steady_clock::time_point g_aqCountdownStart;


static bool IsExpired() {
    return (int64_t)time(nullptr) >= EXPIRY_TS;
}

INLINE void DrawExpired(ImGuiIO& io) {
    float winW = g_menu.sidebarWidth;

    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    SetNextWindowSize(ImVec2(winW, 0), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(21, 21, 21, 255));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30.0f, 30.0f));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##ExpiredWin"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
              ImGuiWindowFlags_AlwaysAutoResize)) {

        SetWindowFontScale(1.6f);
        ImVec2 titleSz = CalcTextSize(O("MOD EXPIRED"));
        SetCursorPosX((winW - 60.0f - titleSz.x) * 0.5f);
        TextColored(ImVec4(1.0f, 0.1f, 0.1f, 1.0f), "%s", O("MOD EXPIRED"));
        SetWindowFontScale(1.0f);

        Dummy(ImVec2(0, 16));

        PushTextWrapPos(GetCursorPosX() + winW - 60.0f);
        TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), "%s",
            O("Beta Version Expired. Update on our Telegram @A_KOJO"));
        PopTextWrapPos();

        Dummy(ImVec2(0, 10));
    }
    End();
    PopStyleVar(3);
    PopStyleColor();
}

INLINE void DrawAutoQueue() {
    if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - g_aqLastCall).count() > 500)
            g_aqCounting = false;
        g_aqLastCall = now;

        if (!g_aqCounting) {
            g_aqCounting = true;
            g_aqCountdownStart = now;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_aqCountdownStart).count();
        int remaining_ms = 8000 - (int)elapsed;

      // if (remaining_ms <= 0) {
           // if (sharedMenuManager.getMenuStateId() == 13) PopMenuState(13);
            //StartLastMatch();
           // g_aqCounting = false;
          //  return;
       // }

        std::string count_str = std::to_string((remaining_ms / 1000) + 1);

        // Minimal auto-sized window, transparent bg — we draw our own rounded rect
        SetNextWindowPos(ImVec2(Width * 0.5f, Height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1.f));
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32.0f, 20.0f));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 24.0f);

        if (Begin(O("##AutoQueueCD"), nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                  ImGuiWindowFlags_AlwaysAutoResize)) {
            ImDrawList* dl  = GetWindowDrawList();
            ImVec2      wp  = GetWindowPos();
            ImVec2      ws  = GetWindowSize();
            dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), IM_COL32(20, 20, 28, 0), 24.0f);

            SetWindowFontScale(3.5f);
            TextColored(ImVec4(1.f, 0.f, 0.f, 1.0f), "%s", count_str.c_str());
            SetWindowFontScale(1.0f);
        }
        End();
        PopStyleVar(2);
        PopStyleColor();
    }
}

#include "mod/ButtonClicker.h"

static void DrawToggleButton(bool cancelMode); // forward declaration — defined after DrawFloatingButton

INLINE void DrawESP(ImDrawList* draw) {
    if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        if (!sharedGameManager) return;

        UpdateScreenTable();

        sharedDirector = F(ptr, libmain + O(0x4f06288));
        if (!sharedDirector) return;

        sharedUserInfo = F(ptr, libmain + O(0x4e9feb8));
        if (!sharedUserInfo) return;

        F(bool, sharedUserInfo + 0x340) = true;

        sharedMainManager = F(ptr, libmain + O(0x4dde3e0));
        if (!sharedMainManager) return;

        sharedMenuManager = F(ptr, libmain + O(0x4dfe838));
        if (!sharedMenuManager) return;

        MainStateManager mainStateManager = sharedMainManager.mStateManager;
        if (!mainStateManager) return;
        if (!mainStateManager.isInGame()) {
        if (persistent_bool[O("bAutoQueue")]) {
            if (!sharedMenuManager.isInQueue()) DrawAutoQueue();
            DrawToggleButton(true);  // acts as cancel button for autoqueue
        } return;
        }

        auto visualCue = sharedGameManager.mVisualCue();

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();

        Table table = sharedGameManager.mTable;
        if (!table) return;

        auto tableProperties = table.mTableProperties();
        if (!tableProperties) return;

        auto& pockets = tableProperties.mPockets();

        if (persistent_bool[O("bESP_DrawPockets")]) {
            for (int i = 0; i < 6; i++) {
                auto screenPos = WorldToScreen(pockets[i]);
                draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 40, WHITE, 0, 3.f);
            }
        }

        GameStateManager gameStateManager = sharedGameManager.mStateManager;
        if (!gameStateManager) return;

        if (persistent_bool[O("bAutoPlay")]) {
            DrawToggleButton(false);
            // ── AutoPlay: تنفيذ الـ auto aim & shoot ──────────────────────
            // نفعّل Auto Aim فقط لو الـ state مناسب (player's turn = state 2 or 3)
            auto apStateId = gameStateManager.getCurrentStateId();
            if ((apStateId == 2 || apStateId == 3) && gPrediction && gPrediction->guiData.ballsCount > 0) {
                // تسليم التحكم لـ AutoAim إذا كان متاحاً
                // AutoPlay::Update(); // uncomment عند تفعيل الـ namespace
            }
        }
        //if (persistent_bool[O("bAutoAim")]) AutoAim::AIM();

        auto stateId = gameStateManager.getCurrentStateId();
        if (stateId == 4) gPrediction->determineShotResult(false);
        if (stateId == 6 || stateId == 7 || stateId == 8) return;

        if (persistent_bool[O("bESP_DrawPocketsShotState")]) {
            for (int i = 0; i < 6; i++) {
                if (Prediction::pocketStatus[i]) {
                    auto screenPos = WorldToScreen(pockets[i]);
                    draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 30, GREEN, 0, 5.f);
                }
            }
        }

        if (persistent_bool[O("bESP_DrawPredictionLine")]) {
            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];

                if (ball.initialPosition != ball.predictedPosition) {
                    ImVec2 lastPos{};
                    float lineThick = (float)persistent_int[O("iLineThickness")];
                    if (lineThick < 1.f) lineThick = 1.f;
                    for (int j = 1; j < ball.positions.size(); j++) {
                        auto point = WorldToScreen(ball.positions[j]);
                        if (lastPos.x || lastPos.y) draw->AddLine(lastPos, point, colors[i], lineThick);
                        lastPos = point;
                    }
                }
            }
        }

        if (persistent_bool[O("bESP_DrawPredictionLine")]) {
            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];

                if (ball.initialPosition != ball.predictedPosition) {
                    float circleR = (float)persistent_int[O("iLineThickness")] + 1.f;
                    if (circleR < 2.f) circleR = 2.f;
                    draw->AddCircleFilled(WorldToScreen(ball.initialPosition), circleR, colors[i]);
                    draw->AddCircleFilled(WorldToScreen(ball.predictedPosition), 16, colors[i]);
                }
            }
        }
    }
}

static void DrawSidebar(float sidebarW) {
    static GLuint draw_icon_tex = LoadTextureFromMemory(draw_icon_png, draw_icon_png_len);
    static GLuint play_icon_tex = LoadTextureFromMemory(play_icon_png, play_icon_png_len);
    static GLuint q_icon_tex    = LoadTextureFromMemory(q_icon_png,    q_icon_png_len);
    static GLuint user_icon_tex = LoadTextureFromMemory(user_icon_png, user_icon_png_len);

    // ─── animation لمؤشر التبويب الحالي ─────────────────────────────────
    static float s_indX = 0.0f;   // X الحالي للمؤشر (interpolated)
    static float s_indTarget = 0.0f;

    ImGuiContext& g  = *GImGui;
    ImDrawList*   dl = GetWindowDrawList();
    ImVec2        wp = GetWindowPos();

    const int   nTabs  = 4;
    float iconSize  = 52.0f;
    float vPad      = 14.0f;
    float navBarH   = vPad + iconSize + 6.0f + g.FontSize + vPad;
    float btnW      = (sidebarW - 70.0f) / (float)nTabs;  // 70px للـ X button

    // ─── خلفية الـ Nav Bar ────────────────────────────────────────────────
    // Gradient غامق من أعلى لأسفل
    dl->AddRectFilledMultiColor(
        wp,
        ImVec2(wp.x + sidebarW, wp.y + navBarH),
        IM_COL32(28, 28, 35, 255), IM_COL32(28, 28, 35, 255),
        IM_COL32(18, 18, 24, 255), IM_COL32(18, 18, 24, 255)
    );
    // خط سفلي مضيء
    dl->AddLine(
        ImVec2(wp.x,           wp.y + navBarH - 1),
        ImVec2(wp.x + sidebarW, wp.y + navBarH - 1),
        IM_COL32(255, 60, 60, 60), 1.5f
    );

    // ─── Sliding Indicator تحت الـ tab النشط ─────────────────────────────
    float targetIndX = wp.x + g_menu.currentTab * btnW + btnW * 0.25f;
    s_indX += (targetIndX - s_indX) * g.IO.DeltaTime * 16.0f;
    float indW = btnW * 0.5f;
    float indH = 3.5f;
    float indY = wp.y + navBarH - indH - 1.0f;
    // glow
    dl->AddRectFilled(
        ImVec2(s_indX - 4, indY - 2),
        ImVec2(s_indX + indW + 4, indY + indH + 2),
        IM_COL32(255, 60, 60, 40), 4.0f
    );
    dl->AddRectFilled(
        ImVec2(s_indX, indY),
        ImVec2(s_indX + indW, indY + indH),
        IM_COL32(255, 80, 80, 255), 4.0f
    );

    // ─── Tab Buttons ─────────────────────────────────────────────────────
    SetCursorPos(ImVec2(0.0f, 0.0f));
    if (NavTabButton("Draw",  draw_icon_tex, g_menu.currentTab==0, btnW, navBarH, s_indX, true))
        g_menu.currentTab = 0;
    SameLine(0, 0);
    if (NavTabButton("Play",  play_icon_tex, g_menu.currentTab==1, btnW, navBarH, s_indX, false))
        g_menu.currentTab = 1;
    SameLine(0, 0);
    if (NavTabButton("Queue", q_icon_tex,    g_menu.currentTab==2, btnW, navBarH, s_indX, false))
        g_menu.currentTab = 2;
    SameLine(0, 0);
    if (NavTabButton("User",  user_icon_tex, g_menu.currentTab==3, btnW, navBarH, s_indX, false))
        g_menu.currentTab = 3;

    // ─── Close (X) strip ─────────────────────────────────────────────────
    float closeStripX = btnW * nTabs;
    float closeSize   = 34.0f;

    // فاصل رأسي شفاف
    float sepX = wp.x + closeStripX;
    dl->AddLine(
        ImVec2(sepX, wp.y + navBarH * 0.15f),
        ImVec2(sepX, wp.y + navBarH * 0.85f),
        IM_COL32(80, 80, 100, 150), 1.0f
    );

    SameLine(0, 0);
    {
        ImGuiWindow* win   = GetCurrentWindow();
        ImGuiID      closeId = win->GetID("##CloseMenu");
        float        cPosY = (navBarH - closeSize) * 0.5f;
        SetCursorPos(ImVec2(closeStripX + (70.0f - closeSize) * 0.5f, cPosY));

        ImVec2 closePos = win->DC.CursorPos;
        ImRect closeBb(closePos, closePos + ImVec2(closeSize, closeSize));
        ItemSize(ImVec2(closeSize, closeSize), g.Style.FramePadding.y);
        ItemAdd(closeBb, closeId);

        bool closeHov = false, closeHeld = false;
        bool closePressed = ButtonBehavior(closeBb, closeId, &closeHov, &closeHeld);
        if (closePressed) g_menu.isOpen = false;

        // دائرة خلف X عند hover
        ImVec2 cc = ImVec2(closeBb.Min.x + closeSize*0.5f, closeBb.Min.y + closeSize*0.5f);
        if (closeHov)
            dl->AddCircleFilled(cc, closeSize*0.5f, IM_COL32(255, 60, 60, 60));

        float xH  = closeSize * 0.28f;
        ImU32 xCol = closeHov
            ? IM_COL32(255, 100, 100, 255)
            : IM_COL32(140, 140, 155, 200);
        dl->AddLine(ImVec2(cc.x - xH, cc.y - xH), ImVec2(cc.x + xH, cc.y + xH), xCol, 2.5f);
        dl->AddLine(ImVec2(cc.x + xH, cc.y - xH), ImVec2(cc.x - xH, cc.y + xH), xCol, 2.5f);
    }

    // Push cursor past the nav bar
    SetCursorPos(ImVec2(0.0f, navBarH));
    Dummy(ImVec2(sidebarW, 6.0f));
}

// Reads an IL2CPP/Unity NSString (UTF-16 internal buffer at offset 0x14, length at 0x10)
static std::string ReadNSString(ptr str) {
    if (!str) return "null";
    int32_t len = F(int32_t, str + 0x10);
    if (len <= 0 || len > 512) return "?";
    std::string result;
    result.reserve(len);
    for (int32_t i = 0; i < len; i++) {
        uint16_t ch = F(uint16_t, str + 0x14 + i * 2);
        result += (ch > 0 && ch < 128) ? (char)ch : '?';
    }
    return result;
}

// Shared vertical position for DrawToggleButton and DrawFloatingButton (they move together)
static float g_sideBtnsY      = 0.0f;
// Kept for linker compatibility — no longer used for animation
static float g_toggleRotAngle = 0.0f;
// Set true by AutoPlay when in SLOW scan state — shows CALCULATING overlay
static bool  g_autoPlayCalculating = false;

// ── svConfig ──────────────────────────────────────────────────────────────────
static void svConfig_Save() {
    std::string path = O("/data/user/0/") + PACKAGE_NAME + O("/files/svConfig.txt");
    FILE* f = fopen(path.c_str(), O("w"));
    if (!f) return;
    fprintf(f, O("iLineThickness=%d\n"),  persistent_int[O("iLineThickness")]);
    fprintf(f, O("iMenuSizeOffset=%d\n"), persistent_int[O("iMenuSizeOffset")]);
    fclose(f);
}
static void svConfig_Load() {
    std::string path = O("/data/user/0/") + PACKAGE_NAME + O("/files/svConfig.txt");
    FILE* f = fopen(path.c_str(), O("r"));
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        int v = 0;
        if (sscanf(line, O("iLineThickness=%d"),  &v) == 1) { persistent_int[O("iLineThickness")]  = v; continue; }
        if (sscanf(line, O("iMenuSizeOffset=%d"), &v) == 1) { persistent_int[O("iMenuSizeOffset")] = v; }
    }
    fclose(f);
}

// ── CALCULATING overlay (shown during AutoPlay SLOW scan) ─────────────────────
static void DrawCalculating(ImGuiIO& io) {
    // Setăm poziția pe centrul ecranului (Width*0.5, Height*0.5)
    // Pivotul (0.5f, 0.5f) înseamnă că mijlocul ferestrei va fi fix pe coordonatele date
    SetNextWindowPos(ImVec2(Width * 0.5f, Height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    // Auto-resize face ca fereastra să aibă dimensiunea textului automat
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(21, 21, 21, 255));
    PushStyleColor(ImGuiCol_Border, IM_COL32(220, 30, 30, 255));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

    if (Begin(O("##CalcOverlay"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | 
              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs)) {
        
        SetWindowFontScale(1.4f);
        TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), O("CALCULATING..."));
        SetWindowFontScale(1.0f);
    }
    End();
    PopStyleVar(2);
    PopStyleColor(2);
}


static void DrawContentArea(float winW, float winH) {
    bool need_save = false;
    
    ImDrawList* dl  = GetWindowDrawList();
    ImVec2      wp  = GetWindowPos();

    // startY este punctul unde se termină bara de butoane (Sidebar)
    float startY   = GetCursorPosY();
    float contentW = winW;

    // Desenăm fundalul zonei de conținut sub sidebar
    dl->AddRectFilled(
        ImVec2(wp.x, wp.y + startY),
        ImVec2(wp.x + contentW, wp.y + winH),
        IM_COL32(16, 16, 22, 255), 20.0f
    );
    
    const char* tabTitles[] = { 
    O("Draw Settings"), 
    O("Auto Play"), 
    O("Auto Queue"), 
    O("User") 
};

    // --- CENTRARE TITLU TAB ---
    const char* currentTitle = tabTitles[g_menu.currentTab];
    float titlePadT = 18.0f;
    float titlePadB = 12.0f;

    // 1. Setăm scara fontului înainte de calcul
    SetWindowFontScale(1.15f);
    ImVec2 ts = CalcTextSize(currentTitle);
    
    // 2. Calculăm X pentru centrare: (Lățime fereastră - Lățime text) / 2
    float centeredX = (contentW - ts.x) * 0.5f;
    SetCursorPos(ImVec2(centeredX, startY + titlePadT));
    
    // 3. Afișăm textul
    TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", currentTitle);
    SetWindowFontScale(1.0f); // Resetăm imediat

    // Linie separatoare centrată și ea (lăsăm 20px margini)
    float lineY = startY + titlePadT + ts.y + titlePadB;
    dl->AddLine(
        ImVec2(wp.x + 20.0f, wp.y + lineY),
        ImVec2(wp.x + contentW - 20.0f, wp.y + lineY),
        IM_COL32(60, 60, 75, 150), 1.0f
    );

    float headerH = (lineY - startY) + 10.0f;
    SetCursorPos(ImVec2(10.0f, startY + headerH));
    
    // Începutul zonei de child (conținutul propriu-zis)
    PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    BeginChild(O("##ContentArea"), ImVec2(contentW - 20.0f, winH - startY - headerH - 10.0f), false);
    
    switch (g_menu.currentTab) {
        case 0: {
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Draw Lines"), &persistent_bool[O("bESP_DrawPredictionLine")]);
           // need_save |= ToggleSwitch(O("Draw Pockets"), &persistent_bool[O("bESP_DrawPockets")]);
            need_save |= ToggleSwitch(O("Draw Pockets"), &persistent_bool[O("bESP_DrawPocketsShotState")]);

            Dummy(ImVec2(0, 16));
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Line Thickness"));
            Dummy(ImVec2(0, 8));
            {
                if (persistent_int[O("iLineThickness")] < 1) persistent_int[O("iLineThickness")] = 4;
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0, 0, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0, 0, 1.0f));
                SetNextItemWidth(GetContentRegionAvail().x);
                need_save |= SliderInt(O("##lineThick"), &persistent_int[O("iLineThickness")], 1, 10, "%d");
                PopStyleColor(3);
                PopStyleVar(2);
            }

            Dummy(ImVec2(0, 16));
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Fix Menu Size"));
            Dummy(ImVec2(0, 8));
            {
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0, 0, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0, 0, 1.0f));
                SetNextItemWidth(GetContentRegionAvail().x);
                int& menuSz = persistent_int[O("iMenuSizeOffset")];
                bool changed = SliderInt(O("##menuSize"), &menuSz, -10, 10,
                    menuSz == 0 ? O("Normal") : "%d");
                need_save |= changed;
                PopStyleColor(3);
                PopStyleVar(2);
            }

            Dummy(ImVec2(0, 20));
            {
                PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
                PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.55f, 0.20f, 1.0f));
                PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.68f, 0.26f, 1.0f));
                PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.09f, 0.42f, 0.15f, 1.0f));
                if (Button(O("Save Config"), ImVec2(GetContentRegionAvail().x, 55.0f))) {
                    svConfig_Save();
                }
                PopStyleColor(3);
                PopStyleVar();
            }
            break;
        }
        
        case 1: {
            // ═══════════════════════════════════════════════════════════════
            // ─── AutoPlay Tab ──────────────────────────────────────────────
            // ═══════════════════════════════════════════════════════════════
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Enable AutoPlay"), &persistent_bool[O("bAutoPlay")]);

            Dummy(ImVec2(0, 18));

            // ── Status Card ──────────────────────────────────────────────
            {
                ImDrawList* cdl = GetWindowDrawList();
                ImVec2      csp = GetCursorScreenPos();
                float       cw  = GetContentRegionAvail().x;
                float       ch  = 80.0f;

                bool  active   = persistent_bool[O("bAutoPlay")];
                ImU32 cardBg   = active ? IM_COL32(25,70,30,255) : IM_COL32(35,35,45,255);
                ImU32 cardBord = active ? IM_COL32(55,190,75,200) : IM_COL32(65,65,85,180);

                cdl->AddRectFilled(csp, ImVec2(csp.x+cw, csp.y+ch), cardBg, 14.0f);
                cdl->AddRect(csp, ImVec2(csp.x+cw, csp.y+ch), cardBord, 14.0f, 0, 1.5f);

                float dotX = csp.x + 28.0f;
                float dotY = csp.y + ch * 0.5f;
                ImU32 dotC = active ? IM_COL32(55,225,85,255) : IM_COL32(130,130,145,255);
                cdl->AddCircleFilled(ImVec2(dotX, dotY), 8.0f, dotC);
                if (active) {
                    static float pulse = 0.0f;
                    pulse += GImGui->IO.DeltaTime * 3.0f;
                    float pr = 12.0f + sinf(pulse) * 4.0f;
                    cdl->AddCircle(ImVec2(dotX, dotY), pr,
                        IM_COL32(55,225,85,(int)(70+40*sinf(pulse))), 0, 1.5f);
                }

                float tx  = dotX + 22.0f;
                float ty1 = csp.y + ch*0.5f - GImGui->FontSize*0.65f;
                float ty2 = ty1 + GImGui->FontSize + 4.0f;
                cdl->AddText(ImVec2(tx, ty1),
                    active ? IM_COL32(110,235,125,255) : IM_COL32(150,150,165,255),
                    active ? O("AutoPlay ACTIVE") : O("AutoPlay OFF"));
                cdl->AddText(ImVec2(tx, ty2),
                    IM_COL32(95,95,110,200),
                    active ? O("Auto aiming and shooting enabled")
                           : O("Enable to start auto-play"));

                Dummy(ImVec2(0, ch + 10.0f));
            }

            // ── Settings (only when active) ────────────────────────────
            if (persistent_bool[O("bAutoPlay")]) {
                TextColored(ImVec4(0.75f,0.75f,0.8f,1.0f), O("Shot Delay (ms)"));
                Dummy(ImVec2(0, 6));
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_GrabRounding,  10.0f);
                PushStyleColor(ImGuiCol_FrameBg,         ImVec4(0.12f,0.12f,0.15f,1.0f));
                PushStyleColor(ImGuiCol_SliderGrab,      ImVec4(0.2f,0.8f,0.35f,1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(0.25f,0.95f,0.4f,1.0f));
                SetNextItemWidth(GetContentRegionAvail().x);
                if (persistent_int[O("iAutoPlayDelay")] < 50)
                    persistent_int[O("iAutoPlayDelay")] = 200;
                need_save |= SliderInt(O("##apDelay"),
                    &persistent_int[O("iAutoPlayDelay")], 50, 1000, "%d ms");
                PopStyleColor(3); PopStyleVar(2);

                Dummy(ImVec2(0, 14));
                need_save |= ToggleSwitch(O("Auto-Queue after win"),
                    &persistent_bool[O("bAutoPlay_AutoQueue")]);
            } else {
                Dummy(ImVec2(0, 16));
                TextColored(ImVec4(0.45f,0.45f,0.5f,1.0f),
                    O("AutoPlay aims and shoots automatically."));
                Dummy(ImVec2(0, 6));
                TextColored(ImVec4(0.38f,0.38f,0.43f,1.0f),
                    O("Toggle the switch above to activate."));
            }
            break;
        }
        
        case 2: {
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Enable AutoQueue"), &persistent_bool[O("bAutoQueue")]);
            Dummy(ImVec2(0, 20));
            
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Mode"));
            Dummy(ImVec2(0, 8));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 12));
            PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
            PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.16f, 0.16f, 0.20f, 1.0f));
            SetNextItemWidth(GetContentRegionAvail().x);
            need_save |= Combo("##mode", &persistent_int["iAutoQueue_Mode"], "Last Selected\0Smart\0Fix Table\0");
            PopStyleColor(2);
            PopStyleVar(2);
            
            if (persistent_int["iAutoQueue_Mode"] == 1) {
                Dummy(ImVec2(0, 15));
                TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Bet Percent"));
                Dummy(ImVec2(0, 8));
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0, 0, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0, 0, 1.0f));
                SetNextItemWidth(GetContentRegionAvail().x);
                need_save |= SliderInt("##betpercent", &persistent_int["iAutoQueue_BetPercent"], 1, 100, "%d%%");
                PopStyleColor(3);
                PopStyleVar(2);
            }

            if (persistent_int["iAutoQueue_Mode"] == 2) {
                Dummy(ImVec2(0, 15));
                TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Select Table"));
                Dummy(ImVec2(0, 8));

                struct TableEntry { const char* label; ImU32 bg; ImU32 bgHov; };
                static const TableEntry tables[17] = {
                    { "100",   IM_COL32( 55,  90, 200, 255), IM_COL32( 75, 110, 220, 255) }, // M1  Blue
                    { "200",   IM_COL32( 40, 150,  65, 255), IM_COL32( 55, 170,  80, 255) }, // M2  Green
                    { "1k",    IM_COL32( 55,  90, 200, 255), IM_COL32( 75, 110, 220, 255) }, // M3  Blue
                    { "2.5k",  IM_COL32(130,  25,  25, 255), IM_COL32(155,  40,  40, 255) }, // M4  Dark Red
                    { "10k",   IM_COL32( 35,  35,  38, 255), IM_COL32( 55,  55,  60, 255) }, // M5  Black
                    { "50k",   IM_COL32(110,   0,   0, 255), IM_COL32(135,  15,  15, 255) }, // M6  Maroon
                    { "100k",  IM_COL32(140, 140, 145, 255), IM_COL32(160, 160, 165, 255) }, // M7  Light Grey
                    { "500k",  IM_COL32(185, 160,   0, 255), IM_COL32(210, 185,  10, 255) }, // M8  Yellow
                    { "1M",    IM_COL32( 20,  45, 130, 255), IM_COL32( 35,  60, 155, 255) }, // M9  Dark Blue
                    { "2M",    IM_COL32(190,  90,  15, 255), IM_COL32(215, 110,  30, 255) }, // M10 Dark Orange
                    { "5M",    IM_COL32(  0, 148, 110, 255), IM_COL32( 15, 170, 128, 255) }, // M11 Emerald
                    { "8M",    IM_COL32(165,  65,  65, 255), IM_COL32(185,  85,  85, 255) }, // M12 Light Maroon
                    { "10M",   IM_COL32( 18,  90,  35, 255), IM_COL32( 30, 112,  50, 255) }, // M13 Dark Green
                    { "20M",   IM_COL32(100, 100, 110, 255), IM_COL32(120, 120, 130, 255) }, // M14 Grey
                    { "30M",   IM_COL32(130,  15,  35, 255), IM_COL32(155,  30,  50, 255) }, // M15 Red Maroon
                    { "50M",   IM_COL32(  0, 148, 110, 255), IM_COL32( 15, 170, 128, 255) }, // M16 Emerald
                    { "200M",  IM_COL32( 20,  45, 130, 255), IM_COL32( 35,  60, 155, 255) }, // M17 Dark Blue
                };

                int& selected = persistent_int["iAutoQueue_FixTable"];
                float avail   = GetContentRegionAvail().x;
                int   cols    = 4;
                float gap     = 8.0f;
                float btnW    = (avail - gap * (cols - 1)) / cols;
                float btnH    = 42.0f;

                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 6));

                for (int i = 0; i < 17; i++) {
                    if (i % cols != 0) SameLine(0, gap);

                    bool isSel = (selected == i);
                    ImU32 bgCol = isSel ? tables[i].bgHov : tables[i].bg;

                    PushStyleColor(ImGuiCol_Button,        (ImU32)bgCol);
                    PushStyleColor(ImGuiCol_ButtonHovered, (ImU32)tables[i].bgHov);
                    PushStyleColor(ImGuiCol_ButtonActive,  (ImU32)tables[i].bgHov);
                    PushStyleColor(ImGuiCol_Text,          isSel ? IM_COL32(255,255,255,255) : IM_COL32(220,220,220,200));

                    char btnId[32];
                    snprintf(btnId, sizeof(btnId), "%s##ft%d", tables[i].label, i);
                    if (Button(btnId, ImVec2(btnW, btnH))) {
                        selected = i;
                        need_save = true;
                    }

                    // Selected indicator: white outline
                    if (isSel) {
                        ImVec2 p = GetItemRectMin();
                        ImVec2 q = GetItemRectMax();
                        GetWindowDrawList()->AddRect(p, q, IM_COL32(255,255,255,200), 10.0f, 0, 2.0f);
                    }

                    PopStyleColor(4);
                }

                PopStyleVar(2);
            }

            if (persistent_int["iAutoQueue_Mode"] == 0) {
                Dummy(ImVec2(0, 15));
                TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("You will be auto queued to"));
                TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("the last game mode you played"));
            }
            break;
        }

        case 3: {
            // ── helpers ──────────────────────────────────────────────────────
            auto DrawSectionHeader = [&](const char* title) {
                Dummy(ImVec2(0, 14));
                float avail = GetContentRegionAvail().x;
                ImVec2 p    = GetCursorScreenPos();
                float  fs   = GImGui->FontSize;
                ImVec2 ts   = CalcTextSize(title);
                float  lineY = p.y + fs * 0.5f;
                float  gap   = 8.0f;
                float  lineW = (avail - ts.x - gap * 2.0f) * 0.5f;
                ImDrawList* dl2 = GetWindowDrawList();
                dl2->AddLine(ImVec2(p.x,                      lineY), ImVec2(p.x + lineW,                      lineY), IM_COL32(60,60,75,160), 1.0f);
                dl2->AddLine(ImVec2(p.x + lineW + gap + ts.x + gap, lineY), ImVec2(p.x + avail, lineY), IM_COL32(60,60,75,160), 1.0f);
                SetCursorPosX(GetCursorPosX() + lineW + gap);
                TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "%s", title);
                Dummy(ImVec2(0, 6));
            };

            auto DrawInfoRow = [&](const char* key, const char* val) {
                TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "%s", key);
                SameLine();
                TextColored(ImVec4(0.90f, 0.90f, 0.95f, 1.0f), "%s", val);
                Dummy(ImVec2(0, 4));
            };

          /*  // ── User Game Info ────────────────────────────────────────────────
            DrawSectionHeader(O("User Game Info"));

            if (sharedUserInfo) {
                DrawInfoRow(O("Coins:        "), ReadNSString(sharedUserInfo.coins()).c_str());
                DrawInfoRow(O("Cash:         "), ReadNSString(sharedUserInfo.cash()).c_str());
                DrawInfoRow(O("Display Name: "), ReadNSString(sharedUserInfo.DisplayName()).c_str());
                DrawInfoRow(O("Country Code: "), ReadNSString(sharedUserInfo.loginCountryCode()).c_str());
            } else {
                TextColored(ImVec4(0.6f, 0.3f, 0.3f, 1.0f), O("UserInfo not available"));
            }*/

            // ── Device Info ───────────────────────────────────────────────────
            DrawSectionHeader(O("Device"));
            {
                static char s_manufacturer[PROP_VALUE_MAX] = {};
                static char s_model[PROP_VALUE_MAX]        = {};
                static char s_abi[PROP_VALUE_MAX]          = {};
                static char s_android[PROP_VALUE_MAX]      = {};
                static bool s_props_loaded = false;
                if (!s_props_loaded) {
                    __system_property_get("ro.product.manufacturer", s_manufacturer);
                    __system_property_get("ro.product.model",        s_model);
                    __system_property_get("ro.product.cpu.abi",      s_abi);
                    __system_property_get("ro.build.version.release", s_android);
                    s_props_loaded = true;
                }
                DrawInfoRow(O("Manufacturer: "), s_manufacturer);
                DrawInfoRow(O("Model:        "), s_model);
                DrawInfoRow(O("ABI:          "), s_abi);
                DrawInfoRow(O("Android:      "), s_android);
            }

            // ── Root / Access Status ──────────────────────────────────────────
            DrawSectionHeader(O("Access"));
            {
                // نكشف مرة واحدة فقط ونحفظ النتيجة
                static RootType s_rootType    = RootType::NONE;
                static bool     s_rootChecked = false;
                if (!s_rootChecked) {
                    s_rootType    = DetectRootType();
                    s_rootChecked = true;
                }

                ImDrawList* rdl = GetWindowDrawList();
                ImVec2      rsp = GetCursorScreenPos();
                float       rw  = GetContentRegionAvail().x;
                float       rh  = 62.0f;

                // Card background حسب نوع الـ root
                ImU32 cardBg, cardBord;
                switch (s_rootType) {
                    case RootType::MAGISK:
                        cardBg   = IM_COL32(60, 35, 10, 255);
                        cardBord = IM_COL32(255, 150, 30, 200);
                        break;
                    case RootType::KSU:
                        cardBg   = IM_COL32(10, 35, 65, 255);
                        cardBord = IM_COL32(50, 160, 255, 200);
                        break;
                    case RootType::MAGISK_AND_KSU:
                        cardBg   = IM_COL32(15, 60, 20, 255);
                        cardBord = IM_COL32(60, 220, 90, 200);
                        break;
                    default:
                        cardBg   = IM_COL32(35, 35, 45, 255);
                        cardBord = IM_COL32(80, 80, 100, 180);
                        break;
                }
                rdl->AddRectFilled(rsp, ImVec2(rsp.x+rw, rsp.y+rh), cardBg, 12.0f);
                rdl->AddRect(rsp, ImVec2(rsp.x+rw, rsp.y+rh), cardBord, 12.0f, 0, 1.5f);

                // Icon (دائرة ملونة)
                float dotX = rsp.x + 26.0f;
                float dotY = rsp.y + rh * 0.5f;
                ImVec4 dotCol4 = RootTypeColor(s_rootType);
                rdl->AddCircleFilled(ImVec2(dotX, dotY), 9.0f,
                    IM_COL32((int)(dotCol4.x*255),(int)(dotCol4.y*255),
                             (int)(dotCol4.z*255),255));

                // Label
                float tx  = dotX + 22.0f;
                float ty1 = rsp.y + rh*0.5f - GImGui->FontSize*0.65f;
                float ty2 = ty1 + GImGui->FontSize + 4.0f;
                ImVec4 lc = RootTypeColor(s_rootType);
                rdl->AddText(ImVec2(tx, ty1), IM_COL32((int)(lc.x*255),(int)(lc.y*255),(int)(lc.z*255),255),
                    RootTypeLabel(s_rootType));
                rdl->AddText(ImVec2(tx, ty2), IM_COL32(110,110,125,200),
                    s_rootType == RootType::NONE
                        ? O("Running without root — normal mode")
                        : O("Root access detected — full features enabled"));

                Dummy(ImVec2(0, rh + 8.0f));
            }

            // ── AKRO GOXI Info ────────────────────────────────────────────────
            DrawSectionHeader(O("AKRO GOXI Info"));
            {
                int64_t now_ts   = (int64_t)time(nullptr);
                int64_t diff     = EXPIRY_TS - now_ts;

                char expireBuf[64];
                if (diff > 0) {
                    int64_t totalSecs = diff;
                    int days  = (int)(totalSecs / 86400);
                    int hours = (int)((totalSecs % 86400) / 3600);
                    int mins  = (int)((totalSecs % 3600)  / 60);
                    snprintf(expireBuf, sizeof(expireBuf), "%dd - %dh - %dm", days, hours, mins);
                } else {
                    snprintf(expireBuf, sizeof(expireBuf), "%s", O("Expired"));
                }

                DrawInfoRow(O("Expire:       "), expireBuf);
                Dummy(ImVec2(0, 6));
                TextColored(ImVec4(0, 1.f, 0, 1.0f), O("Owner By @A_KOJO"));
                Dummy(ImVec2(0, 8));
                PushTextWrapPos(GetContentRegionAvail().x + GetCursorPosX());
                TextColored(ImVec4(1.f, 0.f, 0.f, 1.0f),
                    O("Beware of Scammers. This is a FREE BETA version, if you bought this version it means you got scammed."));
                PopTextWrapPos();
            }
            break;
        }
    }
    
    if (need_save) save_persistence();
    
    EndChild();
    PopStyleColor();
}

INLINE void DrawMenu(ImGuiIO& io) {
    if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        if (is_segv_handler_active()) {
            jump_buffer_active = 1;
            if (!sigsetjmp(jump_buffer, 1)) DrawESP(GetBackgroundDrawList());
            jump_buffer_active = 0;
        }

        // ── إصلاح المشكلة: animation smooth للفتح والإغلاق ──────────────
        // عند الفتح: تسارع سريع  / عند الإغلاق: fade-out بدلاً من تصفير فوري
        float targetAlpha = g_menu.isOpen ? 1.0f : 0.0f;
        float speed       = g_menu.isOpen ? 12.0f : 10.0f;
        g_menu.menuAlpha += (targetAlpha - g_menu.menuAlpha) * io.DeltaTime * speed;
        if (g_menu.menuAlpha < 0.001f && !g_menu.isOpen)
            g_menu.menuAlpha = 0.0f; // snap to zero only when fully faded

        // ── رسم القائمة طالما alpha > 0 (حتى لو isOpen=false أثناء fade-out) ──
        if (g_menu.menuAlpha > 0.001f) {
            float sizeScale = 1.0f + (float)persistent_int[O("iMenuSizeOffset")] * 0.03f;
            if (sizeScale < 0.3f) sizeScale = 0.3f;
            float winW = g_menu.sidebarWidth * sizeScale;
            float winH = 560.0f * sizeScale;
            
            SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
            SetNextWindowPos(ImVec2(Width / 2.0f, Height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            
            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.13f, 0.f));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            PushStyleVar(ImGuiStyleVar_Alpha, g_menu.menuAlpha);
            
            ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            
            // نمرر isOpen حتى لا تتدخل ImGui في حالة الإغلاق
            bool dummyOpen = true;
            if (Begin(O("##MainMenu"), &dummyOpen, winFlags)) {
                DrawSidebar(winW);
                DrawContentArea(winW, winH);
            }
            End();
            
            PopStyleVar(4);
            PopStyleColor();
        }
    }
}

// Moved from AutoPlay namespace — plays/pauses autoplay (cancelMode=false)
// or cancels autoqueue (cancelMode=true)
static void DrawToggleButton(bool cancelMode) {
    ImGuiIO& io = GetIO();

    static GLuint play_on_tex       = LoadTextureFromMemory(play_on_png,       play_on_png_len);
    static GLuint play_off_tex      = LoadTextureFromMemory(play_off_png,       play_off_png_len);
    static GLuint queue_cancel_tex  = LoadTextureFromMemory(play_on_png,   play_on_png_len);

    float button_size   = 130.f;
    float winPadX       = GetStyle().WindowPadding.x;
    float winPadY       = GetStyle().WindowPadding.y;
    float windowWidth   = button_size + winPadX * 2.0f;
    float windowHeight  = button_size + winPadY * 2.0f;

    const float rightMargin  = 20.0f;
    float fixedX = io.DisplaySize.x - rightMargin - windowWidth;

    if (g_sideBtnsY == 0.0f)
        g_sideBtnsY = io.DisplaySize.y - 20.0f - windowHeight;

    SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(fixedX, g_sideBtnsY), ImGuiCond_Always);

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    PushStyleColor(ImGuiCol_Border,   IM_COL32(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 99.0f);

    if (Begin(O("##ToggleBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {

        ImVec2 pos    = GetCursorScreenPos();
        ImVec2 size(button_size, button_size);
        ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);

        bool clicked = InvisibleButton(O("##TglBtnHit"), size);

        // Pick texture based on state
        GLuint tex = cancelMode ? queue_cancel_tex : play_off_tex;
               //    : (AutoPlay::bAutoPlaying ? play_on_tex : play_off_tex);

        float r = size.x * 0.5f;
        ImDrawList* dl = GetWindowDrawList();
        dl->AddImage((void*)(intptr_t)tex,
            ImVec2(center.x - r, center.y - r),
            ImVec2(center.x + r, center.y + r));

        // Vertical-only drag
        if (IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left)) {
            g_sideBtnsY += io.MouseDelta.y;
            g_sideBtnsY = ImClamp(g_sideBtnsY, 0.0f, io.DisplaySize.y - windowHeight);
            SetWindowPos(ImVec2(fixedX, g_sideBtnsY), ImGuiCond_Always);
        }

        if (clicked) {
            if (cancelMode) {
                persistent_bool[O("bAutoQueue")] = false;
                g_aqCounting = false;
            } else {
             //   AutoPlay::bAutoPlaying = !AutoPlay::bAutoPlaying;
              //  if (AutoPlay::bAutoPlaying) AutoPlay::ClearState();
            }
        }
    }
    End();
    PopStyleVar();
    PopStyleColor(2);
}

static void DrawFloatingButton(ImGuiIO& io) {
    static GLuint logo_tex   = LoadTextureFromMemory(logo_png, logo_png_len);
    static bool   isDragging = false;

    float buttonRadius = 65.0f;
    float buttonSize   = buttonRadius * 2.0f;
    float winSize      = buttonSize + 10.0f;
    float margin       = 8.0f;

    float toggleWinH = GetFrameHeight() * 1.7f + GetStyle().WindowPadding.y * 2.0f;
    const float rightMargin = 20.0f;
    float toggleWinW = GetFrameHeight() * 1.7f + GetStyle().WindowPadding.x * 2.0f;
    float fixedX = io.DisplaySize.x - rightMargin - ImMax(winSize, toggleWinW);

    if (g_sideBtnsY == 0.0f)
        g_sideBtnsY = io.DisplaySize.y - 80.0f - toggleWinH;

    float posY = g_sideBtnsY - winSize - margin;

    SetNextWindowPos(ImVec2(fixedX, posY), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(winSize, winSize), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##FloatBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {

        ImDrawList* dl     = GetWindowDrawList();
        ImVec2      center = ImVec2(fixedX + buttonRadius + 5, posY + buttonRadius + 5);

        SetCursorPos(ImVec2(0, 0));
        InvisibleButton(O("##FloatBtnHit"), ImVec2(winSize, winSize));

        // Draw logo — no animations, fixed size
        dl->AddImage((void*)(intptr_t)logo_tex,
                     ImVec2(center.x - buttonRadius, center.y - buttonRadius),
                     ImVec2(center.x + buttonRadius, center.y + buttonRadius));

        // Vertical-only drag moves both buttons together via g_sideBtnsY
        if (IsItemActive() && IsMouseDragging(0)) {
            isDragging = true;
            g_sideBtnsY += io.MouseDelta.y;
            g_sideBtnsY = ImClamp(g_sideBtnsY, winSize + margin,
                                  io.DisplaySize.y - 80.0f - toggleWinH);
        }

        if (IsItemHovered() && IsMouseReleased(0) && !isDragging)
            g_menu.isOpen = !g_menu.isOpen;

        if (!IsItemActive()) isDragging = false;
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}


static bool first_time = true;
INLINE void DrawLogin(ImGuiIO& io) {
    if (logged_in) return DrawMenu(io);

    SetNextWindowPos(ImVec2(0, 0));
    SetNextWindowSize(io.DisplaySize);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.06f, 0.96f));
    Begin(O("##Overlay"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
    PopStyleColor();

    float cardW = 580;
    float cardH = 420;

    SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.14f, 1.0f));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    Begin(O("##LoginCard"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = GetWindowDrawList();
    ImVec2 winPos = GetWindowPos();
    
    DrawGradientRect(dl, winPos, ImVec2(winPos.x + cardW, winPos.y + 110), IM_COL32(35, 95, 170, 255), IM_COL32(55, 125, 200, 255), true);
    dl->AddRectFilled(winPos, ImVec2(winPos.x + cardW, winPos.y + 20), IM_COL32(35, 95, 170, 255), 20.0f, ImDrawFlags_RoundCornersTop);

    SetWindowFontScale(1.4f);
    ImVec2 titleSize = CalcTextSize(O("AKRO GOXI"));
    dl->AddText(ImVec2(winPos.x + (cardW - titleSize.x) * 0.5f, winPos.y + 30), IM_COL32(255, 255, 255, 255), O("Your Name"));
    SetWindowFontScale(1.0f);
    
    ImVec2 subSize = CalcTextSize(O("Premium Mod"));
    dl->AddText(ImVec2(winPos.x + (cardW - subSize.x) * 0.5f, winPos.y + 70), IM_COL32(200, 220, 255, 200), O("Premium Mod"));

    SetCursorPosY(130);

    if (!ERROR_MESSAGE.empty()) {
        SetCursorPosX(30);
        PushTextWrapPos(cardW - 30);
        TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", ERROR_MESSAGE.c_str());
        PopTextWrapPos();
        Dummy(ImVec2(0, 15));
    }

    if (is_logging_in) {
        SetCursorPosY(180);
        
        static float spinner_angle = 0.0f;
        spinner_angle += io.DeltaTime * 5.0f;

        float spinner_size = 40.0f;
        ImVec2 spinnerCenter = ImVec2(winPos.x + cardW * 0.5f, winPos.y + 220);

        for (int i = 0; i < 12; i++) {
            float angle = spinner_angle + (i * PI * 2.0f / 12.0f);
            float alpha = (float)(12 - i) / 12.0f;
            ImVec2 dotPos = ImVec2(
                spinnerCenter.x + cosf(angle) * spinner_size,
                spinnerCenter.y + sinf(angle) * spinner_size
            );
            dl->AddCircleFilled(dotPos, 6.0f, IM_COL32(100, 180, 255, (int)(alpha * 255)));
        }

        ImVec2 loadingSize = CalcTextSize(O("Authenticating..."));
        SetCursorPosX((cardW - loadingSize.x) * 0.5f);
        SetCursorPosY(290);
        TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), O("Authenticating..."));
    } else {
        SetCursorPosY(160);
        
        ImVec2 infoSize = CalcTextSize(O("Copy your license key and tap login"));
        SetCursorPosX((cardW - infoSize.x) * 0.5f);
        TextColored(ImVec4(0.55f, 0.55f, 0.6f, 1.0f), O("Copy your license key and tap login"));
        
        Dummy(ImVec2(0, 50));
        
        bool AutoLogin = first_time && !persistent_string["key"].empty();
        
        SetCursorPosX(40);
        PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.80f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.58f, 0.90f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.45f, 0.72f, 1.0f));
        PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
        
     if (AutoLogin || Button("LGNBTN", ImVec2(cardW - 80, 65))) {
    if (DEBUG_BYPASS_LOGIN) {
        // Debug bypass: open menu immediately
        logged_in = true;
        g_menu.isOpen = true;
    } else {
        JNIEnv* env;
        jint getEnvResult = VM->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (getEnvResult == JNI_EDETACHED) {
            if (VM->AttachCurrentThread(&env, nullptr) != 0) ERROR_MESSAGE = O("Failed to attach thread to JVM");
        } else if (getEnvResult != JNI_OK) {
            ERROR_MESSAGE = O("Failed to get JNIEnv");
        } else {
            std::thread([](std::string androidId, std::string key) {
                Login(androidId, key);
            }, getAndroidID(env), AutoLogin ? persistent_string["key"] : getClipboard(env)).detach();
        }
        first_time = false;
    }
}
        
        PopStyleVar();
        PopStyleColor(3);
        
        Dummy(ImVec2(0, 35));
        
        ImVec2 helpSize = CalcTextSize(O("Your will read"));
        SetCursorPosX((cardW - helpSize.x) * 0.5f);
        TextColored(ImVec4(0.42f, 0.42f, 0.48f, 1.0f), O("Your will read"));
    }

    End();
    PopStyleVar(3);
    PopStyleColor();
    
    End();
}


INLINE void SetupImgui() {
    PACKAGE_NAME = string(getcmdline());

    ImGui::CreateContext();

    auto& style = ImGui::GetStyle();
    auto& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;

    switch_theme(current_theme);

    load_persistence();
    svConfig_Load();
    load_imgui_style();

    static string INI_PATH = O("/data/user_de/0/") + PACKAGE_NAME + O("/no_backup/.ini");
    io.IniFilename = persistent_bool["bImguiAutoSave"] ? INI_PATH.c_str() : nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = persistent_bool["bMoveOnlyWithTitleBar"];

    ImFontConfig font_cfg;
    font_cfg.SizePixels = persistent_float["fFontScale"];
    io.Fonts->AddFontDefault(&font_cfg);

    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init(O("#version 300 es"));

    bImguiSetup = true;
}

DEFINES(EGLBoolean, Draw, EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &Width);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &Height);

    if (Width <= 0 || Height <= 0) return _Draw(dpy, surface);

    screenCenter = Vector2(Width / 2, Height / 2);

    if (!bImguiSetup) SetupImgui();

    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(Width, Height);
    ImGui::NewFrame();

    if (!is_segv_handler_active()) setup_global_segv_handler();
    if (IsExpired()) {
        DrawExpired(io);
    } else if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        DrawFloatingButton(io);
        DrawMenu(io);
// "Powered By @Zoroo_God" — Auto-centrat jos
{
    // Punem poziția la X = mijloc, Y = aproape de marginea de jos (Height - 30px)
    // Pivotul (0.5f, 1.0f) centrează orizontal și lipește marginea de jos a textului de coordonata Y
    SetNextWindowPos(ImVec2(Width * 0.5f, Height - 30.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    
    // Fereastră fără fundal, fără margini, care se redimensionează singură
    Begin(O("##PoweredBy"), nullptr, 
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | 
          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize | 
          ImGuiWindowFlags_NoInputs);
    
    TextColored(ImColor(0, 255, 0, 255), O("Owner By @A_KOJO"));
    
    End();
}

        if (g_autoPlayCalculating) DrawCalculating(io);
    } else {
        DrawLogin(io);
    }
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui_ClearHoverEffect();

    return _Draw(dpy, surface);
}

void __IMGUI__() {
    create_directory_recursive(CONC(O("/data/user_de/0/"), PACKAGE_NAME.c_str(), O("/no_backup")));
}
