package app.organicmaps.sdk.bookmarks.data;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.Nullable;

import java.net.HttpURLConnection;

public class Error implements Parcelable
{
  private final int mHttpCode;
  @Nullable
  private final String mMessage;

  public Error(int httpCode, @Nullable String message)
  {
    mHttpCode = httpCode;
    mMessage = message;
  }

  public Error(@Nullable String message)
  {
    this(HttpURLConnection.HTTP_UNAVAILABLE, message);
  }

  protected Error(Parcel in)
  {
    mHttpCode = in.readInt();
    mMessage = in.readString();
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeInt(mHttpCode);
    dest.writeString(mMessage);
  }

  @Override
  public String toString()
  {
    return "Error{" +
           "mHttpCode=" + mHttpCode +
           ", mMessage='" + mMessage + '\'' +
           '}';
  }

  public static final Creator<Error> CREATOR = new Creator<>()
  {
    @Override
    public Error createFromParcel(Parcel in)
    {
      return new Error(in);
    }

    @Override
    public Error[] newArray(int size)
    {
      return new Error[size];
    }
  };
}
