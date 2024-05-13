package app.organicmaps.sdk.search;

import androidx.annotation.NonNull;

public class DisplayedCategories
{
  @NonNull
  public static String[] getKeys()
  {
    return nativeGetKeys();
  }

  @NonNull
  private static native String[] nativeGetKeys();
}
