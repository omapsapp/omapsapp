package app.organicmaps.bookmarks.data;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.IntRange;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.HashMap;
import java.util.Map;

public class Metadata implements Parcelable
{
  // Values must correspond to the Metadata definition from indexer/feature_meta.hpp.
  public enum MetadataType
  {
    // Defined by classifier types now.
    FMD_CUISINE(1),
    FMD_OPEN_HOURS(2),
    FMD_PHONE_NUMBER(3),
    FMD_FAX_NUMBER(4),
    FMD_STARS(5),
    FMD_OPERATOR(6),
    // Removed and is not used in the core. Use FMD_WEBSITE instead.
    //FMD_URL(7),
    FMD_WEBSITE(8),
    FMD_INTERNET(9),
    FMD_ELE(10),
    FMD_TURN_LANES(11),
    FMD_TURN_LANES_FORWARD(12),
    FMD_TURN_LANES_BACKWARD(13),
    FMD_EMAIL(14),
    FMD_POSTCODE(15),
    // TODO: It is hacked in jni and returns full Wikipedia url. Should use separate getter instead.
    FMD_WIKIPEDIA(16),
    // TODO: Skipped now.
    FMD_DESCRIPTION(17),
    FMD_FLATS(18),
    FMD_HEIGHT(19),
    FMD_MIN_HEIGHT(20),
    FMD_DENOMINATION(21),
    FMD_BUILDING_LEVELS(22),
    FWD_TEST_ID(23),
    FMD_CUSTOM_IDS(24),
    FMD_PRICE_RATES(25),
    FMD_RATINGS(26),
    FMD_EXTERNAL_URI(27),
    FMD_LEVEL(28),
    FMD_AIRPORT_IATA(29),
    FMD_BRAND(30),
    FMD_DURATION(31),
    FMD_CONTACT_FACEBOOK(32),
    FMD_CONTACT_INSTAGRAM(33),
    FMD_CONTACT_TWITTER(34),
    FMD_CONTACT_VK(35),
    FMD_CONTACT_LINE(36),
    FMD_DESTINATION(37),
    FMD_DESTINATION_REF(38),
    FMD_JUNCTION_REF(39),
    FMD_BUILDING_MIN_LEVEL(40),
    FMD_WIKIMEDIA_COMMONS(41),
    FMD_CAPACITY(42),
    FMD_WHEELCHAIR(43),
    FMD_LOCAL_REF(44),
    FMD_DRIVE_THROUGH(45),
    FMD_WEBSITE_MENU(46),
    FMD_COLLECTION_TIMES(47);
    private final int mMetaType;

    MetadataType(int metadataType)
    {
      mMetaType = metadataType;
    }

    @NonNull
    public static MetadataType fromInt(@IntRange(from = 1, to = 41) int metaType)
    {
      for (MetadataType type : values())
        if (type.mMetaType == metaType)
          return type;

      throw new IllegalArgumentException("Illegal metaType: " + metaType);
    }

    public int toInt()
    {
      return mMetaType;
    }
  }

  private final Map<MetadataType, String> mMetadataMap = new HashMap<>();

  public void addMetadata(int metaType, String metaValue)
  {
    final MetadataType type = MetadataType.fromInt(metaType);
    mMetadataMap.put(type, metaValue);
  }

  @Nullable
  String getMetadata(MetadataType type)
  {
    return mMetadataMap.get(type);
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeInt(mMetadataMap.size());
    for (Map.Entry<MetadataType, String> metaEntry : mMetadataMap.entrySet())
    {
      dest.writeInt(metaEntry.getKey().mMetaType);
      dest.writeString(metaEntry.getValue());
    }
  }

  public static Metadata readFromParcel(Parcel source)
  {
    final Metadata metadata = new Metadata();
    final int size = source.readInt();
    for (int i = 0; i < size; i++)
      metadata.addMetadata(source.readInt(), source.readString());
    return metadata;
  }

  public static final Creator<Metadata> CREATOR = new Creator<>()
  {
    @Override
    public Metadata createFromParcel(Parcel source)
    {
      return readFromParcel(source);
    }

    @Override
    public Metadata[] newArray(int size)
    {
      return new Metadata[size];
    }
  };
}
