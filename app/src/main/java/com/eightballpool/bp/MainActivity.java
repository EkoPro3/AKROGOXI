package com.eightballpool.bp;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.LinkedHashMap;
import java.util.Map;

public class MainActivity extends AppCompatActivity {

    private static final String PREFS_NAME  = "VenomPrefs";
    private static final String PREF_KEY    = "saved_key";

    private static View    floatingMenuView = null;
    private static boolean menuExpanded     = false;

    // ─── Colors ───────────────────────────────────────────────
    private static final int COLOR_ACCENT   = 0xFFFF3B3B;
    private static final int COLOR_ACCENT2  = 0xFFFF6B35;
    private static final int COLOR_WHITE    = 0xFFFFFFFF;
    private static final int COLOR_GRAY     = 0xFF888888;
    private static final int COLOR_CARD     = 0xCC1A1A1A;
    private static final int COLOR_INPUT_BG = 0xCC252525;
    private static final int CONTACT_COLOR  = 0xFF2CA5E0;

    // ─── Feature toggles (sync with C++ via SharedPreferences) ─
    private static final String FEAT_AIM         = "bAIM";
    private static final String FEAT_AIM_BOTS    = "bAIM_IgnoreBots";
    private static final String FEAT_AIM_VIS     = "bAIM_CheckVisibility";
    private static final String FEAT_AIM_LINE    = "bAIM_SnapLine";
    private static final String FEAT_AIM_FOV     = "bAIM_DrawFov";
    private static final String FEAT_ESP         = "bESP";
    private static final String FEAT_ESP_BOX     = "bESP_Box";
    private static final String FEAT_ESP_SKEL    = "bESP_Skeleton";
    private static final String FEAT_ESP_HEALTH  = "bESP_Health";
    private static final String FEAT_ESP_NAME    = "bESP_Name";
    private static final String FEAT_ESP_DIST    = "bESP_Distance";

    // ═══════════════════════════════════════════════════════════
    //  API CONFIG  (mirrors TDR-BBOX Sapi pattern)
    // ═══════════════════════════════════════════════════════════
    private static class ApiConfig {
        static String getBaseUrl()       { return "https://venomkey.com/connect"; }
        static String getGame()          { return "PUBG"; }
        static String getUserKeyField()  { return "user_key"; }
        static Map<String, String> getHeaders() {
            Map<String, String> h = new LinkedHashMap<>();
            h.put("Content-Type", "application/x-www-form-urlencoded");
            h.put("Accept",       "application/json");
            h.put("Charset",      "UTF-8");
            h.put("User-Agent",   "public-loder");
            return h;
        }
    }

    // ═══════════════════════════════════════════════════════════
    //  KURO API CLIENT  (mirrors TDR-BBOX KuroApi pattern)
    // ═══════════════════════════════════════════════════════════
    private static class KuroApiClient {
        private final String userKey;
        private final String androidId;

        KuroApiClient(String userKey, String androidId) {
            this.userKey   = userKey;
            this.androidId = androidId;
        }

        private String buildUUID() {
            return userKey + androidId + android.os.Build.MODEL + android.os.Build.BRAND;
        }

        String calculateMD5(String input) {
            try {
                MessageDigest md = MessageDigest.getInstance("MD5");
                byte[] digest = md.digest(input.getBytes("UTF-8"));
                StringBuilder sb = new StringBuilder();
                for (byte b : digest) sb.append(String.format("%02x", b));
                return sb.toString();
            } catch (Exception e) { return input; }
        }

