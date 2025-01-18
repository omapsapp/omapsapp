package app.organicmaps.sdk.routing;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

// Called from JNI.
@Keep
@SuppressWarnings("unused")
public final class RoutePointInfo implements Parcelable
{
  public static final Creator<RoutePointInfo> CREATOR = new Creator<>()
  {
    @Override
    public RoutePointInfo createFromParcel(Parcel in)
    {
      return new RoutePointInfo(in);
    }

    @Override
    public RoutePointInfo[] newArray(int size)
    {
      return new RoutePointInfo[size];
    }
  };

  public final RouteMarkType mMarkType;

  public final int mIntermediateIndex;

  private RoutePointInfo(@NonNull RouteMarkType markType, int intermediateIndex)
  {
    mMarkType = markType;
    mIntermediateIndex = intermediateIndex;
  }

  private RoutePointInfo(@NonNull Parcel in)
  {
    //noinspection WrongConstant
    this(RouteMarkType.values()[in.readInt()] /* mMarkType */, in.readInt() /* mIntermediateIndex */);
  }

  boolean isIntermediatePoint()
  {
    return mMarkType == RouteMarkType.Intermediate;
  }

  boolean isFinishPoint()
  {
    return mMarkType == RouteMarkType.Finish;
  }

  boolean isStartPoint()
  {
    return mMarkType == RouteMarkType.Start;
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeInt(mMarkType.ordinal());
    dest.writeInt(mIntermediateIndex);
  }
}
