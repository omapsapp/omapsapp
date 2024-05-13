package app.organicmaps.sdk.editor.data;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

public class FeatureCategory implements Parcelable
{
  @NonNull
  private final String mType;

  @NonNull
  private final String mLocalizedTypeName;

  public FeatureCategory(@NonNull String type, @NonNull String localizedTypeName)
  {
    mType = type;
    mLocalizedTypeName = localizedTypeName;
  }

  private FeatureCategory(Parcel source)
  {
    mType = source.readString();
    mLocalizedTypeName = source.readString();
  }

  @NonNull
  public String getType()
  {
    return mType;
  }

  @NonNull
  public String getLocalizedTypeName()
  {
    return mLocalizedTypeName;
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeString(mType);
    dest.writeString(mLocalizedTypeName);
  }

  public static final Creator<FeatureCategory> CREATOR = new Creator<>()
  {
    @Override
    public FeatureCategory createFromParcel(Parcel source)
    {
      return new FeatureCategory(source);
    }

    @Override
    public FeatureCategory[] newArray(int size)
    {
      return new FeatureCategory[size];
    }
  };
}
