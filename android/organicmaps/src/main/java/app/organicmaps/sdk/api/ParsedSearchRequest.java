package app.organicmaps.sdk.api;


import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Represents url_scheme::SearchRequest from core.
 */
// Used by JNI.
@Keep
@SuppressWarnings("unused")
public final class ParsedSearchRequest
{
    @NonNull
    public final String mQuery;
    @Nullable
    public final String mLocale;
    public final double mLat;
    public final double mLon;
    public final boolean mIsSearchOnMap;

    public ParsedSearchRequest(@NonNull String query, @Nullable String locale, double lat, double lon, boolean isSearchOnMap) {
        this.mQuery = query;
        this.mLocale = locale;
        this.mLat = lat;
        this.mLon = lon;
        this.mIsSearchOnMap = isSearchOnMap;
    }
}
