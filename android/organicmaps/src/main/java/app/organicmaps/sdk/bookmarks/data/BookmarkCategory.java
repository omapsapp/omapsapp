package app.organicmaps.sdk.bookmarks.data;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.DrawableRes;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;

import app.organicmaps.R;

// Used by JNI.
@Keep
@SuppressWarnings("unused")
public class BookmarkCategory implements Parcelable
{
  private final long mId;
  @NonNull
  private final String mName;
  @NonNull
  private final String mAnnotation;
  @NonNull
  private final String mDescription;
  private final int mTracksCount;
  private final int mBookmarksCount;
  private boolean mIsVisible;

  public BookmarkCategory(long id, @NonNull String name, @NonNull String annotation,
                          @NonNull String description, int tracksCount, int bookmarksCount,
                           boolean isVisible)
  {
    mId = id;
    mName = name;
    mAnnotation = annotation;
    mDescription = description;
    mTracksCount = tracksCount;
    mBookmarksCount = bookmarksCount;
    mIsVisible = isVisible;
  }

  @Override
  public boolean equals(Object o)
  {
    if (this == o) return true;
    if (o == null || getClass() != o.getClass()) return false;
    BookmarkCategory that = (BookmarkCategory) o;
    return mId == that.mId;
  }

  @Override
  public int hashCode()
  {
    return (int)(mId ^ (mId >>> 32));
  }

  public long getId()
  {
    return mId;
  }

  @NonNull
  public String getName()
  {
    return mName;
  }

  public int getTracksCount()
  {
    return mTracksCount;
  }

  public int getBookmarksCount()
  {
    return mBookmarksCount;
  }

  public boolean isVisible()
  {
    return mIsVisible;
  }

  public void setVisible(boolean isVisible)
  {
    mIsVisible = isVisible;
  }

  public int size()
  {
    return getBookmarksCount() + getTracksCount();
  }

  @NonNull
  public String getAnnotation()
  {
    return mAnnotation;
  }

  @NonNull
  public String getDescription()
  {
    return mDescription;
  }

  @Override
  public String toString()
  {
    final StringBuilder sb = new StringBuilder("BookmarkCategory{");
    sb.append("mId=").append(mId);
    sb.append(", mName='").append(mName).append('\'');
    sb.append(", mAnnotation='").append(mAnnotation).append('\'');
    sb.append(", mDescription='").append(mDescription).append('\'');
    sb.append(", mTracksCount=").append(mTracksCount);
    sb.append(", mBookmarksCount=").append(mBookmarksCount);
    sb.append(", mIsVisible=").append(mIsVisible);

    sb.append('}');
    return sb.toString();
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeLong(this.mId);
    dest.writeString(this.mName);
    dest.writeString(this.mAnnotation);
    dest.writeString(this.mDescription);
    dest.writeInt(this.mTracksCount);
    dest.writeInt(this.mBookmarksCount);
    dest.writeByte(this.mIsVisible ? (byte) 1 : (byte) 0);
  }

  protected BookmarkCategory(Parcel in)
  {
    this.mId = in.readLong();
    this.mName = in.readString();
    this.mAnnotation = in.readString();
    this.mDescription = in.readString();
    this.mTracksCount = in.readInt();
    this.mBookmarksCount = in.readInt();
    this.mIsVisible = in.readByte() != 0;
  }

  public static final Creator<BookmarkCategory> CREATOR = new Creator<>()
  {
    @Override
    public BookmarkCategory createFromParcel(Parcel source)
    {
      return new BookmarkCategory(source);
    }

    @Override
    public BookmarkCategory[] newArray(int size)
    {
      return new BookmarkCategory[size];
    }
  };

  public enum AccessRules
  {
    ACCESS_RULES_LOCAL(R.string.not_shared, R.drawable.ic_lock),
    ACCESS_RULES_PUBLIC(R.string.public_access, R.drawable.ic_public_inline),
    ACCESS_RULES_DIRECT_LINK(R.string.limited_access, R.drawable.ic_link_inline),
    ACCESS_RULES_AUTHOR_ONLY(R.string.access_rules_author_only, R.drawable.ic_lock);

    private final int mResId;
    private final int mDrawableResId;

    AccessRules(int resId, int drawableResId)
    {
      mResId = resId;
      mDrawableResId = drawableResId;
    }

    @DrawableRes
    public int getDrawableResId()
    {
      return mDrawableResId;
    }

    @StringRes
    public int getNameResId()
    {
      return mResId;
    }
  }
}
