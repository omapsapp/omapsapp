package app.organicmaps.sdk.editor.data;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.IntRange;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import app.organicmaps.sdk.util.StringUtils;

import java.time.LocalTime;
import java.time.format.DateTimeFormatter;

// Called from JNI.
@Keep
@SuppressWarnings("unused")
public class HoursMinutes implements Parcelable
{
  public final long hours;
  public final long minutes;
  private final boolean m24HourFormat;

  // 24 hours or even 25 and higher values are used in OSM data and passed here from JNI calls.
  // Example: 18:00-24:00
  public HoursMinutes(@IntRange(from = 0, to = 24) long hours,
                      @IntRange(from = 0, to = 59) long minutes, boolean is24HourFormat)
  {
    this.hours = hours;
    this.minutes = minutes;
    m24HourFormat = is24HourFormat;
  }

  protected HoursMinutes(Parcel in)
  {
    hours = in.readLong();
    minutes = in.readLong();
    m24HourFormat = in.readByte() != 0;
  }

  @NonNull
  @Override
  public String toString()
  {
    if (m24HourFormat)
      return StringUtils.formatUsingUsLocale("%02d:%02d", hours, minutes);

    // Formatting a string here with hours outside of 0-23 range causes DateTimeException.
    final LocalTime localTime = LocalTime.of((int) hours % 24, (int) minutes);
    return localTime.format(DateTimeFormatter.ofPattern("hh:mm a"));
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeLong(hours);
    dest.writeLong(minutes);
    dest.writeByte((byte) (m24HourFormat ? 1 : 0));
  }

  public static final Creator<HoursMinutes> CREATOR = new Creator<>()
  {
    @Override
    public HoursMinutes createFromParcel(Parcel in)
    {
      return new HoursMinutes(in);
    }

    @Override
    public HoursMinutes[] newArray(int size)
    {
      return new HoursMinutes[size];
    }
  };
}
