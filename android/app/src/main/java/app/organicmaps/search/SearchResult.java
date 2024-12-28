package app.organicmaps.search;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.StyleSpan;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import app.organicmaps.bookmarks.data.FeatureId;
import app.organicmaps.util.Distance;

/**
 * Class instances are created from native code.
 */
// Used by JNI.
@Keep
@SuppressWarnings("unused")
public class SearchResult
{
  public static final int TYPE_PURE_SUGGEST = 0;
  public static final int TYPE_SUGGEST = 1;
  public static final int TYPE_RESULT = 2;

  // Values should match osm::YesNoUnknown enum.
  public static final int OPEN_NOW_UNKNOWN = 0;
  public static final int OPEN_NOW_YES = 1;
  public static final int OPEN_NOW_NO = 2;

  public static final SearchResult EMPTY = new SearchResult("", "", 0, 0,
                                                            new int[] {}, new int[] {});

  // Used by JNI.
  @Keep
  @SuppressWarnings("unused")
  public static class Description
  {
    public final FeatureId featureId;
    public final String localizedFeatureType;
    public final String region;
    public final Distance distance;

    public final String description;

    public final int openNow;
    public final int minutesUntilOpen;
    public final int minutesUntilClosed;
    public final boolean hasPopularityHigherPriority;

    public Description(FeatureId featureId, String featureType, String region, Distance distance,
                       String description, int openNow, int minutesUntilOpen, int minutesUntilClosed,
                       boolean hasPopularityHigherPriority)
    {
      this.featureId = featureId;
      this.localizedFeatureType = featureType;
      this.region = region;
      this.distance = distance;
      this.description = description;
      this.openNow = openNow;
      this.minutesUntilOpen = minutesUntilOpen;
      this.minutesUntilClosed = minutesUntilClosed;
      this.hasPopularityHigherPriority = hasPopularityHigherPriority;
    }
  }

  public final String name;
  public final String suggestion;
  public final double lat;
  public final double lon;

  public final int type;
  public final Description description;

  // Consecutive pairs of indexes (each pair contains : start index, length), specifying highlighted matches of original query in result
  public final int[] highlightRanges;
  public final int[] descHighlightRanges;

  @NonNull
  private final Popularity mPopularity;

  public SearchResult(String name, String suggestion, double lat, double lon, int[] highlightRanges, int[] descHighlightRanges)
  {
    this.name = name;
    this.suggestion = suggestion;
    this.lat = lat;
    this.lon = lon;
    this.description = null;
    // Looks like a hack, but it's fine. Otherwise, should make one more ctor and JNI code bloat.
    if (lat == 0 && lon == 0)
      this.type = TYPE_PURE_SUGGEST;
    else
      this.type = TYPE_SUGGEST;
    this.highlightRanges = highlightRanges;
    this.descHighlightRanges = descHighlightRanges;
    mPopularity = Popularity.defaultInstance();
  }

  public SearchResult(String name, Description description, double lat, double lon, int[] highlightRanges,
                      int[] descHighlightRanges, @NonNull Popularity popularity)
  {
    this.type = TYPE_RESULT;
    this.name = name;
    mPopularity = popularity;
    this.suggestion = null;
    this.lat = lat;
    this.lon = lon;
    this.description = description;
    this.highlightRanges = highlightRanges;
    this.descHighlightRanges = descHighlightRanges;
  }

  @NonNull
  public String getTitle(@NonNull Context context)
  {
    String title = name;
    if (TextUtils.isEmpty(title) && description != null)
      title = description.localizedFeatureType;
    return title;
  }

  public void formatText(SpannableStringBuilder builder, int[] ranges)
  {
    if (ranges != null)
    {
      final int size = ranges.length / 2;
      int index = 0;
      for (int i = 0; i < size; i++)
      {
        final int start = ranges[index++];
        final int len = ranges[index++];

        builder.setSpan(new StyleSpan(Typeface.BOLD), start, start + len, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
      }
    }
  }

  @NonNull
  public Spannable getFormattedTitle(@NonNull Context context)
  {
    final String title = getTitle(context);
    final SpannableStringBuilder builder = new SpannableStringBuilder(title);
    formatText(builder, highlightRanges);

    return builder;
  }

  public Spannable getFormattedAddress(@NonNull Context context)
  {
    final String address = description != null ? description.region : null;
    final SpannableStringBuilder builder = new SpannableStringBuilder(address);
    formatText(builder, descHighlightRanges);

    return builder;
  }

}
