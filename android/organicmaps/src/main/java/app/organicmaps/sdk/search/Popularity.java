package app.organicmaps.sdk.search;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

// Called from JNI.
@Keep
@SuppressWarnings("unused")
public class Popularity implements Parcelable
{
  @NonNull
  private final Type mType;

  public Popularity(int popularity)
  {
    mType = Type.makeInstance(popularity);
  }

  @NonNull
  public Type getType()
  {
    return mType;
  }

  @NonNull
  public static Popularity defaultInstance()
  {
    return new Popularity(Type.NOT_POPULAR.ordinal());
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeInt(this.mType.ordinal());
  }

  protected Popularity(Parcel in)
  {
    int tmpMPopularity = in.readInt();
    this.mType = Type.values()[tmpMPopularity];
  }

  public static final Creator<Popularity> CREATOR = new Creator<>()
  {
    @Override
    public Popularity createFromParcel(Parcel source)
    {
      return new Popularity(source);
    }

    @Override
    public Popularity[] newArray(int size)
    {
      return new Popularity[size];
    }
  };

  public enum Type
  {
    NOT_POPULAR,
    POPULAR;

    @NonNull
    public static Type makeInstance(int index)
    {
      if (index < 0)
        throw new AssertionError("Incorrect negative index = " + index);

      return index > 0 ? POPULAR : NOT_POPULAR;
    }
  }
}