        private String postFormRequest(Map<String,String> formData, Map<String,String> headers) throws Exception {
            URL url = new URL(ApiConfig.getBaseUrl());
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("POST");
            conn.setDoOutput(true);
            conn.setConnectTimeout(15000);
            conn.setReadTimeout(15000);
            for (Map.Entry<String,String> e : headers.entrySet())
                conn.setRequestProperty(e.getKey(), e.getValue());
            StringBuilder body = new StringBuilder();
            for (Map.Entry<String,String> e : formData.entrySet()) {
                if (body.length() > 0) body.append("&");
                body.append(java.net.URLEncoder.encode(e.getKey(),   "UTF-8"))
                    .append("=")
                    .append(java.net.URLEncoder.encode(e.getValue(), "UTF-8"));
            }
            try (OutputStream os = conn.getOutputStream()) { os.write(body.toString().getBytes("UTF-8")); }
            int code = conn.getResponseCode();
            BufferedReader r = new BufferedReader(new InputStreamReader(
                    code == 200 ? conn.getInputStream() : conn.getErrorStream()));
            StringBuilder sb = new StringBuilder(); String line;
            while ((line = r.readLine()) != null) sb.append(line);
            r.close();
            return sb.toString();
        }

        JSONObject getDatabase() throws Exception {
            Map<String,String> headers  = ApiConfig.getHeaders();
            String serial = calculateMD5(buildUUID());
            Map<String,String> formData = new LinkedHashMap<>();
            formData.put("game",                     ApiConfig.getGame());
            formData.put(ApiConfig.getUserKeyField(), userKey);
            formData.put("serial",                   serial);
            return new JSONObject(postFormRequest(formData, headers));
        }
    }

