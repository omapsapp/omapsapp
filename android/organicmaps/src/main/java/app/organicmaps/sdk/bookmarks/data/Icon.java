package app.organicmaps.sdk.bookmarks.data;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import com.google.common.base.Objects;

import app.organicmaps.sdk.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class Icon implements Parcelable
{
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ PREDEFINED_COLOR_NONE, PREDEFINED_COLOR_RED, PREDEFINED_COLOR_BLUE,
            PREDEFINED_COLOR_PURPLE, PREDEFINED_COLOR_YELLOW, PREDEFINED_COLOR_PINK,
            PREDEFINED_COLOR_BROWN, PREDEFINED_COLOR_GREEN, PREDEFINED_COLOR_ORANGE,
            PREDEFINED_COLOR_DEEPPURPLE, PREDEFINED_COLOR_LIGHTBLUE, PREDEFINED_COLOR_CYAN,
            PREDEFINED_COLOR_TEAL, PREDEFINED_COLOR_LIME, PREDEFINED_COLOR_DEEPORANGE,
            PREDEFINED_COLOR_GRAY, PREDEFINED_COLOR_BLUEGRAY})
  @interface PredefinedColor {}

  static final int PREDEFINED_COLOR_NONE = 0;
  static final int PREDEFINED_COLOR_RED = 1;
  static final int PREDEFINED_COLOR_BLUE = 2;
  static final int PREDEFINED_COLOR_PURPLE = 3;
  static final int PREDEFINED_COLOR_YELLOW = 4;
  static final int PREDEFINED_COLOR_PINK = 5;
  static final int PREDEFINED_COLOR_BROWN = 6;
  static final int PREDEFINED_COLOR_GREEN = 7;
  static final int PREDEFINED_COLOR_ORANGE = 8;
  static final int PREDEFINED_COLOR_DEEPPURPLE = 9;
  static final int PREDEFINED_COLOR_LIGHTBLUE = 10;
  static final int PREDEFINED_COLOR_CYAN = 11;
  static final int PREDEFINED_COLOR_TEAL = 12;
  static final int PREDEFINED_COLOR_LIME = 13;
  static final int PREDEFINED_COLOR_DEEPORANGE = 14;
  static final int PREDEFINED_COLOR_GRAY = 15;
  static final int PREDEFINED_COLOR_BLUEGRAY = 16;

  private static final String[] PREDEFINED_COLOR_NAMES = { "placemark-red",
                                                           "placemark-red",
                                                           "placemark-blue",
                                                           "placemark-purple",
                                                           "placemark-yellow",
                                                           "placemark-pink",
                                                           "placemark-brown",
                                                           "placemark-green",
                                                           "placemark-orange",
                                                           "placemark-deeppurple",
                                                           "placemark-lightblue",
                                                           "placemark-cyan",
                                                           "placemark-teal",
                                                           "placemark-lime",
                                                           "placemark-deeporange",
                                                           "placemark-gray",
                                                           "placemark-bluegray" };

  private static int shift(int v, int bitCount) { return v << bitCount; }
  private static int toARGB(int r, int g, int b)
  {
    return shift(255, 24) + shift(r, 16) + shift(g, 8) + b;
  }

  private static final int[] ARGB_COLORS = { toARGB(229, 27, 35), // none
                                             toARGB(229, 27, 35), // red
                                             toARGB(0, 110, 199), // blue
                                             toARGB(156, 39, 176), // purple
                                             toARGB(255, 200, 0), // yellow
                                             toARGB(255, 65, 130), // pink
                                             toARGB(121, 85, 72), // brown
                                             toARGB(56, 142, 60), // green
                                             toARGB(255, 160, 0), // orange
                                             toARGB(102, 57, 191), // deeppurple
                                             toARGB(36, 156, 242), // lightblue
                                             toARGB(20, 190, 205), // cyan
                                             toARGB(0, 165, 140), // teal
                                             toARGB(147, 191, 57), // lime
                                             toARGB(240, 100, 50), // deeporange
                                             toARGB(115, 115, 115), // gray
                                             toARGB(89, 115, 128) }; // bluegray

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ BOOKMARK_ICON_TYPE_NONE,
            BOOKMARK_ICON_TYPE_HOTEL,
            BOOKMARK_ICON_TYPE_ANIMALS,
            BOOKMARK_ICON_TYPE_BUDDHISM,
            BOOKMARK_ICON_TYPE_BUILDING,
            BOOKMARK_ICON_TYPE_CHRISTIANITY,
            BOOKMARK_ICON_TYPE_ENTERTAINMENT,
            BOOKMARK_ICON_TYPE_EXCHANGE,
            BOOKMARK_ICON_TYPE_FOOD,
            BOOKMARK_ICON_TYPE_GAS,
            BOOKMARK_ICON_TYPE_JUDAISM,
            BOOKMARK_ICON_TYPE_MEDICINE,
            BOOKMARK_ICON_TYPE_MOUNTAIN,
            BOOKMARK_ICON_TYPE_MUSEUM,
            BOOKMARK_ICON_TYPE_ISLAM,
            BOOKMARK_ICON_TYPE_PARK,
            BOOKMARK_ICON_TYPE_PARKING,
            BOOKMARK_ICON_TYPE_SHOP,
            BOOKMARK_ICON_TYPE_SIGHTS,
            BOOKMARK_ICON_TYPE_SWIM,
            BOOKMARK_ICON_TYPE_WATER,
            BOOKMARK_ICON_TYPE_BAR,
            BOOKMARK_ICON_TYPE_TRANSPORT,
            BOOKMARK_ICON_TYPE_VIEWPOINT,
            BOOKMARK_ICON_TYPE_SPORT,
            BOOKMARK_ICON_TYPE_START,
            BOOKMARK_ICON_TYPE_FINISH })
  @interface BookmarkIconType {}

  static final int BOOKMARK_ICON_TYPE_NONE = 0;
  static final int BOOKMARK_ICON_TYPE_HOTEL = 1;
  static final int BOOKMARK_ICON_TYPE_ANIMALS = 2;
  static final int BOOKMARK_ICON_TYPE_BUDDHISM = 3;
  static final int BOOKMARK_ICON_TYPE_BUILDING = 4;
  static final int BOOKMARK_ICON_TYPE_CHRISTIANITY = 5;
  static final int BOOKMARK_ICON_TYPE_ENTERTAINMENT = 6;
  static final int BOOKMARK_ICON_TYPE_EXCHANGE = 7;
  static final int BOOKMARK_ICON_TYPE_FOOD = 8;
  static final int BOOKMARK_ICON_TYPE_GAS = 9;
  static final int BOOKMARK_ICON_TYPE_JUDAISM = 10;
  static final int BOOKMARK_ICON_TYPE_MEDICINE = 11;
  static final int BOOKMARK_ICON_TYPE_MOUNTAIN = 12;
  static final int BOOKMARK_ICON_TYPE_MUSEUM = 13;
  static final int BOOKMARK_ICON_TYPE_ISLAM = 14;
  static final int BOOKMARK_ICON_TYPE_PARK = 15;
  static final int BOOKMARK_ICON_TYPE_PARKING = 16;
  static final int BOOKMARK_ICON_TYPE_SHOP = 17;
  static final int BOOKMARK_ICON_TYPE_SIGHTS = 18;
  static final int BOOKMARK_ICON_TYPE_SWIM = 19;
  static final int BOOKMARK_ICON_TYPE_WATER = 20;
  static final int BOOKMARK_ICON_TYPE_BAR = 21;
  static final int BOOKMARK_ICON_TYPE_TRANSPORT = 22;
  static final int BOOKMARK_ICON_TYPE_VIEWPOINT = 23;
  static final int BOOKMARK_ICON_TYPE_SPORT = 24;
  static final int BOOKMARK_ICON_TYPE_START = 25;
  static final int BOOKMARK_ICON_TYPE_FINISH = 26;

  @DrawableRes
  private static final int[] TYPE_ICONS = { R.drawable.ic_bookmark_none,
                                            R.drawable.ic_bookmark_hotel,
                                            R.drawable.ic_bookmark_animals,
                                            R.drawable.ic_bookmark_buddhism,
                                            R.drawable.ic_bookmark_building,
                                            R.drawable.ic_bookmark_christianity,
                                            R.drawable.ic_bookmark_entertainment,
                                            R.drawable.ic_bookmark_money,
                                            R.drawable.ic_bookmark_food,
                                            R.drawable.ic_bookmark_gas,
                                            R.drawable.ic_bookmark_judaism,
                                            R.drawable.ic_bookmark_medicine,
                                            R.drawable.ic_bookmark_mountain,
                                            R.drawable.ic_bookmark_museum,
                                            R.drawable.ic_bookmark_islam,
                                            R.drawable.ic_bookmark_park,
                                            R.drawable.ic_bookmark_parking,
                                            R.drawable.ic_bookmark_shop,
                                            R.drawable.ic_bookmark_sights,
                                            R.drawable.ic_bookmark_swim,
                                            R.drawable.ic_bookmark_water,
                                            R.drawable.ic_bookmark_bar,
                                            R.drawable.ic_bookmark_transport,
                                            R.drawable.ic_bookmark_viewpoint,
                                            R.drawable.ic_bookmark_sport,
                                            R.drawable.ic_bookmark_start,
                                            R.drawable.ic_bookmark_finish };

  @PredefinedColor
  private final int mColor;
  @BookmarkIconType
  private final int mType;

  public Icon(@PredefinedColor int color, @BookmarkIconType int type)
  {
    mColor = color;
    mType = type;
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeInt(mColor);
    dest.writeInt(mType);
  }

  private Icon(Parcel in)
  {
    mColor = in.readInt();
    mType = in.readInt();
  }

  @PredefinedColor
  public int getColor()
  {
    return mColor;
  }

  @NonNull
  public String getName()
  {
    return PREDEFINED_COLOR_NAMES[mColor];
  }

  public int argb()
  {
    return ARGB_COLORS[mColor];
  }

  @BookmarkIconType
  public int getType()
  {
    return mType;
  }

  @DrawableRes
  public int getResId()
  {
    return TYPE_ICONS[mType];
  }

  @Override
  public boolean equals(Object o)
  {
    if (this == o)
      return true;
    if (o instanceof Icon comparedIcon)
      return mColor == comparedIcon.mColor && mType == comparedIcon.mType;
    return false;
  }

  @Override
  public int hashCode()
  {
    return Objects.hashCode(mColor, mType);
  }

  public static final Parcelable.Creator<Icon> CREATOR = new Parcelable.Creator<>()
  {
    public Icon createFromParcel(Parcel in)
    {
      return new Icon(in);
    }

    public Icon[] newArray(int size)
    {
      return new Icon[size];
    }
  };
}
