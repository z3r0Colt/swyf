package org.gentleking.dialtone;

import android.app.Activity;
import android.content.res.AssetFileDescriptor;
import android.os.Bundle;
import android.os.SystemClock;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.graphics.Color;
import org.json.JSONObject;
import org.json.JSONException;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private WebView web;
    private final ExecutorService engine = Executors.newSingleThreadExecutor();
    private volatile String modelStatus = "extracting";
    private volatile String modelError = "";
    private volatile int extractPercent = 0;
    private static final String FILE = "model.gguf";
    private static final int MAX_REPLY_TOKENS = 160;
    private static final long PARTIAL_INTERVAL_MS = 80;

    @Override public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setStatusBarColor(Color.rgb(16,25,35));
        getWindow().setNavigationBarColor(Color.rgb(16,25,35));
        web = new WebView(this);
        web.setBackgroundColor(Color.rgb(16,25,35));
        web.setWebChromeClient(new WebChromeClient());
        web.setWebViewClient(new WebViewClient());
        WebSettings ws = web.getSettings();
        ws.setJavaScriptEnabled(true);
        ws.setDomStorageEnabled(true);
        ws.setAllowFileAccess(true);
        ws.setAllowContentAccess(false);
        ws.setMediaPlaybackRequiresUserGesture(true);
        web.addJavascriptInterface(new GameBridge(), "DialtoneNative");
        setContentView(web);
        web.loadUrl("file:///android_asset/index.html");
        engine.execute(this::initializeModel);
    }

    private long bundledModelSize() {
        // The model is stored uncompressed (noCompress 'gguf'), so its exact length is known.
        try (AssetFileDescriptor fd = getAssets().openFd(FILE)) { return fd.getLength(); }
        catch (Exception ex) { return -1; }
    }

    private void initializeModel() {
        File target = new File(getFilesDir(), FILE);
        File temp = new File(getFilesDir(), FILE + ".part");
        try {
            long expected = bundledModelSize();
            // Re-extract when missing, partial, or replaced by an app update that ships a different model.
            if (!target.exists() || (expected > 0 ? target.length() != expected : target.length() < 500_000_000L)) {
                modelStatus = "extracting";
                sendStatus();
                if (target.exists() && !target.delete()) throw new Exception("Cannot remove old model");
                if (temp.exists() && !temp.delete()) throw new Exception("Cannot remove stale partial model");
                if (expected > 0 && getFilesDir().getUsableSpace() < expected + 50_000_000L)
                    throw new Exception("Not enough free storage. The AI needs about " + (expected / 1_000_000) + " MB free.");
                // Model is embedded in the APK and copied to private storage once, because llama.cpp loads from a file path.
                try (InputStream input = getAssets().open(FILE); FileOutputStream output = new FileOutputStream(temp)) {
                    byte[] chunk = new byte[1024 * 1024];
                    long copied = 0, lastReport = 0;
                    int n;
                    while ((n = input.read(chunk)) != -1) {
                        output.write(chunk, 0, n);
                        copied += n;
                        if (expected > 0 && SystemClock.uptimeMillis() - lastReport > 500) {
                            extractPercent = (int) (copied * 100 / expected);
                            lastReport = SystemClock.uptimeMillis();
                            sendStatus();
                        }
                    }
                    output.getFD().sync();
                }
                if (temp.length() < 500_000_000L || (expected > 0 && temp.length() != expected)) throw new Exception("Incomplete bundled model");
                if (!temp.renameTo(target)) throw new Exception("Could not move model into private storage");
            }
            extractPercent = 100;
            modelStatus = "loading";
            sendStatus();
            int threads = Math.max(1, Math.min(4, Runtime.getRuntime().availableProcessors()));
            if (!NativeLlama.load(target.getAbsolutePath(), threads, getApplicationInfo().nativeLibraryDir)) throw new Exception("llama.cpp rejected model");
            modelStatus = "ready";
        } catch (Throwable ex) {
            modelStatus = "error";
            modelError = ex.getMessage() != null ? ex.getMessage() : ex.toString();
        }
        sendStatus();
    }

    private String statusJson() {
        try { return new JSONObject().put("state", modelStatus).put("detail", modelError).put("percent", extractPercent).toString(); }
        catch (JSONException ex) { return "{\"state\":\"error\"}"; }
    }
    private void sendStatus() {
        js("window.dialtoneNativeStatus && window.dialtoneNativeStatus(" + statusJson() + ")");
    }
    private void js(String expression) {
        runOnUiThread(() -> { if (web != null) web.evaluateJavascript(expression, null); });
    }

    public final class GameBridge {
        @JavascriptInterface public String status() { return statusJson(); }

        @JavascriptInterface public void generate(String requestJson, String id) {
            engine.execute(() -> {
                JSONObject response = new JSONObject();
                try {
                    if (!"ready".equals(modelStatus)) throw new Exception("Model not ready: " + modelStatus);
                    JSONObject request = new JSONObject(requestJson);
                    String prompt = request.getString("prompt");
                    if (prompt.length() > 16000) throw new Exception("Prompt too long");
                    final long[] lastPartial = {0};
                    byte[] output = NativeLlama.generate(prompt, MAX_REPLY_TOKENS, true, utf8 -> {
                        long now = SystemClock.uptimeMillis();
                        if (now - lastPartial[0] < PARTIAL_INTERVAL_MS) return;
                        lastPartial[0] = now;
                        String partial = new String(utf8, StandardCharsets.UTF_8);
                        js("window.dialtoneNativePartial && window.dialtoneNativePartial(" + JSONObject.quote(id) + "," + JSONObject.quote(partial) + ")");
                    });
                    String reply = output == null ? "" : new String(output, StandardCharsets.UTF_8);
                    if (reply.trim().isEmpty()) throw new Exception("Model produced no reply");
                    response.put("ok", true).put("reply", reply);
                } catch (Throwable ex) {
                    try { response.put("ok", false).put("error", ex.getMessage() != null ? ex.getMessage() : ex.toString()); } catch (JSONException ignored) {}
                }
                // Encoded response as JS string literals to avoid script injection.
                js("window.dialtoneNativeResponse && window.dialtoneNativeResponse(" +
                    JSONObject.quote(id) + "," + JSONObject.quote(response.toString()) + ")");
            });
        }
    }
    @Override public void onBackPressed() {
        if (web != null && web.canGoBack()) web.goBack(); else super.onBackPressed();
    }
    @Override protected void onDestroy() {
        super.onDestroy();
        engine.shutdown();
        if (web != null) { web.removeJavascriptInterface("DialtoneNative"); web.destroy(); web = null; }
    }
}
