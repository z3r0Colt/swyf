package org.gentleking.dialtone;

import android.app.Activity;
import android.os.Bundle;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.view.View;
import android.graphics.Color;
import org.json.JSONObject;
import org.json.JSONException;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private WebView web;
    private final ExecutorService engine = Executors.newSingleThreadExecutor();
    private volatile String modelStatus = "extracting";
    private volatile String modelError = "";
    private static final String FILE = "model.gguf";

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

    private void initializeModel() {
        File target = new File(getFilesDir(), FILE);
        File temp = new File(getFilesDir(), FILE + ".part");
        try {
            if (!target.exists() || target.length() < 500_000_000L) {
                modelStatus = "extracting";
                if (temp.exists() && !temp.delete()) throw new Exception("Cannot remove stale partial model");
                // Model is embedded in the APK and copied to private storage once.
                try (InputStream input = getAssets().open(FILE); FileOutputStream output = new FileOutputStream(temp)) {
                    byte[] chunk = new byte[1024 * 1024];
                    int n;
                    while ((n = input.read(chunk)) != -1) output.write(chunk, 0, n);
                    output.getFD().sync();
                }
                if (temp.length() < 500_000_000L) throw new Exception("Incomplete bundled model");
                if (!temp.renameTo(target)) throw new Exception("Could not move model into private storage");
            }
            modelStatus = "loading";
            if (!NativeLlama.load(target.getAbsolutePath())) throw new Exception("llama.cpp rejected model");
            modelStatus = "ready";
        } catch (Throwable ex) {
            modelStatus = "error";
            modelError = ex.toString();
        }
        sendStatus();
    }

    private void sendStatus() {
        try {
            JSONObject response = new JSONObject().put("state", modelStatus).put("detail", modelError);
            js("window.dialtoneNativeStatus && window.dialtoneNativeStatus(" + response.toString() + ")");
        } catch (JSONException ignored) {}
    }
    private void js(String expression) {
        runOnUiThread(() -> { if (web != null) web.evaluateJavascript(expression, null); });
    }
    public final class GameBridge {
        @JavascriptInterface public String status() {
            try { return new JSONObject().put("state", modelStatus).put("detail", modelError).toString(); }
            catch (JSONException ex) { return "{\"state\":\"error\"}"; }
        }
        @JavascriptInterface public void generate(String requestJson, String id) {
            engine.execute(() -> {
                JSONObject response = new JSONObject();
                try {
                    if (!"ready".equals(modelStatus)) throw new Exception("Model not ready: " + modelStatus);
                    JSONObject request = new JSONObject(requestJson);
                    String prompt = request.getString("prompt");
                    if (prompt.length() > 8000) throw new Exception("Prompt too long");
                    String output = NativeLlama.generate(prompt, 180);
                    if (output == null || output.isEmpty()) throw new Exception("Model produced no reply");
                    response.put("ok", true).put("reply", output);
                } catch (Throwable ex) {
                    try { response.put("ok", false).put("error", ex.toString()); } catch (JSONException ignored) {}
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
