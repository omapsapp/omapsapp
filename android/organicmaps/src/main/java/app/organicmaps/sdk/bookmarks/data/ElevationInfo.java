package app.organicmaps.sdk.bookmarks.data;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import app.organicmaps.widget.placepage.PlacePageData;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

// Used by JNI.
@Keep
@SuppressWarnings("unused")
public class ElevationInfo implements PlacePageData
{
  private final long mId;
  @NonNull
  private final String mName;
  @NonNull
  private final List<Point> mPoints;
  private final int mAscent;
  private final int mDescent;
  private final int mMinAltitude;
  private final int mMaxAltitude;
  private final int mDifficulty;
  private final long mDuration;

  public ElevationInfo(long trackId, @NonNull String name,
                       @NonNull Point[] points, int ascent, int descent, int minAltitude,
                       int maxAltitude, int difficulty, long duration)
  {
    mId = trackId;
    mName = name;
    mPoints = Arrays.asList(points);
    mAscent = ascent;
    mDescent = descent;
    mMinAltitude = minAltitude;
    mMaxAltitude = maxAltitude;
    mDifficulty = difficulty;
    mDuration = duration;
  }

  protected ElevationInfo(Parcel in)
  {
    mId = in.readLong();
    mName = in.readString();
    mAscent = in.readInt();
    mDescent = in.readInt();
    mMinAltitude = in.readInt();
    mMaxAltitude = in.readInt();
    mDifficulty = in.readInt();
    mDuration = in.readLong();
    mPoints = readPoints(in);
  }

  @NonNull
  private static List<Point> readPoints(@NonNull Parcel in)
  {
    List<Point> points = new ArrayList<>();
    in.readTypedList(points, Point.CREATOR);
    return points;
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

  @NonNull
  public List<Point> getPoints()
  {
    return Collections.unmodifiableList(mPoints);
  }

  public int getAscent()
  {
    return mAscent;
  }

  public int getDescent()
  {
    return mDescent;
  }

  public int getMinAltitude()
  {
    return mMinAltitude;
  }

  public int getMaxAltitude()
  {
    return mMaxAltitude;
  }

  public int getDifficulty()
  {
    return mDifficulty;
  }

  public long getDuration()
  {
    return mDuration;
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeLong(mId);
    dest.writeString(mName);
    dest.writeInt(mAscent);
    dest.writeInt(mDescent);
    dest.writeInt(mMinAltitude);
    dest.writeInt(mMaxAltitude);
    dest.writeInt(mDifficulty);
    dest.writeLong(mDuration);
    // All collections are deserialized AFTER non-collection and primitive type objects,
    // so collections must be always serialized at the end.
    dest.writeTypedList(mPoints);
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  public static class Point implements Parcelable
  {
    private final double mDistance;
    private final int mAltitude;

    public Point(double distance, int altitude)
    {
      mDistance = distance;
      mAltitude = altitude;
    }

    protected Point(Parcel in)
    {
      mDistance = in.readDouble();
      mAltitude = in.readInt();
    }

    public static final Creator<Point> CREATOR = new Creator<>()
    {
      @Override
      public Point createFromParcel(Parcel in)
      {
        return new Point(in);
      }

      @Override
      public Point[] newArray(int size)
      {
        return new Point[size];
      }
    };

    public double getDistance()
    {
      return mDistance;
    }

    public int getAltitude()
    {
      return mAltitude;
    }

    @Override
    public int describeContents()
    {
      return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags)
    {
      dest.writeDouble(mDistance);
      dest.writeInt(mAltitude);
    }
  }

  public static final Creator<ElevationInfo> CREATOR = new Creator<>()
  {
    @Override
    public ElevationInfo createFromParcel(Parcel in)
    {
      return new ElevationInfo(in);
    }

    @Override
    public ElevationInfo[] newArray(int size)
    {
      return new ElevationInfo[size];
    }
  };
}