    // ═══════════════════════════════════════════════════════════
    //  ACTIVITY
    // ═══════════════════════════════════════════════════════════
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        buildLoginUI();
    }

    // ═══════════════════════════════════════════════════════════
    //  BUILD LOGIN UI
    // ═══════════════════════════════════════════════════════════
    @SuppressLint("SetTextI18n")
    private void buildLoginUI() {
        SharedPreferences prefs    = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        String            savedKey = prefs.getString(PREF_KEY, "");

        // ── Content (background comes from theme/window, transparent here) ──
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.TRANSPARENT);
        scroll.setFillViewport(true);

        // Dark overlay so text is readable over background image
        FrameLayout overlay = new FrameLayout(this);
        overlay.setBackgroundColor(0xAA000000);

        LinearLayout container = new LinearLayout(this);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setGravity(Gravity.CENTER);
        container.setBackgroundColor(Color.TRANSPARENT);
        container.setPadding(dp(24), dp(48), dp(24), dp(40));

        // ── Modern 8-ball Icon ────────────────────────────────
        View modernIcon = buildModernIcon();
        LinearLayout.LayoutParams iconLP = new LinearLayout.LayoutParams(dp(120), dp(120));
        iconLP.gravity = Gravity.CENTER_HORIZONTAL;
        modernIcon.setLayoutParams(iconLP);
        container.addView(modernIcon);

        // ── App Title ─────────────────────────────────────────
        TextView appTitle = new TextView(this);
        appTitle.setText("AKRO PRO");
        appTitle.setTextSize(30);
        appTitle.setTextColor(COLOR_WHITE);
        appTitle.setTypeface(null, Typeface.BOLD);
        appTitle.setGravity(Gravity.CENTER);
        appTitle.setLetterSpacing(0.12f);
        appTitle.setShadowLayer(16f, 0, 0, COLOR_ACCENT);
        LinearLayout.LayoutParams titleLP = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        titleLP.topMargin = dp(16);
        appTitle.setLayoutParams(titleLP);
        container.addView(appTitle);

        // ── Subtitle ──────────────────────────────────────────
        TextView subtitle = new TextView(this);
        subtitle.setText("8 Ball Pool Enhancement");
        subtitle.setTextSize(13);
        subtitle.setTextColor(COLOR_GRAY);
        subtitle.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams subLP = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        subLP.topMargin = dp(4);
        subLP.bottomMargin = dp(28);
        subtitle.setLayoutParams(subLP);
        container.addView(subtitle);

        // ── Device ID Row ─────────────────────────────────────
        String androidId = getAndroidId();
        LinearLayout idRow = new LinearLayout(this);
        idRow.setOrientation(LinearLayout.HORIZONTAL);
        idRow.setGravity(Gravity.CENTER_VERTICAL);
        idRow.setPadding(dp(16), dp(12), dp(16), dp(12));
        GradientDrawable idBg = new GradientDrawable();
        idBg.setShape(GradientDrawable.RECTANGLE);
        idBg.setCornerRadius(dp(10));
        idBg.setColor(COLOR_CARD);
        idBg.setStroke(dp(1), 0xFF2A2A2A);
        idRow.setBackground(idBg);

        TextView idLabel = new TextView(this);
        idLabel.setText("DEVICE ID : ");
        idLabel.setTextSize(12);
        idLabel.setTextColor(COLOR_GRAY);
        idRow.addView(idLabel);

        TextView idValue = new TextView(this);
        idValue.setText(androidId);
        idValue.setTextSize(12);
        idValue.setTextColor(0xFFCCCCCC);
        idValue.setTypeface(null, Typeface.BOLD);
        idValue.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        idRow.addView(idValue);

        Button copyBtn = new Button(this);
        copyBtn.setText("COPY");
        copyBtn.setTextSize(10);
        copyBtn.setTextColor(COLOR_WHITE);
        copyBtn.setTypeface(null, Typeface.BOLD);
        copyBtn.setAllCaps(true);
        GradientDrawable copyBg = new GradientDrawable();
        copyBg.setShape(GradientDrawable.RECTANGLE);
        copyBg.setCornerRadius(dp(6));
        copyBg.setColor(0xFF333333);
        copyBtn.setBackground(copyBg);
        copyBtn.setPadding(dp(10), dp(4), dp(10), dp(4));
        copyBtn.setOnClickListener(v -> {
            android.content.ClipboardManager cm =
                    (android.content.ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm != null) {
                cm.setPrimaryClip(android.content.ClipData.newPlainText("id", androidId));
                Toast.makeText(this, "Copied!", Toast.LENGTH_SHORT).show();
            }
        });
        idRow.addView(copyBtn);

        LinearLayout.LayoutParams idRowLP = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        idRowLP.bottomMargin = dp(16);
        idRow.setLayoutParams(idRowLP);
        container.addView(idRow);

        // ── Key Input ─────────────────────────────────────────
        EditText keyInput = new EditText(this);
        keyInput.setHint("Enter your key");
        keyInput.setHintTextColor(0xFF555555);
        keyInput.setTextColor(COLOR_WHITE);
        keyInput.setTextSize(15);
        keyInput.setSingleLine(true);
        keyInput.setGravity(Gravity.CENTER);
        if (!savedKey.isEmpty()) keyInput.setText(savedKey);
        GradientDrawable inputBg = new GradientDrawable();
        inputBg.setShape(GradientDrawable.RECTANGLE);
        inputBg.setCornerRadius(dp(12));
        inputBg.setColor(COLOR_INPUT_BG);
        inputBg.setStroke(dp(1), COLOR_ACCENT);
        keyInput.setBackground(inputBg);
        keyInput.setPadding(dp(20), dp(16), dp(20), dp(16));
        LinearLayout.LayoutParams inputLP = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        inputLP.bottomMargin = dp(12);
        keyInput.setLayoutParams(inputLP);
        container.addView(keyInput);

        // ── Error ─────────────────────────────────────────────
        TextView errorText = new TextView(this);
        errorText.setTextColor(COLOR_ACCENT);
        errorText.setTextSize(12);
        errorText.setGravity(Gravity.CENTER);
        errorText.setVisibility(View.GONE);
        LinearLayout.LayoutParams errLP = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        errLP.bottomMargin = dp(8);
        errorText.setLayoutParams(errLP);
        container.addView(errorText);

        // ── Progress ──────────────────────────────────────────
        ProgressBar progress = new ProgressBar(this);
        progress.setVisibility(View.GONE);
        LinearLayout.LayoutParams pbLP = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        pbLP.gravity = Gravity.CENTER_HORIZONTAL;
        pbLP.bottomMargin = dp(8);
        progress.setLayoutParams(pbLP);
        container.addView(progress);

        // ── Buttons Row ───────────────────────────────────────
        LinearLayout btnRow = new LinearLayout(this);
        btnRow.setOrientation(LinearLayout.HORIZONTAL);
        btnRow.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams btnRowLP = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        btnRowLP.topMargin = dp(8);
        btnRow.setLayoutParams(btnRowLP);

        Button buyBtn = buildButton("Buy Key", false);
        LinearLayout.LayoutParams buyLP = new LinearLayout.LayoutParams(0, dp(52), 1f);
        buyLP.rightMargin = dp(8);
        buyBtn.setLayoutParams(buyLP);
        buyBtn.setOnClickListener(v -> {
            try { startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://venomkey.com"))); }
            catch (Exception e) { Toast.makeText(this, "Could not open browser", Toast.LENGTH_SHORT).show(); }
        });
        btnRow.addView(buyBtn);

        Button loginBtn = buildButton("Login", true);
        LinearLayout.LayoutParams loginLP = new LinearLayout.LayoutParams(0, dp(52), 1f);
        loginLP.leftMargin = dp(8);
        loginBtn.setLayoutParams(loginLP);
        loginBtn.setOnClickListener(v -> {
            String key = keyInput.getText().toString().trim();
            if (key.isEmpty()) { errorText.setText("Please enter your key"); errorText.setVisibility(View.VISIBLE); return; }
            errorText.setVisibility(View.GONE);
            loginBtn.setEnabled(false);
            progress.setVisibility(View.VISIBLE);
            doLogin(key, androidId, loginBtn, progress, errorText, prefs);
        });
        btnRow.addView(loginBtn);
        container.addView(btnRow);

        // ── Divider ───────────────────────────────────────────
        View divider = new View(this);
        divider.setBackgroundColor(0xFF222222);
        LinearLayout.LayoutParams divLP = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(1));
        divLP.topMargin = dp(24);
        divLP.bottomMargin = dp(16);
        divider.setLayoutParams(divLP);
        container.addView(divider);

        // ── Contact Button ────────────────────────────────────
        Button contactBtn = new Button(this);
        contactBtn.setText("📞 تواصل مع AKRO");
        contactBtn.setTextColor(CONTACT_COLOR);
        contactBtn.setTextSize(14);
        contactBtn.setTypeface(null, Typeface.BOLD);
        contactBtn.setBackground(null);
        contactBtn.setOnClickListener(v -> openTelegramContact());
        container.addView(contactBtn);

        // ── Version ───────────────────────────────────────────
        TextView version = new TextView(this);
        version.setText("v1.0 | 8 Ball Pool");
        version.setTextSize(11);
        version.setTextColor(0xFF444444);
        version.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams verLP = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        verLP.topMargin = dp(6);
        version.setLayoutParams(verLP);
        container.addView(version);

        overlay.addView(container, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        scroll.addView(overlay);
        setContentView(scroll);
    }

    // ═══════════════════════════════════════════════════════════
    //  MODERN 8-BALL ICON  (canvas-drawn)
    // ═══════════════════════════════════════════════════════════
    private View buildModernIcon() {
        return new View(this) {
            private final Paint glowPaint   = new Paint(Paint.ANTI_ALIAS_FLAG);
            private final Paint circlePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            private final Paint shinePaint  = new Paint(Paint.ANTI_ALIAS_FLAG);
            private final Paint textPaint   = new Paint(Paint.ANTI_ALIAS_FLAG);
            private final Paint ringPaint   = new Paint(Paint.ANTI_ALIAS_FLAG);

            @Override
            protected void onDraw(Canvas canvas) {
                float w = getWidth(), h = getHeight();
                float cx = w / 2f, cy = h / 2f;
                float r  = Math.min(cx, cy) - dp(6);

                glowPaint.setStyle(Paint.Style.STROKE);
                glowPaint.setStrokeWidth(dp(8));
                glowPaint.setColor(0x33FF3B3B);
                canvas.drawCircle(cx, cy, r + dp(10), glowPaint);
                glowPaint.setStrokeWidth(dp(4));
                glowPaint.setColor(0x66FF3B3B);
                canvas.drawCircle(cx, cy, r + dp(5), glowPaint);

                circlePaint.setStyle(Paint.Style.FILL);
                circlePaint.setShader(new RadialGradient(cx, cy - r * 0.2f, r,
                        new int[]{ 0xFFFF6B35, 0xFFFF3B3B, 0xFF8B0000 },
                        new float[]{ 0f, 0.5f, 1f }, Shader.TileMode.CLAMP));
                canvas.drawCircle(cx, cy, r, circlePaint);

                ringPaint.setStyle(Paint.Style.STROKE);
                ringPaint.setStrokeWidth(dp(3));
                ringPaint.setShader(new LinearGradient(cx - r, cy, cx + r, cy,
                        new int[]{ 0xFFFF6B35, 0xFFFFD700, 0xFFFF3B3B },
                        null, Shader.TileMode.CLAMP));
                canvas.drawCircle(cx, cy, r, ringPaint);

                shinePaint.setStyle(Paint.Style.FILL);
                shinePaint.setShader(new RadialGradient(cx - r * 0.25f, cy - r * 0.35f, r * 0.5f,
                        new int[]{ 0x55FFFFFF, 0x00FFFFFF }, null, Shader.TileMode.CLAMP));
                canvas.drawCircle(cx, cy, r, shinePaint);

                float textSize = r * 1.05f;
                textPaint.setTextSize(textSize);
                textPaint.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));
                textPaint.setColor(COLOR_WHITE);
                textPaint.setTextAlign(Paint.Align.CENTER);
                textPaint.setShadowLayer(dp(6), 0, dp(2), 0xAA000000);
                canvas.drawText("8", cx, cy + textSize * 0.36f, textPaint);
            }
        };
    }

    // ═══════════════════════════════════════════════════════════
    //  DO LOGIN
    // ═══════════════════════════════════════════════════════════
    private void doLogin(String key, String androidId,
                         Button loginBtn, ProgressBar progress,
                         TextView errorText, SharedPreferences prefs) {
        new Thread(() -> {
            try {
                KuroApiClient api = new KuroApiClient(key, androidId);
                JSONObject resp   = api.getDatabase();
                new Handler(Looper.getMainLooper()).post(() -> {
                    progress.setVisibility(View.GONE);
                    loginBtn.setEnabled(true);
                    try {
                        String  status = resp.optString("status", "");
                        boolean ok     = "success".equals(status) || resp.optBoolean("status", false);
                        if (ok) {
                            prefs.edit().putString(PREF_KEY, key).apply();
                            String expiry = "N/A";
                            JSONObject data = resp.optJSONObject("data");
                            if (data != null)
                                expiry = data.optString("expiry_date",
                                         data.optString("expired_date",
                                         data.optString("exdate", "N/A")));

                            // ── Launch 8 Ball Pool then show menu ────────
                            floatingMenu(this, prefs);
                            launchGame(this, expiry);
                        } else {
                            String msg = resp.optString("message",
                                         resp.optString("reason", "Invalid key or unauthorized"));
                            errorText.setText(msg);
                            errorText.setVisibility(View.VISIBLE);
                        }
                    } catch (Exception e) {
                        errorText.setText("Parse error: " + e.getMessage());
                        errorText.setVisibility(View.VISIBLE);
                    }
                });
            } catch (Exception e) {
                new Handler(Looper.getMainLooper()).post(() -> {
                    progress.setVisibility(View.GONE);
                    loginBtn.setEnabled(true);
                    errorText.setText("Connection error: " + e.getMessage());
                    errorText.setVisibility(View.VISIBLE);
                });
            }
        }).start();
    }

    // ═══════════════════════════════════════════════════════════
    //  FLOATING MOD MENU  (appears in-game after login)
    // ═══════════════════════════════════════════════════════════
    @SuppressLint("UseSwitchCompatOrMaterialCode")
    public static void floatingMenu(Activity activity, SharedPreferences prefs) {
        if (floatingMenuView != null) return;

        WindowManager wm = (WindowManager) activity.getSystemService(Activity.WINDOW_SERVICE);

        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.TYPE_APPLICATION,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.TOP | Gravity.START;
        params.x = 20;
        params.y = 180;

        // ── Root container ────────────────────────────────────
        LinearLayout root = new LinearLayout(activity);
        root.setOrientation(LinearLayout.VERTICAL);

        // ── Header bar (always visible / drag handle) ─────────
        LinearLayout header = new LinearLayout(activity);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        header.setPadding(28, 14, 18, 14);
        GradientDrawable headerBg = new GradientDrawable();
        headerBg.setShape(GradientDrawable.RECTANGLE);
        headerBg.setCornerRadii(new float[]{ 24,24, 24,24, 0,0, 0,0 });
        int[] hColors = { 0xFF8B0000, 0xFFFF3B3B };
        headerBg.setColors(hColors);
        headerBg.setOrientation(GradientDrawable.Orientation.LEFT_RIGHT);
        header.setBackground(headerBg);

        TextView logo = new TextView(activity);
        logo.setText("⚙ AKRO PRO");
        logo.setTextColor(0xFFFFFFFF);
        logo.setTextSize(14f);
        logo.setTypeface(null, Typeface.BOLD);
        logo.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        header.addView(logo);

        TextView toggleArrow = new TextView(activity);
        toggleArrow.setText("▼");
        toggleArrow.setTextColor(0xFFFFFFFF);
        toggleArrow.setTextSize(14f);
        toggleArrow.setPadding(8, 0, 8, 0);
        header.addView(toggleArrow);
        root.addView(header);

        // ── Menu body (collapsible) ───────────────────────────
        ScrollView menuScroll = new ScrollView(activity);
        menuScroll.setVisibility(View.GONE); // starts collapsed
        menuScroll.setBackgroundColor(0xEE0D0D0D);

        LinearLayout menuBody = new LinearLayout(activity);
        menuBody.setOrientation(LinearLayout.VERTICAL);
        menuBody.setPadding(0, 4, 0, 8);

        // Contact row at top
        addMenuContact(activity, menuBody);

        // ── AIM section ───────────────────────────────────────
        addSectionLabel(activity, menuBody, "🎯  AIM BOT");
        addToggle(activity, menuBody, prefs, "Auto Aim",         FEAT_AIM,      false);
        addToggle(activity, menuBody, prefs, "Ignore Bots",      FEAT_AIM_BOTS, false);
        addToggle(activity, menuBody, prefs, "Check Visibility", FEAT_AIM_VIS,  true);
        addToggle(activity, menuBody, prefs, "Snap Line",        FEAT_AIM_LINE, false);
        addToggle(activity, menuBody, prefs, "Draw FOV",         FEAT_AIM_FOV,  true);

        // ── ESP section ───────────────────────────────────────
        addSectionLabel(activity, menuBody, "👁  ESP");
        addToggle(activity, menuBody, prefs, "Enable ESP",     FEAT_ESP,        false);
        addToggle(activity, menuBody, prefs, "Box ESP",        FEAT_ESP_BOX,    false);
        addToggle(activity, menuBody, prefs, "Skeleton",       FEAT_ESP_SKEL,   false);
        addToggle(activity, menuBody, prefs, "Health Bar",     FEAT_ESP_HEALTH, false);
        addToggle(activity, menuBody, prefs, "Name ESP",       FEAT_ESP_NAME,   false);
        addToggle(activity, menuBody, prefs, "Distance",       FEAT_ESP_DIST,   false);

        menuScroll.addView(menuBody, new LinearLayout.LayoutParams(
                340, ViewGroup.LayoutParams.WRAP_CONTENT));
        root.addView(menuScroll);

        // ── Outer rounded background ──────────────────────────
        GradientDrawable rootBg = new GradientDrawable();
        rootBg.setShape(GradientDrawable.RECTANGLE);
        rootBg.setCornerRadius(24f);
        rootBg.setColor(0xEE0D0D0D);
        rootBg.setStroke(2, 0xFFFF3B3B);
        root.setBackground(rootBg);
        root.setElevation(16f);

        // ── Toggle expand/collapse on header click ────────────
        header.setOnClickListener(v -> {
            menuExpanded = !menuExpanded;
            menuScroll.setVisibility(menuExpanded ? View.VISIBLE : View.GONE);
            toggleArrow.setText(menuExpanded ? "▲" : "▼");
            params.flags = menuExpanded
                    ? WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                    : WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
            wm.updateViewLayout(root, params);
        });

        // ── Drag support ──────────────────────────────────────
        header.setOnTouchListener(new View.OnTouchListener() {
            private int   ix, iy;
            private float tx, ty;
            private long  t0;
            private boolean dragging;

            @Override
            public boolean onTouch(View v, MotionEvent e) {
                switch (e.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        ix = params.x; iy = params.y;
                        tx = e.getRawX(); ty = e.getRawY();
                        t0 = System.currentTimeMillis();
                        dragging = false;
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        float dx = e.getRawX() - tx, dy = e.getRawY() - ty;
                        if (!dragging && (Math.abs(dx) > 8 || Math.abs(dy) > 8)) dragging = true;
                        if (dragging) {
                            params.x = ix + (int) dx;
                            params.y = iy + (int) dy;
                            wm.updateViewLayout(root, params);
                        }
                        return true;
                    case MotionEvent.ACTION_UP:
                        if (!dragging && System.currentTimeMillis() - t0 < 200) v.performClick();
                        return true;
                }
                return false;
            }
        });

        wm.addView(root, params);
        floatingMenuView = root;
    }

    // ── Section label helper ─────────────────────────────────
    private static void addSectionLabel(Activity a, LinearLayout parent, String text) {
        TextView tv = new TextView(a);
        tv.setText(text);
        tv.setTextColor(0xFFFF3B3B);
        tv.setTextSize(11.5f);
        tv.setTypeface(null, Typeface.BOLD);
        tv.setLetterSpacing(0.08f);
        tv.setPadding(20, 12, 20, 4);
        parent.addView(tv);

        View line = new View(a);
        line.setBackgroundColor(0xFF2A2A2A);
        parent.addView(line, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 1));
    }

    // ── Toggle row helper ─────────────────────────────────────
    @SuppressLint("UseSwitchCompatOrMaterialCode")
    private static void addToggle(Activity a, LinearLayout parent,
                                   SharedPreferences prefs,
                                   String label, String key, boolean defaultVal) {
        LinearLayout row = new LinearLayout(a);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(20, 8, 16, 8);

        TextView tv = new TextView(a);
        tv.setText(label);
        tv.setTextColor(0xFFCCCCCC);
        tv.setTextSize(13f);
        tv.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        row.addView(tv);

        Switch sw = new Switch(a);
        sw.setChecked(prefs.getBoolean(key, defaultVal));
        sw.setOnCheckedChangeListener((b, isOn) -> {
            prefs.edit().putBoolean(key, isOn).apply();
            tv.setTextColor(isOn ? 0xFFFFFFFF : 0xFFCCCCCC);
        });
        if (sw.isChecked()) tv.setTextColor(0xFFFFFFFF);
        row.addView(sw);

        parent.addView(row);
    }

    // ── Contact row ───────────────────────────────────────────
    private static void addMenuContact(Activity a, LinearLayout parent) {
        LinearLayout row = new LinearLayout(a);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER);
        row.setPadding(20, 12, 20, 12);

        TextView tv = new TextView(a);
        tv.setText("📞 تواصل: 01508294415");
        tv.setTextColor(0xFF2CA5E0);
        tv.setTextSize(12f);
        tv.setTypeface(null, Typeface.BOLD);
        tv.setGravity(Gravity.CENTER);
        tv.setOnClickListener(v -> {
            try {
                Intent i = new Intent(Intent.ACTION_VIEW, Uri.parse("https://t.me/+201508294415"));
                i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                a.startActivity(i);
            } catch (Exception ignored) {}
        });
        row.addView(tv);

        View line = new View(a);
        line.setBackgroundColor(0xFF2A2A2A);
        parent.addView(row);
        parent.addView(line, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 1));
    }

    public static void removeFloatingMenu(Activity activity) {
        if (floatingMenuView != null) {
            WindowManager wm = (WindowManager) activity.getSystemService(Activity.WINDOW_SERVICE);
            try { wm.removeView(floatingMenuView); } catch (Exception ignored) {}
            floatingMenuView = null;
            menuExpanded = false;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        removeFloatingMenu(this);
    }

    // ═══════════════════════════════════════════════════════════
    //  LAUNCH GAME  (com.miniclip.eightballpool)
    // ═══════════════════════════════════════════════════════════
    private static final String GAME_PKG = "com.miniclip.eightballpool";

    private void launchGame(Activity activity, String expiry) {
        // Small delay so floating menu window is registered before we leave
        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            try {
                Intent gameIntent = activity.getPackageManager()
                        .getLaunchIntentForPackage(GAME_PKG);
                if (gameIntent != null) {
                    gameIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                            | Intent.FLAG_ACTIVITY_CLEAR_TOP);
                    activity.startActivity(gameIntent);
                    activity.finish();
                } else {
                    // Game not installed – open Play Store
                    Toast.makeText(activity,
                            "8 Ball Pool غير مثبتة، جاري فتح المتجر...",
                            Toast.LENGTH_SHORT).show();
                    Intent store = new Intent(Intent.ACTION_VIEW,
                            Uri.parse("market://details?id=" + GAME_PKG));
                    store.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    activity.startActivity(store);
                    activity.finish();
                }
            } catch (Exception e) {
                Toast.makeText(activity,
                        "خطأ في فتح اللعبة: " + e.getMessage(),
                        Toast.LENGTH_LONG).show();
                activity.finish();
            }
        }, 400); // 400ms – enough for WindowManager to register the floating menu
    }

    // ═══════════════════════════════════════════════════════════
    //  HELPERS
    // ═══════════════════════════════════════════════════════════
    private void openTelegramContact() {
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://t.me/+201508294415")));
        } catch (Exception e) {
            Toast.makeText(this, "Telegram: 01508294415", Toast.LENGTH_LONG).show();
        }
    }

    private Button buildButton(String text, boolean isPrimary) {
        Button btn = new Button(this);
        btn.setText(text);
        btn.setTextColor(COLOR_WHITE);
        btn.setTextSize(15);
        btn.setTypeface(null, Typeface.BOLD);
        btn.setAllCaps(false);
        GradientDrawable bg = new GradientDrawable();
        bg.setShape(GradientDrawable.RECTANGLE);
        bg.setCornerRadius(dp(12));
        if (isPrimary) {
            int[] c = { COLOR_ACCENT, COLOR_ACCENT2 };
            bg.setColors(c);
            bg.setOrientation(GradientDrawable.Orientation.LEFT_RIGHT);
        } else {
            bg.setColor(0xFF222222);
            bg.setStroke(dp(1), 0xFF444444);
        }
        btn.setBackground(bg);
        return btn;
    }

    @SuppressLint("HardwareIds")
    private String getAndroidId() {
        try {
            return android.provider.Settings.Secure.getString(
                    getContentResolver(), android.provider.Settings.Secure.ANDROID_ID);
        } catch (Exception e) { return "unknown"; }
    }

    private int dp(int dp) {
        return (int)(dp * getResources().getDisplayMetrics().density + 0.5f);
    }
}
