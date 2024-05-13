package app.organicmaps.sdk.routing;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.IntDef;
import androidx.annotation.Keep;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// Called from JNI.
@Keep
@SuppressWarnings("unused")
public class RoutePointInfo implements Parcelable
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

  public static final int ROUTE_MARK_START = 0;
  public static final int ROUTE_MARK_INTERMEDIATE = 1;
  public static final int ROUTE_MARK_FINISH = 2;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ ROUTE_MARK_START, ROUTE_MARK_INTERMEDIATE, ROUTE_MARK_FINISH })
  public @interface RouteMarkType {}

  @RouteMarkType
  public final int mMarkType;

  public final int mIntermediateIndex;

  public RoutePointInfo(@RouteMarkType int markType, int intermediateIndex)
  {
    mMarkType = markType;
    mIntermediateIndex = intermediateIndex;
  }

  private RoutePointInfo(Parcel in)
  {
    //noinspection WrongConstant
    this(in.readInt() /* mMarkType */, in.readInt() /* mIntermediateIndex */);
  }

  boolean isIntermediatePoint()
  {
    return mMarkType == ROUTE_MARK_INTERMEDIATE;
  }

  boolean isFinishPoint()
  {
    return mMarkType == ROUTE_MARK_FINISH;
  }

  boolean isStartPoint()
  {
    return mMarkType == ROUTE_MARK_START;
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeInt(mMarkType);
    dest.writeInt(mIntermediateIndex);
  }
}
