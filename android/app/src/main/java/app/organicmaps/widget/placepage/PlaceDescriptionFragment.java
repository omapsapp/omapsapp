package app.organicmaps.widget.placepage;

import android.content.res.Configuration;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import app.organicmaps.R;
import app.organicmaps.base.BaseMwmFragment;
import app.organicmaps.util.Utils;
import app.organicmaps.util.WindowInsetUtils;

import java.util.Objects;

public class PlaceDescriptionFragment extends BaseMwmFragment {
    public static final String EXTRA_DESCRIPTION = "description";
    private static final String SOURCE_SUFFIX = "<p><b>wikipedia.org</b></p>";

    @SuppressWarnings("NullableProblems")
    @NonNull
    private String mDescription;
    private WebView webView;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mDescription = Objects.requireNonNull(requireArguments()
                .getString(EXTRA_DESCRIPTION));
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                              @Nullable Bundle savedInstanceState) {
        View root = inflater.inflate(R.layout.fragment_place_description, container, false);
        webView = root.findViewById(R.id.webview);
        
        setupWebView();
        loadDescriptionWithAdaptiveStyling();
        
        ViewCompat.setOnApplyWindowInsetsListener(root, WindowInsetUtils.PaddingInsetsListener.excludeTop());
        
        return root;
    }

    private void setupWebView() {
        webView.setVerticalScrollBarEnabled(true);
        webView.setWebViewClient(new WebViewClient());
        webView.getSettings().setJavaScriptEnabled(true);
        webView.getSettings().setDefaultTextEncodingName("utf-8");
        webView.setBackgroundColor(getResources().getColor(android.R.color.transparent));
    }

    private void loadDescriptionWithAdaptiveStyling() {
        // Determine current theme (light/dark)
        int nightModeFlags = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        boolean isDarkMode = nightModeFlags == Configuration.UI_MODE_NIGHT_YES;

        // Adaptive color scheme
        String textColor = isDarkMode ? "#E0E0E0" : "#333333";
        String backgroundColor = isDarkMode ? "#32363A" : "#FFFFFF";
        String headingColor = isDarkMode ? "#E0E0E0" : "#333333";
        String linkColor = isDarkMode ? "#81D4FA" : "#1E88E5";

        // Comprehensive HTML and CSS
        String htmlContent = "<!DOCTYPE html>" +
            "<html>" +
            "<head>" +
            "<meta charset='utf-8'>" +
            "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>" +
            "<style>" +
            "* {" +
            "  box-sizing: border-box;" +
            "  margin: 0;" +
            "  padding: 0;" +
            "}" +
            "html, body {" +
            "  width: 100%;" +
            "  max-width: 100%;" +
            "  overflow-x: hidden;" +
            "}" +
            "body {" +
            "  width: 90%;" +
            "  max-width: 800px;" +
            "  margin: 0 auto;" +
            "  padding: 5% 2.5%;" +
            "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, 'Open Sans', 'Helvetica Neue', sans-serif;" +
            "  line-height: 1.6;" +
            "  font-size: calc(14px + 0.5vw);" +
            "  color: " + textColor + ";" +
            "  background-color: " + backgroundColor + ";" +
            "}" +
            "h2, h3 {" +
            "  width: 100%;" +
            "  text-align: center;" +
            "  color: " + headingColor + ";" +
            "  margin: 3% 0 1% 0;" + 
            "  padding-bottom: 0.5%;" +  
            "  font-weight: 600;" +
            "}" +
            "h2 {" +
            "  font-size: calc(1.5em + 0.5vw);" +
              "  margin-top: 4%;" +  
            "}" +
            "h3 {" +
            "  font-size: calc(1.2em + 0.4vw);" +
               "  margin-top: 2%;" +  
            "}" +
            "p {" +
            "  margin-bottom: 3%;" +
            "  text-align: justify;" +
            "  hyphens: auto;" +
            "  width: 100%;" +
            "}" +
            "ul, ol {" +
            "  margin-bottom: 3%;" +
            "  padding-left: 5%;" +
            "  width: 100%;" +
            "}" +
            "li {" +
            "  margin-bottom: 1.5%;" +
            "}" +
            "a {" +
            "  color: " + linkColor + ";" +
            "  text-decoration: none;" +
            "}" +
            "b {" +
            "  font-weight: 600;" +
            "}" +
            "sup {" +
            "  font-size: 0.6em;" +
            "  vertical-align: super;" +
            "}" +
            "@media screen and (max-width: 600px) {" +
            "  body {" +
            "    width: 95%;" +
            "    padding: 3% 2.5%;" +
            "    font-size: calc(12px + 0.5vw);" +
            "  }" +
            "  h2 {" +
            "    font-size: calc(1.3em + 0.5vw);" +
            "  }" +
            "  h3 {" +
            "    font-size: calc(1em + 0.4vw);" +
            "  }" +
            "}" +
            "</style>" +
            "</head>" +
            "<body>" +
            mDescription + 
            SOURCE_SUFFIX + 
            "</body>" +
            "</html>";

        webView.loadDataWithBaseURL(null, htmlContent, "text/html", "UTF-8", null);
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        loadDescriptionWithAdaptiveStyling();
    }
}