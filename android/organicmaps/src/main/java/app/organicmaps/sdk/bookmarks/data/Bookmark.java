package app.organicmaps.sdk.bookmarks.data;

import android.annotation.SuppressLint;
import android.os.Parcel;

import androidx.annotation.IntRange;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.os.ParcelCompat;

import app.organicmaps.sdk.Framework;
import app.organicmaps.sdk.routing.RoutePointInfo;
import app.organicmaps.sdk.search.Popularity;
import app.organicmaps.sdk.util.Constants;

// TODO consider refactoring to remove hack with MapObject unmarshalling itself and Bookmark at the same time.
// Used by JNI.
@Keep
@SuppressWarnings("unused")
@SuppressLint("ParcelCreator")
public class Bookmark extends MapObject
{
  private Icon mIcon;
  private long mCategoryId;
  private final long mBookmarkId;
  private final double mMerX;
  private final double mMerY;

  public Bookmark(@NonNull FeatureId featureId, @IntRange(from = 0) long categoryId,
                  @IntRange(from = 0) long bookmarkId, String title, @Nullable String secondaryTitle,
                  @Nullable String subtitle, @Nullable String address, @Nullable RoutePointInfo routePointInfo,
                  @OpeningMode int openingMode, @NonNull Popularity popularity, @NonNull String description,
                  @Nullable String[] rawTypes)
  {
    super(featureId, BOOKMARK, title, secondaryTitle, subtitle, address, 0, 0, "",
          routePointInfo, openingMode, popularity, description, RoadWarningMarkType.UNKNOWN.ordinal(), rawTypes);

    mCategoryId = categoryId;
    mBookmarkId = bookmarkId;
    mIcon = getIconInternal();

    final ParcelablePointD ll = BookmarkManager.INSTANCE.getBookmarkXY(mBookmarkId);
    mMerX = ll.x;
    mMerY = ll.y;

    initXY();
  }

  private void initXY()
  {
    setLat(Math.toDegrees(2.0 * Math.atan(Math.exp(Math.toRadians(mMerY))) - Math.PI / 2.0));
    setLon(mMerX);
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    super.writeToParcel(dest, flags);
    dest.writeLong(mCategoryId);
    dest.writeLong(mBookmarkId);
    dest.writeParcelable(mIcon, flags);
    dest.writeDouble(mMerX);
    dest.writeDouble(mMerY);
  }

  // Do not use Core while restoring from Parcel! In some cases this constructor is called before
  // the App is completely initialized.
  // TODO: Method restoreHasCurrentPermission causes this strange behaviour, needs to be investigated.
  protected Bookmark(@MapObjectType int type, Parcel source)
  {
    super(type, source);
    mCategoryId = source.readLong();
    mBookmarkId = source.readLong();
    mIcon = ParcelCompat.readParcelable(source, Icon.class.getClassLoader(), Icon.class);
    mMerX = source.readDouble();
    mMerY = source.readDouble();
    initXY();
  }

  @Override
  public double getScale()
  {
    return BookmarkManager.INSTANCE.getBookmarkScale(mBookmarkId);
  }

  public DistanceAndAzimut getDistanceAndAzimuth(double cLat, double cLon, double north)
  {
    return Framework.nativeGetDistanceAndAzimuth(mMerX, mMerY, cLat, cLon, north);
  }

  private Icon getIconInternal()
  {
    return new Icon(BookmarkManager.INSTANCE.getBookmarkColor(mBookmarkId),
                    BookmarkManager.INSTANCE.getBookmarkIcon(mBookmarkId));
  }

  @Nullable
  public Icon getIcon()
  {
    return mIcon;
  }

  public String getCategoryName()
  {
    return BookmarkManager.INSTANCE.getCategoryById(mCategoryId).getName();
  }

  public void setCategoryId(@IntRange(from = 0) long catId)
  {
    BookmarkManager.INSTANCE.notifyCategoryChanging(this, catId);
    mCategoryId = catId;
  }

  public void setParams(@NonNull String title, @Nullable Icon icon, @NonNull String description)
  {
    BookmarkManager.INSTANCE.notifyParametersUpdating(this, title, icon, description);
    if (icon != null)
      mIcon = icon;
    setTitle(title);
    setDescription(description);
  }

  public long getCategoryId()
  {
    return mCategoryId;
  }

  public long getBookmarkId()
  {
    return mBookmarkId;
  }

  @NonNull
  public String getBookmarkDescription()
  {
    return BookmarkManager.INSTANCE.getBookmarkDescription(mBookmarkId);
  }

  @NonNull
  public String getGe0Url(boolean addName)
  {
    return BookmarkManager.INSTANCE.encode2Ge0Url(mBookmarkId, addName);
  }

  @NonNull
  public String getHttpGe0Url(boolean addName)
  {
    return getGe0Url(addName).replaceFirst(Constants.Url.SHORT_SHARE_PREFIX, Constants.Url.HTTP_SHARE_PREFIX);
  }
}
